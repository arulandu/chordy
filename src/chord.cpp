#include <kiss_fft.h>
#include <map>
#include <iostream>
#include <algorithm>

void computeChord(std::string& s, float* samples, int n, float sampleRate) {
    kiss_fft_cfg cfg = kiss_fft_alloc(n, 0, nullptr, nullptr);
    std::vector<kiss_fft_cpx> in(n), out(n); // Re/Im = 0
    for(int i = 0; i < n; i++) in[i].r = samples[i];

    kiss_fft(cfg, in.data(), out.data());
    free(cfg);

    std::vector<double> sqMag(n / 2);
    for (int i = 0; i < n / 2; ++i) {
        sqMag[i] = out[i].r * out[i].r + out[i].i * out[i].i;
    }

    std::map<int, std::string> noteMap = {
        {0, "C"}, {1, "C#"}, {2, "D"}, {3, "D#"}, {4, "E"}, {5, "F"},
        {6, "F#"}, {7, "G"}, {8, "G#"}, {9, "A"}, {10, "A#"}, {11, "B"}
    };
    int mxBin = 0; for(int i = 0; i < n/2; i++) if(sqMag[i] > sqMag[mxBin]) mxBin = i;
    float freq = mxBin * (sampleRate / n);
    int midi = std::round(12 * std::log2(freq / 440.0) + 69);

    s = noteMap[midi%12] + std::to_string((midi / 12)-1);
}