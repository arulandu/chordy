# Chordy
A port of chordy using C++, OpenGL, and PortAudio. We manually implement the relevant pyACA algorithms.

## References
Huge thanks to PortAudio's lock-free SPSC ring buffer implementation. 

## Lerch's Algorithm
Compute the spectral pitch chroma: round each frequency to the nearest note, take the average power of Hzs for each (octave, note), then for each note sum across the octaves. Then, generate chord node templates and product and filter. 