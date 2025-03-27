#pragma once
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "constants.h"
#include "oscillator.h"

typedef struct
{
	float alpha;
	float prev_sample;
	float gain;
} ResistorCapacitor;

void init_preemphasis(ResistorCapacitor *filter, float tau, float sample_rate);
float apply_preemphasis(ResistorCapacitor *filter, float sample);

float hard_clip(float sample, float threshold);

typedef struct
{
	float b0, b1, b2;
	float a1, a2;
	float x1, x2;
	float y1, y2;
} Biquad;
void init_chebyshev_lpf(Biquad* filter, float cutoff_freq, float sample_rate, float ripple_db);
float biquad(Biquad *filter, float input);