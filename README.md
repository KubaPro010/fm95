# FMTools
FMTools is a repository of apps you can use to make your FM broadcast better, pirate or not this will help you if you don't have something, maybe you want a better stereo encoder? SCA? We have what you need, for RDS just use MiniRDS

# fm95
FM95 is a audio processor for FM, it does:
- Pre-Emphasis
- Low Pass Filter
- Stereo
- SSB Stereo
- Polar Stereo
- Polar SSB Stereo (huh)

Supports 2 inputs:
- Audio (via Pulse)
- MPX (via Pulse)
and one output:
- MPX (via Pulse or ALSA)

Note that i haven't tested it, but i will on monday (29-01-25)

# SCAMod
SCAMod is a simple FM modulator which can be used to modulate a secondary audio stream, has similiar cpu usage and latency as STCode

Has a fine quality, but as it goes for 12 khz fm signals

# How to compile?
To compile you need `cmake` and `libpulse-dev`, if you have those then do these commands:
```
mkdir build
cd build
cmake ..
make
```
Done!