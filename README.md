# FMTools
FMTools is a repository of apps you can use to make your FM broadcast better, pirate or not this will help you if you don't have something, maybe you want a better stereo encoder? SCA? or even went crazy and decided to do quadrophonic? We have what you need, for RDS just use MiniRDS

# STCode
STCode is a simple stereo encoder for FM, it uses pasimple and math to:
-   Calculate mono signal ((L+R)/2)
-   Generate the stereo pilot in phase to the stereo subcarrier
-   Generate the stereo diffrence signal using DSB-SC ((L-R)/2)

All that in about 3.5% cpu usage on a RPI-5 (lpf makes it 10, but stereo tool has 3 threads which do 100% cpu usage anyway, one 200)!

Also nearly no latency, not like Stereo Tool (or mpxgen which doesn't even work)

As far as i've tested it (29-31 december) it's been fine but after a fix it was great, so i'd redecommend you

Also i'd recommend to use the SSB version because it's more spectrum effiecent

# SSB-STCode
This is a version of the stereo code but instead of DSB-SC it transmits some kind of VSG (mostly USB with a bit of LSB), about 600 hz of usb is left, just as god intended (Hilbert isn't perfect so i got some usb but managed to turn it into more into LSB)

This also has a cpu usage of 20% with lpf, but goes to 13-15% without the lpf

# SCAMod
SCAMod is a simple FM modulator which can be used to modulate a secondary audio stream, has similiar cpu usage and latency as STCode

Has a fine quality, but as it goes for 12 khz fm signals

# QDCode
QD code is a FM quadrophonic encoder, following the Dorren standard

I haven't tested this, but i'm scared, i don't have a decoder anyway

# CSTCode
This is a stereo encoder, but using the crosby system, as we all know, stereo is made of these things:
0-15: mono
19 khz: pilot
38 khz: stereo
but the crosby system is:
0-15: mono (seems normal, right?)
50 khz: fm modulated l-r


yeah (https://en.wikipedia.org/wiki/Crosby_system)