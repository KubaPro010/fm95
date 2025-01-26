# fm95
FM95 is a audio processor for FM, it does:
- Pre-Emphasis
- Low Pass Filter
- Stereo
- SSB Stereo
- Polar Stereo
- Polar SSB Stereo (huh)
- SCA

Supports 2 inputs:
- Audio (via Pulse)
- MPX (via Pulse)

and one output:
- MPX (via Pulse or ALSA)

# How to compile?
To compile you need `cmake`, `libasound2-dev` and `libpulse-dev`, if you have those then do these commands:
```
mkdir build
cd build
cmake ..
make
```
Done!