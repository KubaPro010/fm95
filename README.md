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

# CPU Usage?
Should run completly fine on a pi 5, right now with the preemp, lpf, compressor on a pi 3b, its 35-40% cpu usage, get rid of compressor and it goes to 20%