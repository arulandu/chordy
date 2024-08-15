#include <string>
#include <kiss_fft.h>

struct ChordConfig {
    int n;
    float sampleRate;
    kiss_fft_cfg cfg;
    kiss_fft_cpx *in, *out;
};

struct ChordComputeData {
    std::string name;
    float* spec;
};

ChordComputeData* initChordComputeData(int n);
void freeChordComputeData(ChordComputeData* x);
ChordConfig initChordConfig(int n, float sampleRate);
void freeChordConfig(ChordConfig& cfg);
void computeChord(ChordComputeData& out, float* samples, ChordConfig& cfg);