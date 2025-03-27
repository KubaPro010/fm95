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
typedef struct {
    Biquad section1;
    Biquad section2;
} LPF4;
void init_lpf(Biquad* filter, float sample_rate, float cutoff_freq, float Q);
void init_lpf4(LPF4* filter, float sample_rate, float cutoff_freq);
float apply_lpf4(LPF4* filter, float input);
float biquad(Biquad *filter, float input);