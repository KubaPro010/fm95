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
void init_bpf(BiquadFilter* filter, float centerFreq, float qFactor, float sampleRate);
float apply_frequency_filter(BiquadFilter* filter, float input);

float hard_clip(float sample, float threshold);
float voltage_db_to_voltage(float db);
float power_db_to_voltage(float db);
float voltage_to_voltage_db(float linear);
float voltage_to_power_db(float linear);

typedef struct {
    float threshold;
    float ratio;
    float knee;
    float makeup_gain;
    float attack;
    float release;
    float sample_rate;
    float gainReduction;
    float rmsEnv;
    float rmsTime;
} Compressor;
void init_compressor(Compressor *compressor, float threshold, float ratio, float knee, float makeup_gain, float attack, float release, float rmsTime, float sample_rate);
float peak_compress(Compressor *compressor, float sample);
float rms_compress(Compressor *compressor, float sample);

typedef struct {
    float threshold;
    float ratio;
    float knee;
    float makeup_gain;
    float attack;
    float release;
    float sample_rate;
    float gainReduction;
    float rmsEnv;
    float rmsEnv2;
    float rmsTime;
} StereoCompressor;
void init_compressor_stereo(StereoCompressor *compressor, float threshold, float ratio, float knee, float makeup_gain, float attack, float release, float rmsTime, float sample_rate);
float peak_compress_stereo(StereoCompressor *compressor, float l, float r, float *output_r);
float rms_compress_stereo(StereoCompressor *compressor, float l, float r, float *output_r);
