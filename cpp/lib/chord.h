#include <string>
#include <kiss_fftr.h>

const std::string notes[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

struct ChordConfig {
    int n;
    int octaves;
    float threshold;
    float sampleRate;
    kiss_fftr_cfg cfg;
    // kiss_fft_scalar *in;
    kiss_fft_cpx *out;
};

struct ChordComputeData {
    std::string name = "C";
    float* spec;
    float* hps;
    double dt;
    float* chroma;
};

ChordComputeData* initChordComputeData(int n);
void freeChordComputeData(ChordComputeData* x);
ChordConfig initChordConfig(int n, float sampleRate, int octaves, float threshold);
void freeChordConfig(ChordConfig& cfg);
void computeChord(ChordComputeData& out, float* samples, ChordConfig& cfg);