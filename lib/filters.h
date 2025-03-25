#pragma once
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "constants.h"
#include "oscillator.h"

typedef struct
{
	float alpha;
	float prev_sample;
} ResistorCapacitor;

void init_preemphasis(ResistorCapacitor *filter, float tau, float sample_rate);
float apply_preemphasis(ResistorCapacitor *filter, float sample);

float hard_clip(float sample, float threshold);

typedef struct {
	float phase;
	float freq;
	float loop_filter_state;
	float kp;
	float ki;
	int sample_rate;
} PLL;
void init_pll(PLL *pll, float freq, float loop_filter_bandwidth, float damping, int sample_rate);
float apply_pll(PLL *pll, float ref_sample);