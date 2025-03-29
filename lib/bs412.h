#pragma once
#include <math.h>

typedef struct
{
    int i;
    int sample_rate;
    float sample;
} MPXPowerMeasurement;

float dbr_to_deviation(float dbr);
float deviation_to_dbr(float deviation);

void init_modulation_power_measure(MPXPowerMeasurement *mpx, int sample_rate);
float measure_mpx(MPXPowerMeasurement *mpx, int deviation);