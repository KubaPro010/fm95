# fm95
FM95 is a audio processor for FM, it does:
- Pre-Emphasis
- Stereo
- Polar Stereo
- SCA

Supports these inputs:
- Audio (via Pulse)
- MPX (via Pulse)
- RDS (via Pulse, expects unmodulated RDS)
- RDS2 (via Pulse, expects unmodulated RDS, one stream on 66.5)
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
Should run completly fine on a pi 5, right now with the preemp, on a pi 3b, its 20%

# Recommendations
use a lpf, for example swh-plugins's lowpass_iir, for example:

```
pactl load-module module-null-sink sink_name=FM_Audio rate=48000 # this goes to fm95
pactl load-module module-ladspa-sink sink_name=FM_Audio_lpf sink_master=FM_Audio plugin=lowpass_iir_1891 label=lowpass_iir control=15000,6 rate=48000 # use 4 poles minimum
pactl load-module module-loopback source=radio_audio.monitor sink=FM_Audio_lpf rate=48000 # from the apps to the filter
```