#pragma once
#include <math.h>

typedef struct
{
    int i;
    int sample_rate;
    float sample;
} MPXPowerMeasurement;

void init_modulation_power_measure(MPXPowerMeasurement *mpx, int sample_rate);

float measure_mpx(MPXPowerMeasurement *mpx, int deviation);
