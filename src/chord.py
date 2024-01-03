from enum import IntEnum
from collections import Counter
from pyACA import computeChords, computeChordsCl

import threading
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

def stream(handler=lambda *args, **kwargs : None, chunk=1024, channels=1, fs=44100):
    p = pyaudio.PyAudio()
    print("initialized pyaudio")

    stream = p.open(format=pyaudio.paInt16,
                    channels=channels,
                    rate=fs,
                    frames_per_buffer=chunk,
                    input=True)
    print("opened stream")

    try:
        while True:
            bdata = stream.read(chunk)
            data = norm_audio(np.frombuffer(bdata, dtype=np.int16))

            handler(data)
    except Exception as e: 
        pass # continue to close the stream

    print("close stream")
    stream.stop_stream()
    stream.close()
    p.terminate()

class ChordAlgo(IntEnum):
    RAW = 0
    VITERBI = 1

def formatChordLabel(label: str):
    return label.upper().replace("MAJ", "").replace("MIN", "m").replace(" ", "")

def compute_chords(frames, fs=44100, threshold=0.05, algorithm=ChordAlgo.RAW, iBlockLength=8192, iHopLength=2048):
    cChordLabel, aiChordIdx, t, P_E = computeChords(frames, fs, iBlockLength=iBlockLength, iHopLength=iHopLength)
    chordLabel, chordIdx = cChordLabel[algorithm], aiChordIdx[algorithm]

    chordLabel = np.array(chordLabel)

    chordDict = {}
    for i,chord in enumerate(chordLabel):
        if chord not in chordDict: chordDict[chord] = 0
        chordDict[chord] += P_E[chordIdx[i]][0]   
    mostLikelyChord = list(max(chordDict.items(), key=lambda x: x[1]))
    mostLikelyChord[1] /= len(chordLabel) # max avg probability of chord over frames

    chord, prob = mostLikelyChord
    chord = formatChordLabel(chord)
    if prob < threshold:
        chord = None
    
    return chord, prob


