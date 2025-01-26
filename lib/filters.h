#pragma once
#define FILTER_TAPS 256
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "constants.h"

typedef struct {
    float alpha;
    float prev_sample;
} ResistorCapacitor; 

void init_rc(ResistorCapacitor *pe, float alpha);
void init_rc_tau(ResistorCapacitor *pe, float tau, float sample_rate);
float apply_pre_emphasis(ResistorCapacitor *pe, float sample);

typedef struct {
    float coeffs[FILTER_TAPS];
    float delay[FILTER_TAPS];
    int index;
} FrequencyFilter;

void init_lpf(FrequencyFilter* filter, float cutoffFreq, float sampleRate);
float apply_freqeuncy_filter(FrequencyFilter* filter, float input);

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