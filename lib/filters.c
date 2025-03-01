#include "filters.h"

void init_preemphasis(ResistorCapacitor *filter, float tau, float sample_rate) {
    filter->prev_sample = 0.0f;
    filter->alpha = exp(-1 / (tau*sample_rate));
}
float apply_preemphasis(ResistorCapacitor *filter, float sample) {
    float out = sample-filter->alpha*filter->prev_sample;
    filter->prev_sample = sample;
    return out;
}

void init_lpf(BiquadFilter* filter, float cutoffFreq, float qFactor, float sampleRate) {
    float x = (cutoffFreq * M_2PI) / sampleRate;
    float sinX = sin(x);
    float y = sinX / (qFactor*2.0f);
    float cosX = cos(x);
    float z = (1.0f-cosX)/2.0f;

    float _a0 = y + 1.0f;
    float _a1 = cosX * -2.0f;
    float _a2 = 1.0f - y;
    float _b0 = z;
    float _b1 = 1.0f - cosX;
    float _b2 = z;

    filter->y2 = 0;
    filter->y1 = 0;
    filter->x2 = 0;
    filter->x1 = 0;
    filter->b0 = _b0/_a0;
    filter->b1 = _b1/_a0;
    filter->b2 = _b2/_a0;
    filter->a1 = -_a1/_a0;
    filter->a2 = -_a2/_a0;
}

void init_hpf(BiquadFilter* filter, float cutoffFreq, float qFactor, float sampleRate) {
    float x = (cutoffFreq * M_2PI) / sampleRate;
    float sinX = sin(x);
    float y = sinX / (qFactor*2.0f);
    float cosX = cos(x);
    float z = (1.0f-cosX)/2.0f;

    float _a0 = y + 1.0f;
    float _a1 = cosX * -2.0f;
    float _a2 = 1.0f - y;
    float _b0 = 1.0f - z;
    float _b1 = cosX * -2.0f;
    float _b2 = 1.0f - z;

    filter->y2 = 0;
    filter->y1 = 0;
    filter->x2 = 0;
    filter->x1 = 0;
    filter->b0 = _b0/_a0;
    filter->b1 = _b1/_a0;
    filter->b2 = _b2/_a0;
    filter->a1 = -_a1/_a0;
    filter->a2 = -_a2/_a0;
}

float apply_frequency_filter(BiquadFilter* filter, float input) {
    float out = input*filter->b0+filter->x1*filter->b1+filter->x2*filter->b2+filter->y1*filter->a1+filter->y2*filter->a2;
    filter->y2 = filter->y1;
    filter->y1 = out;
    filter->x2 = filter->x1;
    filter->x1 = input;
    return out;
}

float hard_clip(float sample, float threshold) {
    if (sample > threshold) {
        return threshold;  // Clip to the upper threshold
    } else if (sample < -threshold) {
        return -threshold;  // Clip to the lower threshold
    } else {
        return sample;  // No clipping
    }
}