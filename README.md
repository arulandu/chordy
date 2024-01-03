# Chordy
My < 24hr attempt at a real-time polyphonic pitch detector for perfecting guitar chords.

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

## Performance
GUI ticks at ~23.7fps (42.1 ms/f). Chord avg 6.7ms (~149 per sec). 

## References
```
@book{lerch2012introduction,
  title={An introduction to audio content analysis: Applications in signal processing and music informatics},
  author={Lerch, Alexander},
  year={2012},
  publisher={Wiley-IEEE Press}
}
``````
