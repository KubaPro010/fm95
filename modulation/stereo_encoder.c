#include "stereo_encoder.h"

// Multiplier is the multiplier to get to 19 khz
void init_stereo_encoder(StereoEncoder* st, uint8_t multiplier, Oscillator* osc, float mono_volume, float pilot_volume, float stereo_volume) {
    st->multiplier = multiplier;
    st->osc = osc;
    st->mono_volume = mono_volume;
    st->pilot_volume = pilot_volume;
    st->stereo_volume = stereo_volume;
}

float stereo_encode(StereoEncoder* st, uint8_t enabled, float left, float right) {
    float mid = (left+right) * 0.5f;
    if(!enabled) return mid * st->mono_volume;

    float side = (left-right) * 0.5f;
    
    float signalx1 = get_oscillator_sin_multiplier_ni(st->osc, st->multiplier);
    float signalx2 = get_oscillator_sin_multiplier_ni(st->osc, st->multiplier * 2.0f);

    return (mid*st->mono_volume) + (signalx1*st->pilot_volume) + ((side*signalx2) * st->stereo_volume);
}