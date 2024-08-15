#include <map>
#include <iostream>
#include <algorithm>
#include "chord.h"

// TODO: redo spec/hps buffers to n/2
ChordComputeData* initChordComputeData(int n) {
    ChordComputeData* x = (ChordComputeData*)malloc(sizeof(ChordComputeData));
    x->spec = (float*)malloc(sizeof(float)*n);
    x->hps = (float*)malloc(sizeof(float)*n);
    return x;
}

void freeChordComputeData(ChordComputeData* x) {
    free(x->spec);
    free(x->hps);
    delete x;
}

ChordConfig initChordConfig(int n, float sampleRate, int R) {
    ChordConfig cfg;
    cfg.R = R;
    cfg.n = n;
    cfg.sampleRate = sampleRate;
    cfg.cfg = kiss_fft_alloc(n, 0, nullptr, nullptr);
    cfg.in = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx)*n);
    cfg.out = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx)*n);
    return cfg;
}

void freeChordConfig(ChordConfig& cfg) {
    free(cfg.in); free(cfg.out); free(cfg.cfg);
}

// TODO: populate out.spec and display FFT spectra
void computeChord(ChordComputeData& out, float* samples, ChordConfig& cfg) {
    for(int i = 0; i < cfg.n; i++) cfg.in[i].r = samples[i];

    kiss_fft(cfg.cfg, cfg.in, cfg.out);
    
    for(int i = 0; i < cfg.n; i++) out.spec[i] = sqrtf(cfg.out[i].r * cfg.out[i].r + cfg.out[i].i * cfg.out[i].i);
    memset(out.hps, 0, sizeof(float)*cfg.n);

    for(int i = 3; i < cfg.n/2; i++){ // set first 3 bins to 0
        out.hps[i] = 1;
        for(int r = 1; r <= cfg.R; r++){
            out.hps[i] *= out.spec[r*i];
        }
    }

    std::map<int, std::string> noteMap = {
        {0, "C"}, {1, "C#"}, {2, "D"}, {3, "D#"}, {4, "E"}, {5, "F"},
        {6, "F#"}, {7, "G"}, {8, "G#"}, {9, "A"}, {10, "A#"}, {11, "B"}
    };
    int mxBin = 0; for(int i = 0; i < cfg.n/2; i++) if(out.hps[i] > out.hps[mxBin]) mxBin = i;
    float freq = mxBin * (cfg.sampleRate / cfg.n);
    int midi = std::round(12 * std::log2(freq / 440.0) + 69);
    out.name = noteMap[midi%12] + std::to_string((midi / 12)-1);
    // std::cout << freq << ' ' << out.name << ' ' << out.hps[mxBin] << '\n';
}