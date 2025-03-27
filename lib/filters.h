#pragma once

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    #include <arm_neon.h>
    #define USE_NEON 1
#else
    #define USE_NEON 0
#endif

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "constants.h"
#include "oscillator.h"

#define LPF_ORDER 10

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
