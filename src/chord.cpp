#include <map>
#include <iostream>
#include <algorithm>
#include "chord.h"

// TODO: redo spec/hps buffers to n/2
ChordComputeData* initChordComputeData(int n) {
    ChordComputeData* x = (ChordComputeData*)malloc(sizeof(ChordComputeData));
    x->spec = (float*)malloc(sizeof(float)*n);
    x->hps = (float*)malloc(sizeof(float)*n);
    x->chroma = (float*)malloc(sizeof(float)*12);
    return x;
}

void freeChordComputeData(ChordComputeData* x) {
    free(x->spec);
    free(x->hps);
    free(x->chroma);
    delete x;
}

ChordConfig initChordConfig(int n, float sampleRate, int R) {
    ChordConfig cfg;
    cfg.R = R;
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
    
    for(int i = 0; i < cfg.n; i++) out.spec[i] = cfg.out[i].r * cfg.out[i].r + cfg.out[i].i * cfg.out[i].i;
    memset(out.hps, 0, sizeof(float)*cfg.n);

    for(int i = 3; i < cfg.n/2; i++){ // set first 3 bins to 0
        out.hps[i] = 1;
        for(int r = 1; r <= cfg.R; r++){
            out.hps[i] *= out.spec[r*i];
        }
        out.hps[i] = sqrtf(out.hps[i]);
    }

    const float qrat = 1.02930223664349F; // quarter tone ratio
    float mid = midi2Freq(60);
    for(int p = 0; p < 12; p++) {
        float lbf = mid/qrat, ubf = mid*qrat;
        for(int r = 0; r < cfg.R; r++){
            const int lbi = freq2Bin(lbf, cfg.sampleRate, cfg.n), ubi = freq2Bin(ubf, cfg.sampleRate, cfg.n);
            float sm = 0; for(int j = lbi; j <= ubi; j++) sm += out.spec[j];
            out.chroma[p] += sm/(ubi-lbi+1);

            lbf *= 2.F; ubf *= 2.F;
        }
        mid *= qrat*qrat;
    }

    float chords[2][12];
    std::pair<int, int> mxl = {0, 0};
    for(int i = 0; i < 2; i++) {
        for(int p = 0; p < 12; p++) {
            float sm = 0, c = 0;
            for(int q = 0; q < 12; q++) {
                sm += mask[i][q]*out.chroma[(p+q)%12];
                c += mask[i][q];
            }
            chords[i][p] = sm/c;
            if(chords[i][p] > chords[mxl.first][mxl.second]) mxl = {i, p};
        }
    }

    float avg = 0; for(int p = 0; p < 12; p++) avg += out.chroma[p];
    avg /= 12.;

    if(chords[mxl.first][mxl.second] > avg){
        out.name = notes[mxl.second] + " " + maskIndToName(mxl.first); 
    } else {
        out.name = "N/A";
    }
}