#include "stereo_encoder.h"

// Multiplier is the multiplier to get to 19 khz
void init_stereo_encoder(StereoEncoder* st, uint8_t multiplier, Oscillator* osc, float audio_volume, float pilot_volume) {
    st->multiplier = multiplier;
    st->osc = osc;
    st->pilot_volume = pilot_volume;
    st->audio_volume = audio_volume;
}

float stereo_encode(StereoEncoder* st, uint8_t enabled, float left, float right) {
    float mid = (left+right) * 0.5f;
    if(!enabled) return mid * st->audio_volume;

    float half_audio = st->audio_volume * 0.5f;

    float side = (left-right) * 0.5f;
    
    float signalx1 = get_oscillator_sin_multiplier_ni(st->osc, st->multiplier);
    float signalx2 = get_oscillator_sin_multiplier_ni(st->osc, st->multiplier * 2.0f);

    return (mid*half_audio) + (signalx1*st->pilot_volume) + ((side*signalx2) * half_audio);
}