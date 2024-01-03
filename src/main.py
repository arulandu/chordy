from collections import deque
import tkinter as tk
import numpy as np
from tkinter import font
from scipy.interpolate import interp1d
from scipy import signal
import threading
import queue
import time
from chord import compute_chords, stream, ChordAlgo
"""arial rounded mt bold gm7#
yuppy sc gm7#
yugothic
Baloo 2
"""

class App(tk.Tk):
    def __init__(self, chunk_queue, chord_queue, tick=500, max_frames=10*2048) -> None:
        super().__init__()

        self.tick = tick
        self.chunk_queue = chunk_queue
        self.chord_queue = chord_queue
        self.max_frames = max_frames
        
        self.init_gui()
        self.main()

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

        t0 = time.time()
        ypx = (frames*-1+1)*height/2
        xpx = np.arange(frames.shape[0])*1.*width/self.max_frames
        pts = np.column_stack([xpx, ypx])

        # TODO: implement better poly smoothing here
        # f = interp1d(pts[:,0], pts[:,1], fill_value='extrapolate')
        # x = np.arange(0, width)
        # y = signal.resample(pts[:,1], width)
        # print(y.shape, np.max(y), np.min(y))
        # pts = np.column_stack([x, y])
        
        self.canvas.coords(self.polyline_id, list(np.ravel(pts)))
        print(time.time()-t0)

    def main(self):
        def poll():            
            if len(self.chord_queue) > 0:
                # flush + get latest
                data = self.chord_queue[-1]
                self.chord_queue.clear()

                # update gui
                chord, prob = data
                
                if chord is None: chord = "N/A"
                if prob is None: prob = 0

                self.chord_var.set(chord)
                self.prob_var.set("{:.2f}%".format(prob*100))
            
            if len(self.chunk_queue) > 0:
                # concatenate up to max
                frames = np.array([])
                i = len(self.chunk_queue)-1
                while i >= 0:
                    _frames = np.append(frames, self.chunk_queue[i])
                    if _frames.shape[0] > self.max_frames:
                        break
                    else:
                        frames = _frames
                    
                    i -= 1
                
                self.plot_waveform(frames)


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
                _frames = np.append(frames, chunk_q[i])
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

def main():
    chunk_queue = deque([]) # audio (write), compute + gui (read only)
    chord_queue = [] # audio (none), compute (write), gui (read only)

    CHUNK_SIZE = 1024
    DISPLAY_CHUNKS = 200
    CHORD_CHUNKS = 8
    HOP_CHUNKS = 2
    BLOCK_CHUNKS = 8

    audio_thread = threading.Thread(
        target=start_stream, 
        args=(chunk_queue, ),
        kwargs={'max_chunks': DISPLAY_CHUNKS, 'chunk': CHUNK_SIZE}
    )
    compute_thread = threading.Thread(
        target=start_compute,
        args=(chunk_queue, chord_queue,), 
        kwargs={'poll_delay': 0.01, 'fs': 44100, 'max_frames': CHORD_CHUNKS*CHUNK_SIZE, 'iBlockLength':BLOCK_CHUNKS*CHUNK_SIZE, 'iHopLength':HOP_CHUNKS*CHUNK_SIZE, 'algorithm':ChordAlgo.RAW}
    )

    gui = App(chunk_queue, chord_queue, tick=1, max_frames=DISPLAY_CHUNKS*CHUNK_SIZE)

    audio_thread.daemon = True
    audio_thread.start()

    compute_thread.daemon = True
    compute_thread.start()

    gui.mainloop()
    
if __name__ == '__main__':
    main()
    