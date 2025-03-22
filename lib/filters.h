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
    float phase_error;
    float loop_filter;
    float vco_phase;
    float vco_frequency;

    float reference_frequency;
    float target_frequency;
    float multiplier;
    float sample_rate;
    float loop_gain;
    float filter_coefficient;
} PLL;

void init_pll(PLL *pll, float reference_frequency, float target_frequency, float sample_rate);
float update_pll(PLL *pll, float reference_signal);