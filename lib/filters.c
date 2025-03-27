#include "filters.h"

void init_preemphasis(ResistorCapacitor *filter, float tau, float sample_rate) {
	filter->prev_sample = 0.0f;
	filter->alpha = expf(-1 / (tau*sample_rate));
	filter->gain = 1.0f / sqrtf(1.0f - filter->alpha);
}
float apply_preemphasis(ResistorCapacitor *filter, float sample) {
	float out = (sample - filter->alpha * filter->prev_sample) * filter->gain;
	filter->prev_sample = sample;
	return out;
}

float hard_clip(float sample, float threshold) {
	return fmaxf(-threshold, fminf(threshold, sample));
}

void init_lpf(Biquad* filter, float sample_rate, float cutoff_freq, float Q) {
    float omega = 2.0f * M_PI * cutoff_freq / sample_rate;
    float alpha = sinf(omega) / (2.0f * Q);
    float cos_omega = cosf(omega);

    float norm = 1.0f / (1.0f + alpha);
    filter->b0 = (1.0f - cos_omega) * 0.5f * norm;
    filter->b1 = (1.0f - cos_omega) * norm;
    filter->b2 = filter->b0;
    filter->a1 = -2.0f * cos_omega * norm;
    filter->a2 = (1.0f - alpha) * norm;
    
    filter->x1 = filter->x2 = 0.0f;
    filter->y1 = filter->y2 = 0.0f;
}

void init_lpf4(LPF4* filter, float sample_rate, float cutoff_freq) {
    float Q1 = 1.0f / (2.0f * cosf(M_PI / 8.0f));
    init_lpf(&filter->section1, sample_rate, cutoff_freq, Q1);

    float Q2 = 1.0f / (2.0f * cosf(3.0f * M_PI / 8.0f));
    init_lpf(&filter->section2, sample_rate, cutoff_freq, Q2);
}

float apply_lpf4(LPF4* filter, float input) {
    float output = biquad(&filter->section1, input);
    output = biquad(&filter->section2, output);
    return output;
}