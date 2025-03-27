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

void init_lpf(LPFFilter *filter, float cutoff, int sample_rate) {
	float a = tanf(M_PI * cutoff / sample_rate);
	float a2 = a * a;
	float r, e;

	for (int i = 0; i < LPF_ORDER; i++) {
		r = sinf(M_PI * (2.0f * i + 1.0f) / (4.0f * LPF_ORDER));
		e = a2 + 2.0f * a * r + 1.0f;
		float inv_e = 1.0f / e;

		filter->A[i]  = a2 * inv_e;
		filter->d1[i] = 2.0f * (1.0f - a2) * inv_e;
		filter->d2[i] = -(a2 - 2.0f * a * r + 1.0f) * inv_e;
	}
}

float process_lpf(LPFFilter *filter, float x) {
    float y = x;
    for (int i = 0; i < LPF_ORDER; i++) {
        float w0_new = filter->d1[i] * filter->w1[i] + filter->d2[i] * filter->w2[i] + y;
        y = filter->A[i] * (w0_new + 2.0f * filter->w1[i] + filter->w2[i]);
        filter->w2[i] = filter->w1[i];
        filter->w1[i] = w0_new;
    }
    return y;
}