# fm95
FM95 is a audio processor for FM, it does:
- Pre-Emphasis
- Low Pass Filter
- Stereo
- Polar Stereo
- SCA

Supports 2 inputs:
- Audio (via Pulse)
- MPX (via Pulse)
- SCA (via Pulse)

and one output:
- MPX (via Pulse)

# How to compile?
To compile you need `cmake` and `libpulse-dev`, if you have those then do these commands:
```
mkdir build
cd build
cmake ..
make
```
Done!