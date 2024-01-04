import os
import argparse
from collections import deque
import traceback
import tkinter as tk
import numpy as np
from tkinter import font, PhotoImage
from scipy.interpolate import interp1d
from scipy import signal
import threading
import queue
import time
from chord import compute_chords, stream, ChordAlgo

def data_path(p):
    basedir = os.path.dirname(__file__)
    return os.path.join(p)

class App(tk.Tk):
    def __init__(self, chunk_queue, chord_queue, tick=500, max_frames=10*2048) -> None:
        super().__init__()

        self.tick = tick
        self.chunk_queue = chunk_queue
        self.chord_queue = chord_queue
        self.max_frames = max_frames
        
        self.exited = False
        self.protocol("WM_DELETE_WINDOW", self.exit)

        self.init_gui()
        self.main()

    def exit(self):
        self.exited = True
        self.eval("::tk::CancelRepeat")
        self.destroy()

    def init_gui(self):
        self.title("Chordy")
        
        self.geometry('700x350')
        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(0, weight=1)
        self.grid_rowconfigure(1, weight=0)

        self.frame = tk.Frame(self, background="black")
        self.frame.grid(row=0, column=0, sticky="ewns")

        self.frame.grid_anchor("center")

        self.info_frame = tk.Frame(self.frame, background="black")
        self.info_frame.grid(row=0, column=0, sticky="")

        self.default_font = "Arial Rounded Mt Bold"
        self.chord_var = tk.StringVar(value="None")        
        self.chord_label = tk.Label(self.info_frame, fg="white", bg="black", padx=5, font=(self.default_font, 36), textvariable=self.chord_var)
        self.chord_label.grid(row=0, column=0, sticky="ew")

        self.prob_var = tk.StringVar(value="0.00%")
        self.prob_label = tk.Label(self.info_frame, fg="white", bg="black", padx=5, font=(self.default_font, 12), textvariable=self.prob_var)
        self.prob_label.grid(row=1, column=0, sticky="ew")

        self.canvas = tk.Canvas(self, background="black", highlightthickness=0)
        self.canvas.grid(row=1, column=0, sticky="nsew")
        self.polyline_id = self.canvas.create_line((0, 0, 0, 0), fill="white")

    def plot_waveform(self, frames):
        self.update()
        width, height = self.canvas.winfo_width(), self.canvas.winfo_height()

        ypx = (frames*-1+1)*height/2
        xpx = np.arange(frames.shape[0])*1.*width/self.max_frames
        pts = np.column_stack([xpx, ypx])

        # timeseries resampling: 75ms -> 7ms (display cost)
        x = np.arange(0, np.min([np.max(xpx), width]), 0.25) # 0.5px prevents wobbly waveform look
        # y = signal.resample(pts[:,1], width) 
        y = interp1d(pts[:,0], pts[:,1], fill_value=(float(height)/2, float(height)/2), bounds_error=False)(x)
        pts = np.column_stack([x, y])
        
        self.canvas.coords(self.polyline_id, list(np.ravel(pts)))

    def main(self):
        def poll():   
            try:
                if len(self.chord_queue) > 0:
                    # flush + get latest
                    data = self.chord_queue[-1]
                    self.chord_queue.clear()

                    # update gui
                    chord, prob = data
                    
                    if chord is None: chord = "N/A"
                    if prob is None or np.isnan(prob): prob = 0

                    self.chord_var.set(chord)
                    self.prob_var.set("{:.2f}%".format(prob*100))
                
                if len(self.chunk_queue) > 0:
                    # TODO: use deque / np.array views for better solution to concatenation. main factor to fps
                    # concatenate up to max
                    frames = np.array([])
                    i = len(self.chunk_queue)-1
                    while i >= 0:
                        _frames = np.append(self.chunk_queue[i], frames)
                        if _frames.shape[0] > self.max_frames:
                            break
                        else:
                            frames = _frames
                        
                        i -= 1

                    self.plot_waveform(frames)
            except Exception as e:
                if self.exited: return
                print(traceback.format_exc())

            self.after(self.tick, poll)
        
        poll()

def start_stream(q, max_chunks=100, **kwargs):
    def handler(chunk):
        q.append(chunk) # put frames into compute queue
        if len(q) > max_chunks: q.popleft()

    stream(handler, **kwargs) # avg 0.01s btwn chunks for size = 1024

def start_compute(chunk_q, chord_q, poll_delay=0, max_frames=1e99, **kwargs):
    while True:
        if len(chunk_q) > 0:
            # concatenate up to max
            frames = np.array([])
            i = len(chunk_q)-1
            while i >= 0:
                _frames = np.append(chunk_q[i], frames)
                if _frames.shape[0] > max_frames:
                    break
                else:
                    frames = _frames
                
                i -= 1
            
            # process and send to queue
            chord, prob = compute_chords(frames, **kwargs) # avg: 0.3s for 4*2048 frames
            chord_q.append([chord, prob])
        else:
            time.sleep(poll_delay) # delay iff chunk was empty

def main(args):
    chunk_queue = deque([]) # audio (write), compute + gui (read only)
    chord_queue = [] # audio (none), compute (write), gui (read only)

    audio_thread = threading.Thread(
        target=start_stream, 
        args=(chunk_queue, ),
        kwargs={'max_chunks': args.display_chunks, 'chunk': args.chunk_size, 'fs': args.fs}
    )
    compute_thread = threading.Thread(
        target=start_compute,
        args=(chunk_queue, chord_queue,), 
        kwargs={'poll_delay': 0.00, 'fs': args.fs, 'max_frames': args.chord_chunks*args.chunk_size, 'iBlockLength':args.block_chunks*args.chunk_size, 'iHopLength':args.hop_chunks*args.chunk_size, 'algorithm': args.algorithm, 'threshold': args.threshold}
    )

    gui = App(chunk_queue, chord_queue, tick=1, max_frames=args.display_chunks*args.chunk_size)

    audio_thread.daemon = True
    audio_thread.start()

    compute_thread.daemon = True
    compute_thread.start()

    gui.mainloop()
    
if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        prog='chordy', 
        usage="%(prog)s [options] [audio] [gui] [algo]",
        description="A configurable, multi-threaded, real-time chord detector!",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )

    audio = parser.add_argument_group('audio')
    audio.add_argument('--sample-rate', '-fs', dest='fs', type=int, default=44100, help="Audio sample rate in frames per second")
    audio.add_argument('--chunk-size', '-cs', dest='chunk_size', type=int, default=1024, help="Number of samples per streamed chunk")

    gui = parser.add_argument_group('gui')
    gui.add_argument('--display-chunks', '-d', dest='display_chunks', type=int, default=200, help="Number of chunks to display in viewer waveform")

    algo = parser.add_argument_group('algo')
    algo.add_argument('--chord-chunks', '-cc', dest="chord_chunks", type=int, default=8, help="Number of chunks to send to chord recognition algorithm")
    algo.add_argument('--hop-chunks', '-hc', dest='hop_chunks', type=int, default=2, help="Number of chunks to hop")
    algo.add_argument('--block-chunks', '-bc', dest='block_chunks', type=int, default=2, help="Number of chunks per block")
    algo.add_argument('--algorithm', '-a', type=ChordAlgo.from_str, dest="algorithm", choices=list(ChordAlgo), default=ChordAlgo.RAW, help="Choice of recognition algorithm")
    algo.add_argument('--raw', dest="algorithm", action='store_const', const=ChordAlgo.RAW, help="Use raw chords and probabilities")
    algo.add_argument('--viterbi', dest="algorithm", action='store_const', const=ChordAlgo.VITERBI, help="Use Markov Chains / Viterbi Algorithm to process raw chord probabilities")
    algo.add_argument('--threshold', '-t', dest="threshold", type=float, default=0.07, help="Minimum probability for a detected chord")

    args = parser.parse_args()

    main(args)
    