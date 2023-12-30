# Chordy
A real-time polyphonic pitch detector for perfecting guitar chords.

## Approach
1. Read a .wav file with guitar chords
2. Normalize and chunk the buffer into windows
3. For each window:
    - Compute the FFT
    - Compute the Harmonic Product Spectrum (HPS)
    - Sort frequencies by HPS
    - Convert all frequencies to their respective note number
    - De-duplicate notes while maintaining sort order
    - Output first ~10 notes

## Considerations
The Harmonic Product Spectrum (HPS) technique is meant to be used for monophonic audio, so this adaptation for polyphonic audio has proven difficult but has shown some promise. 