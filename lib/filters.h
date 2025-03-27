#pragma once

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "constants.h"
#include "optimization.h"
#include "oscillator.h"

#if USE_NEON
#define LPF_ORDER 12 // neon has to have divisable by 4
#else
#define LPF_ORDER 10
#endif

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
	float A[LPF_ORDER];
	float d1[LPF_ORDER];
	float d2[LPF_ORDER];
	float w0[LPF_ORDER];
	float w1[LPF_ORDER];
	float w2[LPF_ORDER];
} LPFFilter;
void init_lpf(LPFFilter *filter, float cutoff, int sample_rate);
float process_lpf(LPFFilter *filter, float x);
