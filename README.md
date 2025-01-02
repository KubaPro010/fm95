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

# PSTCode
This is a yet another version of a Stereo encoder, however for the OIRT band which is in use in Russia, Belarus and other countries

Haven't tested it nor plan to

# CrosbySTCode
This is a stereo coder however with a diffrent system, let me yap some: 
In the 1950-1960s the FCC had to decide between two stereo coding systems, we had the Zenith/GE system and the Crosby system, what was the diffrence?
The Zenith system had a 19 khz pilot and a 38 khz dsb-sc modulated stereo l-r signal, sounds familliar? yeah that's why you haven't heard of the crosby system
The crosby system on the other hand had a (better) decision of modulation the l-4 signal into 50 khz with FM, why was it rejected? becuase of SCA, 67 and 41 khz were used up by stereo, 41 khz was also used up on the Zenith system but who cares

Also it doesnsn't sound bad, how may ask where did i find a decoder for it? Made it myself in GNU radio, it's even easier, if they chose 35 khz instead of 50, then we'd be using this, also i like this one because it has FM, not AM so if some idiot has a transmitter with hardware pre-emphasis then the stereo won't be affected by it

# SCAMod
SCAMod is a simple FM modulator which can be used to modulate a secondary audio stream, has similiar cpu usage and latency as STCode

Has a fine quality, but as it goes for 12 khz fm signals

# StereoSCAMod
Stereo SCA, like normal SCA but encodes L-R onto 80 khz, only demodulator of this right now is gnu radio