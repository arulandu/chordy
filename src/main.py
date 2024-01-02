from enum import Enum
from collections import Counter
from pyACA import computeChords, computeChordsCl

import sys
import numpy as np
import pyaudio
import wave
from scipy.io.wavfile import read as wavread

def norm_audio(x):
    if x.dtype == 'float32':
        audio = x
    else:
        # change range to [-1,1)
        if x.dtype == 'uint8':
            nbits = 8
        elif x.dtype == 'int16':
            nbits = 16
        elif x.dtype == 'int32':
            nbits = 32

        audio = x / float(2**(nbits - 1))

    # special case of unsigned format
    if x.dtype == 'uint8':
        audio = audio - 1.

    return audio

def stream(handler, max_frames=1e99, chunk=1024, channels=1, fs=44100):
    p = pyaudio.PyAudio()
    print("initialized pyaudio")

    stream = p.open(format=pyaudio.paInt16,
                    channels=channels,
                    rate=fs,
                    frames_per_buffer=chunk,
                    input=True)
    print("opened stream")

    frames = np.array([])

    try:
        while True:
            bdata = stream.read(chunk)
            data = norm_audio(np.frombuffer(bdata, dtype=np.int16))

            frames = np.append(frames, data)
            if frames.shape[0] >= max_frames:
                frames = frames[-max_frames:]

            handler(frames, fs=fs)
            
    except KeyboardInterrupt: 
        pass # continue to close the stream

    print("close stream")
    stream.stop_stream()
    stream.close()
    p.terminate()

class ChordAlgo(Enum):
    RAW = 1
    VITERBI = 2

def make_chord_handler(algorithm=ChordAlgo.RAW, iBlockLength=8192, iHopLength=2048):
    def handler(frames, *args, **kwargs):
        fs = kwargs['fs']
        
        (rawChordLabel, viterbiChordLabel), aiChordIdx, t, P_E = computeChords(frames, fs, iBlockLength=iBlockLength, iHopLength=iHopLength)
        
        chordLabel = None
        if algorithm == ChordAlgo.RAW: 
            chordLabel = rawChordLabel
        elif algorithm == ChordAlgo.VITERBI:
            chordLabel = viterbiChordLabel
        
        if chordLabel is None:
            raise Exception("Algorithm type doesn't exist")
        
        chordLabel = np.array(chordLabel)
        print(chordLabel)
        
    return handler

if __name__ == "__main__":
    HOP_MEMORY = 4
    HOP_LENGTH = 2048
    handler = make_chord_handler(iHopLength=HOP_LENGTH)
    stream(handler, max_frames=HOP_LENGTH*HOP_MEMORY)
