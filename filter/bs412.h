#pragma once

#ifdef DEBUG
#define BS412_DEBUG
#endif

#include <math.h>
#ifdef BS412_DEBUG
#include "../lib/debug.h"
#endif

typedef struct
{
	int sample_counter;
	int sample_rate;
	float target;
	float attack;
	float release;
	float gain;
	double sample;
} BS412Compressor;

float dbr_to_deviation(float dbr);
float deviation_to_dbr(float deviation);

void init_bs412(BS412Compressor *mpx, float target_power, float attack, float release, int sample_rate);
float bs412_compress(BS412Compressor *mpx, float deviation);