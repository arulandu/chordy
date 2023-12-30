import numpy as np
from scipy.io import wavfile
from scipy import fft
import math

import matplotlib.pyplot as plt
import plotly
from plotly import express as ex
from plotly import graph_objects as go
from plotly.subplots import make_subplots

FILENAME = "./res/A_D_Em_G_Bm.wav"

def compute_fft(data, sample_rate):
    power = np.abs(fft.rfft(data))
    freq = fft.rfftfreq(len(data), 1./sample_rate)
    return freq, power

def nearest_note_num(hz):
    v = hz_to_note_num(hz)
    rounded = int(np.rint(v))
    return rounded

def hz_to_note_num(hz):
    if hz > 0:
        return 12*np.log2(hz/440.)+49
    else:
        return -1

def note_num_to_hz(n):
    return np.exp2((n-49)/12)*440

def note_num_to_name(n):
    notes = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

    if n >= 0:
        return notes[(n+8)%12] + str((n+8)//12)
    else:
        return "DNE"

def HPS(X, sample_rate):
    """
    Computes the Harmonic Product Spectrum
    Modified From: https://github.com/alexanderlerch/pyACA/blob/master/pyACA/PitchSpectralHps.py
    """

    iOrder = 4
    f_min = 300
    f_0 = np.zeros(X.shape[0])

    iLen = int((X.shape[0] - 1) / iOrder)
    afHps = X[np.arange(0, iLen)]
    k_min = int(round(f_min / sample_rate * 2 * (X.shape[0] - 1)))

    # compute the HPS
    for j in range(1, iOrder):
        X_d = X[::(j + 1)]
        afHps *= X_d[np.arange(0, iLen)]

    hps = afHps[np.arange(k_min, afHps.shape[0])]
    f_hz = (np.arange(hps.shape[0]) + k_min) / (X.shape[0] - 1) * sample_rate / 2

    return f_hz, hps

def main():
    sample_rate, data = wavfile.read(FILENAME)
    data_len = len(data)
    chunk_len = sample_rate # ~44100
    num_chunks = data_len//chunk_len

    data = data.T[0] # WLOG stero = mono :) 
    norm_data = data/(2**(16.-1)) # 16bit [-2**15, 2**15] -> norm [-1, 1]

    fig, axs = plt.subplots(num_chunks, 3, figsize=(12, 4*num_chunks))
    for i in range(num_chunks):
        print(f"CHUNK {i}/{num_chunks-1}")
        chunk = norm_data[chunk_len*i:chunk_len*(i+1)]
        freq, power = compute_fft(chunk, sample_rate)
        fhz, hps = HPS(power, sample_rate)

        sort_inds = hps.argsort()[::-1]
        sorted_fhz, sorted_hps = fhz[sort_inds], hps[sort_inds]

        notes = [note_num_to_name(nearest_note_num(hz)) for hz in sorted_fhz] # TODO: vectorize        
        filtered_notes = []

        # TODO: optimize
        note_set = set()
        for note in notes:
            if note not in note_set:
                filtered_notes.append(note)
                note_set.add(note)

        print(filtered_notes[:10]) # arbitrary

        axs[i, 0].plot(chunk)
        axs[i, 1].plot(freq, power)
        axs[i, 2].plot(fhz, hps)
    
    fig.savefig("./res/chunk_fft.png")


if __name__ == "__main__":
    main()
