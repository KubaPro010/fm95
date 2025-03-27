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

void init_chebyshev_lpf(Biquad* filter, float cutoff_freq, float sample_rate, float ripple_db) {
	float Wn = M_2PI * cutoff_freq / sample_rate;
	float eps = sqrtf(powf(10.0f, ripple_db / 10.0f) - 1.0f);
	int n = 2;
	float gamma = (2.0f * eps) / (n + 1.0f);
	float phi = acoshf(1.0f / gamma);
	float sin_phi = sinhf(phi / n);
	float cos_phi = coshf(phi / n);
	float alpha = sin_phi * cosf(Wn);
	float beta = sin_phi * sinf(Wn);

	float d = 1.0f + 2.0f * alpha + (alpha * alpha + beta * beta);

	filter->b0 = (1.0f - (alpha * alpha + beta * beta)) / d;
	filter->b1 = 2.0f * ((alpha * alpha + beta * beta) - 1.0f) / d;
	filter->b2 = (1.0f - 2.0f * alpha + (alpha * alpha + beta * beta)) / d;

	filter->a1 = -2.0f * (1.0f + 2.0f * alpha - (alpha * alpha + beta * beta)) / d;
	filter->a2 = (1.0f - 2.0f * alpha + (alpha * alpha + beta * beta)) / d;

	filter->x1 = filter->x2 = 0.0f;
	filter->y1 = filter->y2 = 0.0f;
}

float biquad(Biquad *filter, float input) {
    float output = filter->b0 * input 
                 + filter->b1 * filter->x1 
                 + filter->b2 * filter->x2
                 - filter->a1 * filter->y1 
                 - filter->a2 * filter->y2;
    
    filter->x2 = filter->x1;
    filter->x1 = input;
    
    filter->y2 = filter->y1;
    filter->y1 = output;
    
    return output;
}