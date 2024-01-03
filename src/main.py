import tkinter as tk
from tkinter import font
import threading
import queue
from chord import make_chord_handler, stream
"""arial rounded mt bold gm7#
yuppy sc gm7#
yugothic
Baloo 2
"""
class App(tk.Tk):
    def __init__(self, tick=500) -> None:
        super().__init__()
        self.tick = tick
        
        self.title("Chordy")
        self.geometry('700x350')
        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(0, weight=1)

        self.frame = tk.Frame(self, background="black")
        self.frame.grid(row=0, column=0, sticky="ewns")

        self.frame.grid_anchor("center")

        self.info_frame = tk.Frame(self.frame, background="black")
        self.info_frame.grid(row=0, column=0, sticky="")
        self.info_frame.grid_propagate(True)

        self.default_font = "Arial Rounded Mt Bold"
        self.chord_var = tk.StringVar(value="None")        
        self.chord_label = tk.Label(self.info_frame, fg="white", bg="black", padx=5, font=(self.default_font, 36), textvariable=self.chord_var)
        self.chord_label.grid(row=0, column=0, sticky="ew")

        self.prob_var = tk.StringVar(value="0.00%")
        self.prob_label = tk.Label(self.info_frame, fg="white", bg="black", padx=5, font=(self.default_font, 12), textvariable=self.prob_var)
        self.prob_label.grid(row=1, column=0, sticky="ew")

        # self.protocol('WM_DELETE_WINDOW', lambda: )
        
        self.main()
    
    def main(self):
        q = queue.Queue()
        thread = threading.Thread(target=self.start_stream, args=(q,))
        thread.daemon = True
        thread.start()
        
        def poll():
            nonlocal q

            if not q.empty():
                data = None # get last queued message and clear queue
                while not q.empty():
                    data = q.get()
                
                chord, prob = data
                
                if chord is None: chord = "○"
                if prob is None: prob = 0

                self.chord_var.set(chord)
                self.prob_var.set("{:.2f}%".format(prob*100))

            self.after(self.tick, poll)
        
        poll()
        

    def start_stream(self, q:queue.Queue):
        HOP_MEMORY = 4
        HOP_LENGTH = 2048
        
        def callback(chord: str|None, prob: float):
            if chord is None: prob = None
            q.put([chord, prob])

        handler = make_chord_handler(callback, iHopLength=HOP_LENGTH)
        stream(handler, max_frames=HOP_LENGTH*HOP_MEMORY)
        
    
if __name__ == '__main__':
    app = App()
    app.mainloop()


