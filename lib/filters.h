#pragma once
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "constants.h"

typedef struct
{
    float alpha;
    float prev_sample;
} ResistorCapacitor;

void init_preemphasis(ResistorCapacitor *filter, float tau, float sample_rate);
float apply_preemphasis(ResistorCapacitor *filter, float sample);

float hard_clip(float sample, float threshold);