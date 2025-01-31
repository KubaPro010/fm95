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
    float b0, b1, b2;
    float a1, a2;
    float x1, x2;
    float y1, y2;
} BiquadFilter;
void init_lpf(BiquadFilter* filter, float cutoffFreq, float qFactor, float sampleRate);
void init_hpf(BiquadFilter* filter, float cutoffFreq, float qFactor, float sampleRate);
float apply_frequency_filter(BiquadFilter* filter, float input);

float hard_clip(float sample, float threshold);

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