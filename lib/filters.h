#pragma once
#include <string.h>
#include <stdlib.h>
#include <math.h>
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

void init_rc_lpf(ResistorCapacitor *filter, float cutoff, float sample_rate);
float apply_rc_lpf(ResistorCapacitor *filter, float sample);