<h1 align="center">
  <div align="center">
    <a href="https://youtube.com/watch?v=-3eEzzKrywo">
      <img src="https://img.youtube.com/vi/-3eEzzKrywo/0.jpg">
    </a>
  </div>
  Chordy
  <br>
</h1>

<p align="center">
  A configurable, multi-threaded, real-time chord detector for musicians and audio nerds.
</p>


<h4 align="center">
  <a href="https://opensource.org/licenses/Apache-2.0">
    <img src="https://img.shields.io/badge/MIT-blue.svg?label=license" alt="license" style="height: 20px;">
  </a>
  <a href="[https://youtube.com/watch?v=-3eEzzKrywo](http://makeapullrequest.com)">
    <img src="https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=shields" alt="pr" style="height: 20px;">
  </a>
  <a href="https://youtube.com/watch?v=-3eEzzKrywo">
    <img src="https://img.shields.io/badge/youtube-d95652.svg?logo=youtube" alt="youtube" style="height: 20px;">
  </a>
</h4>

## Architecture
Chordy maintains three threads to isolate audio streaming, chord recognition, and GUI rendering, with dequeues for data management.

### Audio Streaming
Chordy uses `pyaudio` to stream microphone audio into a queue of chunks. 

### Chord Recognition
Chordy uses interpretable, analytic techniques instead of machine learning, building on Alexander Lerch's `pyACA` package. See `compute_chords` in `chord.py` for the implementation.

### Graphics
Chordy uses Python's default `tkinter` package for GUI rendering, as well as `scipy` for timeseries resampling. 

## Performance
The GUI application ticks at ~23.7fps (42.1 ms/f), while the chord detection itself takes ~6.7ms (~149 per sec). 

## Usage
### MacOS CLI Tool
See `/release/chordy` for an executable MacOS CLI tool. Add to PATH if needed and refer to the following usage guidelines:
```
usage: chordy [options] [audio] [gui] [algo]

A configurable, multi-threaded, real-time chord detector!

options:
  -h, --help            show this help message and exit

audio:
  --sample-rate FS, -fs FS
                        Audio sample rate in frames per second (default: 44100)
  --chunk-size CHUNK_SIZE, -cs CHUNK_SIZE
                        Number of samples per streamed chunk (default: 1024)

gui:
  --display-chunks DISPLAY_CHUNKS, -d DISPLAY_CHUNKS
                        Number of chunks to display in viewer waveform (default: 200)

algo:
  --chord-chunks CHORD_CHUNKS, -cc CHORD_CHUNKS
                        Number of chunks to send to chord recognition algorithm (default: 8)
  --hop-chunks HOP_CHUNKS, -hc HOP_CHUNKS
                        Number of chunks to hop (default: 2)
  --block-chunks BLOCK_CHUNKS, -bc BLOCK_CHUNKS
                        Number of chunks per block (default: 2)
  --algorithm {RAW,VITERBI}, -a {RAW,VITERBI}
                        Choice of recognition algorithm (default: RAW)
  --raw                 Use raw chords and probabilities (default: None)
  --viterbi             Use Markov Chains / Viterbi Algorithm to process raw chord probabilities (default: None)
  --threshold THRESHOLD, -t THRESHOLD
                        Minimum probability for a detected chord (default: 0.07)
```
### MacOS `.dmg`
See `/release/Chordy.dmg`. Some users may experience a malicious software warning when running the application. Navigate to `Settings > Privacy and Security`, scroll down, and choose `Open Anyway`. 

### Source Build
For Windows / Linux users, a source build is mandatory. Clone the respository, install the dependencies, and run `python src/main.py`. Run `python src/main.py -h` for assistance.

## Contribution
This project is open to contribution! Feel free to open a PR / GitHub issue!

## References
```
@book{lerch2012introduction,
  title={An introduction to audio content analysis: Applications in signal processing and music informatics},
  author={Lerch, Alexander},
  year={2012},
  publisher={Wiley-IEEE Press}
}
``````
