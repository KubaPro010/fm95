#pragma once

#include <math.h>
#include "../lib/constants.h"

typedef struct
{
	float alpha;
	float prev_sample;
	float gain;
} ResistorCapacitor;

void init_preemphasis(ResistorCapacitor *filter, float tau, float sample_rate, float ref_freq);
float apply_preemphasis(ResistorCapacitor *filter, float sample);

typedef struct {
    float a0, a1;     // lowpass coeffs
    float lp;         // lowpass state
    float low_gain;   // gain for low frequencies
    float high_gain;  // gain for high frequencies
} TiltCorrectionFilter;

void tilt_init(TiltCorrectionFilter* f, float correction_strength, float sr);
float tilt(TiltCorrectionFilter *f, float in);
