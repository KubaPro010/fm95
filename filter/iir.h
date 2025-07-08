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
    float b0, b1;
    float a1;

    float x_prev;
    float y_prev;
} TiltCorrectionFilter;

void tilt_init(TiltCorrectionFilter *filter, float correction_strength);
float tilt(TiltCorrectionFilter *filter, float input_sample);
