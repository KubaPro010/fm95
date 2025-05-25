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
	double sample;
} MPXPowerMeasurement;

float dbr_to_deviation(float dbr);
float deviation_to_dbr(float deviation);

void init_modulation_power_measure(MPXPowerMeasurement *mpx, int sample_rate);
float measure_mpx(MPXPowerMeasurement *mpx, float deviation);