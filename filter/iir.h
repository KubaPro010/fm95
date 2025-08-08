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
    float tilt;     // Tilt amount (-1.0 to +1.0 typical)
    float prev_in;  // Previous input sample
    float prev_out; // Previous output sample
} TiltCorrectionFilter;

void tilt_init(TiltCorrectionFilter* filter, float alpha);
float tilt(TiltCorrectionFilter *filter, float input);
