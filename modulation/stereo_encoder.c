#include "stereo_encoder.h"

// Multiplier is the multiplier to get to 19 khz, or 31.25 if polar
void init_stereo_encoder(StereoEncoder* st, uint8_t multiplier, Oscillator* osc, uint8_t polar, float mono_volume, float pilot_volume, float stereo_volume) {
    st->multiplier = multiplier;
    st->osc = osc;
    st->polar = polar;
    st->mono_volume = mono_volume;
    st->pilot_volume = pilot_volume;
    st->stereo_volume = stereo_volume;
}

float stereo_encode(StereoEncoder* st, uint8_t enabled, float left, float right) {
    float mid = (left+right) * 0.5f;
    if(!enabled) return mid * st->mono_volume;
    float side = (left-right) * 0.5f;

    if(!st->polar) {
        float pilot = get_oscillator_sin_multiplier_ni(st->osc, st->multiplier);
        float carrier = get_oscillator_sin_multiplier_ni(st->osc, st->multiplier * 2.0f);
        return (mid*st->mono_volume) + (pilot*st->pilot_volume) + ((side*carrier) * st->stereo_volume);
    } else {
        float carrier = get_oscillator_sin_multiplier_ni(st->osc, st->multiplier);
        return (mid*st->mono_volume) + (((side+0.2f)*carrier) * st->stereo_volume); // Polar stereo does not contain a pilot, but it contains a -14 db carrier wave on the stereo subcarrier
    }
}