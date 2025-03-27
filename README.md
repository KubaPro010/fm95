# fm95

FM95 is a audio processor for FM, it does:

- Pre-Emphasis
- Stereo
- Polar Stereo
- SCA

Supports these inputs:

- Audio (via Pulse)
- MPX (via Pulse, basically passthrough, i don't recommend this unless you have something else than rds or sca to modulate)
- RDS (via Pulse, expects unmodulated RDS, stereo, left channel on 57 KHz, right on 66.5, rds95 is recommended here, in modulation this is inphase to the pilot)
- SCA (via Pulse, by default on 67 khz with a 7 khz deviation)

and one output:

- MPX (via Pulse)

## How to compile?

To compile you need `cmake` and `libpulse-dev`, if you have those then do these commands:

```bash
mkdir build
cd build
cmake ..
make
```

Done!

## CPU Usage?

Should run completly fine on a pi 5, fine on a pi 3b

## Other Apps

FM95 also includes some other apps, such as chimer95 which generates GTS tones each half hour, and dcf95 which creates a DCF77 compatible signal
