#pragma once

#ifdef DEBUG
#define BS412_DEBUG
#endif

#include <math.h>
#include <string.h>
#include <stdint.h>
#ifdef BS412_DEBUG
#include "../lib/debug.h"
#endif

typedef struct
{
	uint32_t mpx_deviation;
	uint32_t average_counter;
	uint32_t sample_rate;
	float target;
	float attack;
	float release;
	float max;
	float gain;
	double average;
} BS412Compressor;

float dbr_to_deviation(float dbr);
float deviation_to_dbr(float deviation);

void init_bs412(BS412Compressor *mpx, uint32_t mpx_deviation, float target_power, float attack, float release, float max, uint32_t sample_rate);
float bs412_compress(BS412Compressor *mpx, float average);