# FMTools
FMTools is a repository of apps you can use to make your FM broadcast better, pirate or not this will help you if you don't have something, maybe you want a better stereo encoder? SCA? We have what you need, for RDS just use MiniRDS

# STCode
STCode is a simple stereo encoder for FM, it uses pasimple and math to:
-   Calculate mono signal ((L+R)/2)
-   Generate the stereo pilot in phase to the stereo subcarrier
-   Generate the stereo diffrence signal using DSB-SC ((L-R)/2)

All that in about 3.5% cpu usage on a RPI-5 (stereo tool has 3 threads which do 100% cpu usage anyway, one 200)

Also nearly no latency, not like Stereo Tool (or mpxgen which doesn't even work)

As far as i've tested it (29-31 december) it's been fine but after a fix it was great, so i'd recommend this to you

Also i'd recommend to use the SSB version because it's more spectrum effiecent
but SSB has slightly more cpu usage

This supports alsa output

# PSTCode
This is a yet another version of a Stereo encoder, however for the OIRT band which is in use in Russia, Belarus and other countries

Haven't tested it nor plan to

# SCAMod
SCAMod is a simple FM modulator which can be used to modulate a secondary audio stream, has similiar cpu usage and latency as STCode

Has a fine quality, but as it goes for 12 khz fm signals

# MonoPass
want to keep mono for a reason but have the lpf and preemphasis, do so

# How to compile?
To compile you need `cmake` and `libpulse-dev`, if you have those then do these commands:
```
mkdir build
cd build
cmake ..
make
```
Done!