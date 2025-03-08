#pragma once
#define FILTER_TAPS 256
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

typedef struct {
    // https://www.earlevel.com/main/2012/11/26/biquad-c-source-code/
    float a0, a1, a2;
    float b1, b2;
    float z1, z2;
} BiquadFilter;
void init_lpf(BiquadFilter* filter, float cutoffFreq, float qFactor, float sampleRate);
float apply_biquad(BiquadFilter* filter, float input);

typedef struct
{
    int i;
    int ratio;
    BiquadFilter lpf;
} Upsampler;
void init_upsampler(Upsampler* up, int ratio, float sample_rate);
float upsample(Upsampler *up, float sample);

float hard_clip(float sample, float threshold);
float voltage_db_to_voltage(float db);
float power_db_to_voltage(float db);
float voltage_to_voltage_db(float linear);
float voltage_to_power_db(float linear);