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

float hard_clip(float sample, float threshold) {
	return fmaxf(-threshold, fminf(threshold, sample));
}

void init_bpf(FIRFilter *bpf, float start, float end) {
	int m = FILTER_LEN - 1;
	float sum = 0.0f;

	for(int n = 0; n < FILTER_LEN; n++) {
		float x = n-m/2.0f;
		float h1 = sincf(2.0f*start*x) * (0.54 - 0.46 * cosf(M_2PI * n / m));
		float h2 = sincf(2.0f*end*x) * (0.54 - 0.46 * cosf(M_2PI * n / m));
		bpf->filter[n] = h1-h2;
		sum += bpf->filter[n];
	}

	for(int n = 0; n < FILTER_LEN; n++) {
		bpf->filter[n] /= sum;
	}
}
void init_lpf(FIRFilter *lpf, float freq) {
	int m = FILTER_LEN - 1;
	float sum = 0.0f;

	for(int n = 0; n < FILTER_LEN; n++) {
		float x = n-m/2.0f;
		lpf->filter[n] = sincf(2.0f*freq*x) * (0.54 - 0.46 * cosf(M_2PI * n / m));
		sum += lpf->filter[n];
	}

	for(int n = 0; n < FILTER_LEN; n++) {
		lpf->filter[n] /= sum;
	}
}

float fir_filter(FIRFilter *fir, float sample) {
    float out = 0.0f;
    for(int i = 0; i < FILTER_LEN; i++) {
        out += fir->filter[i] * sample;
    }
    fir->filter_idx++;
    if(fir->filter_idx == FILTER_LEN) fir->filter_idx = 0;
    return out;
}