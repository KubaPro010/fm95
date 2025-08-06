#pragma once

#include <stdint.h>
#include "../dsp/oscillator.h"

typedef struct
{
    uint8_t multiplier;
    Oscillator* osc;
    float audio_volume;
    float pilot_volume;
} StereoEncoder;

void init_stereo_encoder(StereoEncoder *st, uint8_t multiplier, Oscillator *osc, float audio_volume, float pilot_volume);

float stereo_encode(StereoEncoder* st, uint8_t enabled, float left, float right);
