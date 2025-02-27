#include <map>
#include <iostream>
#include <algorithm>
#include <cassert>
#include "chord.h"

ChordComputeData* initChordComputeData(int n) {
    ChordComputeData* x = (ChordComputeData*)malloc(sizeof(ChordComputeData));
    x->spec = (float*)malloc(sizeof(float)*(n/2+1));
    x->hps = (float*)malloc(sizeof(float)*(n/2+1));
    x->chroma = (float*)malloc(sizeof(float)*12);
    return x;
}

void freeChordComputeData(ChordComputeData* x) {
    free(x->spec);
    free(x->hps);
    free(x->chroma);
    delete x;
}

ChordConfig initChordConfig(int n, float sampleRate, int octaves, float threshold) {
    ChordConfig cfg;
    cfg.octaves = octaves;
    cfg.threshold = threshold;
    cfg.n = n;
    cfg.sampleRate = sampleRate;
    cfg.cfg = kiss_fftr_alloc(n, 0, nullptr, nullptr);
    cfg.out = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx)*n);
    return cfg;
}

void freeChordConfig(ChordConfig& cfg) {
    free(cfg.out);
    free(cfg.cfg);
}

const bool mask[2][12] = {
    {1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0},
    {1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0},
};

std::string maskIndToName(int ind) {
    switch (ind) {
        case 0: return "Maj";
        case 1: return "Min"; 
        default: return "";
    }
}

int freq2Midi(float freq) {
    return std::round(12 * std::log2(freq / 440.0) + 69);
}

float midi2Freq(int midi) {
    return std::exp2f((midi-69)/12.)*440.;
}

int freq2Bin(float freq, int sampleRate, int n) {
    return std::floorf(freq*n/sampleRate);
}



void computeChord(ChordComputeData& out, float* samples, ChordConfig& cfg) {
    kiss_fftr(cfg.cfg, samples, cfg.out);
    
    for(int i = 0; i < cfg.n/2; i++) {
        out.spec[i] = cfg.out[i].r * cfg.out[i].r + cfg.out[i].i * cfg.out[i].i;
    }
    
    const int highPassBin = freq2Bin(50, cfg.sampleRate, cfg.n);
    const float qrat = 1.02930223664349F; // quarter tone ratio
    float mid = midi2Freq(60);
    for(int p = 0; p < 12; p++) {
        float lbf = mid/qrat, ubf = mid*qrat;
        out.chroma[p] = 1;
        for(int r = 0; r < cfg.octaves; r++){
            // downsample to bins
            const int lbi = freq2Bin(lbf, cfg.sampleRate, cfg.n), ubi = std::min(freq2Bin(ubf, cfg.sampleRate, cfg.n), cfg.n/2);
            // ignore first 3 bins (high pass filter)
            float sm = 0; for(int j = std::max(lbi, highPassBin); j <= ubi; j++) sm += out.spec[j];
            float avg = sm/(ubi-lbi+1);

            // harmonic product spectrum
            out.chroma[p] *= avg; 
            lbf *= 2.F; ubf *= 2.F;
        }
        mid *= qrat*qrat;
    }

    float chords[24]; int inds[24];
    for(int c = 0; c < 24; c++) {
        inds[c] = c;
        int i = c/12, p = c%12;
        float sm = 0, sz = 0;
        for(int q = 0; q < 12; q++) {
            sm += mask[i][q]*out.chroma[(p+q)%12];
            sz += mask[i][q];
        }
        chords[c] = sm/sz;
    }

    std::sort(inds, inds+24, [&chords](int a, int b) {
        return chords[a] > chords[b];
    });

    float maxSpec = 0;
    for(int i = highPassBin; i < cfg.n/2; i++) {
        maxSpec = std::max(maxSpec, out.spec[i]);
    }
    
    for(int c = 0; c < 24; c++) chords[c] /= maxSpec;

    int best = 0;
    for(int c = 0; c < 24; c++) {
        if(chords[c] > chords[best]) best = c;
    }

    if(chords[best] > cfg.threshold){
        out.name = notes[best%12] + " " + maskIndToName(best/12); 
        std::cout << "COMPUTE: " << chords[best] << " / " << maxSpec << " \t<- ";        
        for(int i = 0; i < 5; i++) {
            int idx = inds[i];
            std::cout << notes[idx%12] << " " << maskIndToName(idx/12) << ", ";
        }
        std::cout << std::endl;
    } else {
        out.name = "N/A";
    }
}