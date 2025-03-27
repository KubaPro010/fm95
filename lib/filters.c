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

void init_lpf(FIR *filter, float cutoff, int sample_rate) {
	float a = tanf(M_PI*cutoff/sample_rate);
	float a2 = a*a;
	float r, e;

	for(int i = 0; i < FIR_ORDER; i++) {
		r = sinf(M_PI*(2.0f*i+1.0f)/(4.0f*FIR_ORDER));
		e = a2+2.0f*a*r+1.0f;
		filter->A[i] = a2 / e;
		filter->d1[i] = 2.0f*(1.0f-a2)/e;
		filter->d2[i] = -(a2 - 2.0f * a * r + 1.0f) / e;
	}
}

float process_lpf(FIR *filter, float x) {
	float output;
	for(int i = 0; i < FIR_ORDER; i++) {
		filter->w0[i] = filter->d1[i]*filter->w1[i] + filter->d2[i]*filter->w2[i] + x;
        output = filter->A[i] * (filter->w0[i] + 2.0f * filter->w1[i] + filter->w2[i]);
        filter->w2[i] = filter->w1[i];
        filter->w1[i] = filter->w0[i];
	}
	return output;
}