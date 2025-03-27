#pragma once
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "constants.h"
#include "oscillator.h"

#define FIR_ORDER 21

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
	float A[FIR_ORDER];
	float d1[FIR_ORDER];
	float d2[FIR_ORDER];
	float w0[FIR_ORDER];
	float w1[FIR_ORDER];
	float w2[FIR_ORDER];
} FIR;
void init_lpf(FIR *filter, float cutoff, int sample_rate);
float process_lpf(FIR *filter, float x);