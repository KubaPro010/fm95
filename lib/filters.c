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
    float cutoffNorm = cutoffFreq / sampleRate;
    float K = tanf(M_PI * cutoffNorm);
    float norm = 1.0f/(1.0f+K/qFactor+K*K);
    filter->a0 = K*K*norm;
    filter->a1 = 2.0f*K*K*norm;
    filter->a2 = K*K*norm;
    filter->b1 = 2.0f*(K*K-1.0f)*norm;
    filter->b2 = (1.0f-K/qFactor+K*K)*norm;

    filter->z1 = 0.0f;
    filter->z2 = 0.0f;
}

float apply_biquad(BiquadFilter* filter, float input) {
    float out = input*filter->a0+filter->z1;
    filter->z1 = input*filter->a1+filter->z2-filter->b1*out;
    filter->z2 = input*filter->a2-filter->b2*out;
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
