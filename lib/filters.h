#pragma once
#include <string.h>
#include <stdlib.h>
#include "constants.h"
#include "oscillator.h"

#define FILTER_LEN 51

typedef struct
{
	float alpha;
	float prev_sample;
} ResistorCapacitor;

void init_preemphasis(ResistorCapacitor *filter, float tau, float sample_rate);
float apply_preemphasis(ResistorCapacitor *filter, float sample);

float hard_clip(float sample, float threshold);

typedef struct filters
{
	float filter[FILTER_LEN];
	int filter_idx;
} FIRFilter;
void init_bpf(FIRFilter *bpf, float start, float end);
void init_lpf(FIRFilter *lpf, float freq);
float fir_filter(FIRFilter *fir, float sample);