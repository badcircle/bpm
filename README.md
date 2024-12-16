# Fast BPM Detector

A high-performance command-line tool written in C that detects the BPM (Beats Per Minute) of MP3 files. Uses parallel processing and efficient signal processing algorithms for quick and accurate tempo detection.

## Features

- Fast MP3 decoding using libmpg123
- OpenMP parallelization for performance
- Adaptive thresholding for beat detection
- Supports both mono and stereo MP3 files
- Memory-efficient streaming processing
- Handles BPM ranges from 60 to 200 BPM

## Prerequisites to build

You need MinGW-w64 and MSYS2 installed on your Windows system. The following libraries are required:

- libmpg123 (for MP3 decoding)
- OpenMP (for parallel processing)

## Installation
See Releases

## Installation from source

1. Install MSYS2 from https://www.msys2.org/

2. Open MSYS2 terminal and install required packages:
```bash
pacman -Syu
pacman -S mingw-w64-x86_64-gcc
pacman -S mingw-w64-x86_64-mpg123
```

3. Clone this repository:
```bash
git clone https://github.com/badcircle/bpm.git
cd bpm
```

4. Compile the program:
```bash
gcc -O3 -fopenmp bpm.c -o bpm.exe -I"C:/msys64/mingw64/include" -L"C:/msys64/mingw64/lib" -lmpg123
```

## Usage

```bash
./bpm.exe path/to/your/music.mp3
```

Example output:
```
Sample rate: 44100 Hz
Channels: 2
Reading MP3 data...
Read 7654321 mono samples
Number of analysis frames: 14876
Calculating energy...
Detecting onsets...
Found 328 onsets
Calculating BPM...
Median interval: 0.416667 seconds

Estimated BPM: 144
```

## How It Works

1. **Audio Decoding**: Uses libmpg123 to efficiently decode MP3 files into raw PCM data
2. **Mono Conversion**: Converts stereo audio to mono for consistent processing
3. **Energy Analysis**: Calculates energy levels across small windows of audio data
4. **Onset Detection**: Uses adaptive thresholding to detect significant changes in energy
5. **BPM Calculation**: Analyzes intervals between onsets to determine the dominant tempo

## Energy stats
- Mean represents the average "background" level of the song
- Max represents the loudest peak/moment
- Standard deviation tells us how much the song typically varies from that background level
- Threshold is our "this is probably a beat" cutoff point, set above the normal variation (mean + 1.5 * std_dev)

## Performance

The program uses several optimizations for performance:
- OpenMP parallel processing for frame analysis
- Efficient memory management with streaming processing
- Optimized signal processing algorithms
- Compiler optimizations (-O3 flag)

## Limitations

- Designed for music with clear beats
- Works best with BPM range of 60-200
- May need parameter adjustments for certain genres
- Currently supports MP3 format only

## Contributing

Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.

## License

This project is licensed under the GNU General Public License v3.0 - see the LICENSE file for details.
