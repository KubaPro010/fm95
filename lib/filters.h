#pragma once
#define FILTER_TAPS 256
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "constants.h"

typedef struct {
    float b0, b1, b2;
    float a1, a2;
    float x1, x2;
    float y1, y2;
} BiquadFilter;

void init_preemphasis(BiquadFilter *filter, float tau, float sample_rate);
float apply_preemphasis(BiquadFilter *filter, float input);

typedef struct {
    float coeffs[FILTER_TAPS];
    float delay[FILTER_TAPS];
    int index;
} FrequencyFilter;

void init_lpf(FrequencyFilter* filter, float cutoffFreq, float sampleRate);
void init_hpf(FrequencyFilter* filter, float cutoffFreq, float sampleRate);
float apply_frequency_filter(FrequencyFilter* filter, float input);

float hard_clip(float sample, float threshold);
float soft_clip(float sample, float threshold);

typedef struct {
    float *buffer;
    int write_idx;  // Write position
    int read_idx;   // Read position
    int size;       // Total buffer size
    int delay;      // Delay in samples
} DelayLine;

void init_delay_line(DelayLine *delay_line, int max_delay);
void set_delay_line(DelayLine *delay_line, int new_delay);
float delay_line(DelayLine *delay_line, float in);
void exit_delay_line(DelayLine *delay_line);