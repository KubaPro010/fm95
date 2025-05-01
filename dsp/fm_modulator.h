#pragma once

#include "oscillator.h"

typedef struct
{
	float frequency;
	float deviation;
	float osc_phase;
	float sample_rate;
} FMModulator;

void init_fm_modulator(FMModulator *fm, float frequency, float deviation, float sample_rate);
float modulate_fm(FMModulator *fm, float sample);

typedef struct
{
	float deviation;
	Oscillator* osc;
} RefrencedFMModulator;
void init_refrenced_fm_modulator(RefrencedFMModulator *fm, Oscillator *osc, float deviation);
float refrenced_modulate_fm(RefrencedFMModulator *fm, float sample);
