# STCode
STCode is a simple stereo encoder for FM, it uses pasimple and math to:
    - Calculate mono signal from s16le ((L+R)/2)
    - Generate the stereo pilot in phase to the stereo subcarrier
    - Generate the stereo diffrence signal using DSB-SC
All that in about 3.5% cpu usage on a RPI-5!