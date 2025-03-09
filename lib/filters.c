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

float hard_clip_fast(float sample, float threshold) {
    // Branchless clipping
    return fmaxf(-threshold, fminf(threshold, sample));
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
