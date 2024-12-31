# FMTools
FMTools is a repository of apps you can use to make your FM broadcast better, pirate or not this will help you if you don't have something, maybe you want a better stereo encoder? SCA? or even went crazy and decided to do quadrophonic? We have what you need, for RDS just use MiniRDS

# STCode
STCode is a simple stereo encoder for FM, it uses pasimple and math to:
-   Calculate mono signal ((L+R)/2)
-   Generate the stereo pilot in phase to the stereo subcarrier
-   Generate the stereo diffrence signal using DSB-SC ((L-R)/2)

All that in about 3.5% cpu usage on a RPI-5 (lpf makes it 10, but stereo tool has 3 threads which do 100% cpu usage anyway, one 200)!

Also nearly no latency, not like Stereo Tool (or mpxgen which doesn't even work)

# SCAMod
SCAMod is a simple FM modulator which can be used to modulate a secondary audio stream, has similiar cpu usage and latency as STCode

# QDCode
QD code is a FM quadrophonic encoder, following the Dorren standard