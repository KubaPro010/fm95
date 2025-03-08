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
    // Calculate intermediate values
    float omega = 2.0f * M_PI * cutoffFreq / sampleRate;
    float sn = sinf(omega);
    float cs = cosf(omega);
    float alpha = sn / (2.0f * qFactor);
    
    // Calculate coefficients
    float b0 = (1.0f - cs) * 0.5f;
    float b1 = 1.0f - cs;
    float b2 = (1.0f - cs) * 0.5f;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cs;
    float a2 = 1.0f - alpha;
    
    // Normalize by a0
    filter->b0 = b0 / a0;
    filter->b1 = b1 / a0;
    filter->b2 = b2 / a0;
    filter->a1 = a1 / a0;
    filter->a2 = a2 / a0;
    
    // Initialize state variables
    filter->x1 = 0.0f;
    filter->x2 = 0.0f;
    filter->y1 = 0.0f;
    filter->y2 = 0.0f;
}

void init_hpf(BiquadFilter* filter, float cutoffFreq, float qFactor, float sampleRate) {
    float omega = 2.0f * M_PI * cutoffFreq / sampleRate;
    float alpha = sinf(omega) / (2.0f * qFactor);
    float cosw = cosf(omega);
    
    float b0 = (1.0f + cosw) / 2.0f;
    float b1 = -(1.0f + cosw);
    float b2 = (1.0f + cosw) / 2.0f;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cosw;
    float a2 = 1.0f - alpha;
    
    // Normalize by a0
    filter->b0 = b0 / a0;
    filter->b1 = b1 / a0;
    filter->b2 = b2 / a0;
    filter->a1 = a1 / a0;
    filter->a2 = a2 / a0;
    
    // Initialize state variables
    filter->x1 = 0.0f;
    filter->x2 = 0.0f;
    filter->y1 = 0.0f;
    filter->y2 = 0.0f;
}

void init_bpf(BiquadFilter* filter, float centerFreq, float qFactor, float sampleRate) {
    float omega = 2.0f * M_PI * centerFreq / sampleRate;
    float alpha = sinf(omega) / (2.0f * qFactor);
    float cosw = cosf(omega);
    
    float b0 = alpha;
    float b1 = 0.0f;
    float b2 = -alpha;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cosw;
    float a2 = 1.0f - alpha;
    
    // Normalize by a0
    filter->b0 = b0 / a0;
    filter->b1 = b1 / a0;
    filter->b2 = b2 / a0;
    filter->a1 = a1 / a0;
    filter->a2 = a2 / a0;
    
    // Initialize state variables
    filter->x1 = 0.0f;
    filter->x2 = 0.0f;
    filter->y1 = 0.0f;
    filter->y2 = 0.0f;
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

float voltage_db_to_voltage(float db) {
    return powf(10.0f, db / 20.0f);
}

float power_db_to_voltage(float db) {
    return powf(10.0f, db / 10.0f);
}

float voltage_to_voltage_db(float linear) {
    return 20.0f * log10f(fmaxf(linear, 1e-10f)); // Avoid log(0)
}

float voltage_to_power_db(float linear) {
    return 10.0f * log10f(fmaxf(linear, 1e-10f)); // Avoid log(0)
}
