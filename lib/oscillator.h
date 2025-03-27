#pragma once

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    #include <arm_neon.h>
    #define USE_NEON 1
#else
    #define USE_NEON 0
#endif

#include "constants.h"
#include <math.h>

typedef struct {
	float phase;
	float phase_increment;
	float sample_rate;
} Oscillator;

void init_oscillator(Oscillator *osc, float frequency, float sample_rate);
void change_oscillator_frequency(Oscillator *osc, float frequency);
float get_oscillator_sin_sample(Oscillator *osc);
float get_oscillator_cos_sample(Oscillator *osc);
float get_oscillator_sin_multiplier_ni(Oscillator *osc, float multiplier);
float get_oscillator_cos_multiplier_ni(Oscillator *osc, float multiplier);
void advance_oscillator(Oscillator *osc);