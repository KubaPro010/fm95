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

void init_rc_lpf(ResistorCapacitor *filter, float cutoff, float sample_rate) {
	filter->prev_sample = 0.0f;
	float dt = 1.0f/sample_rate;
	float rc = 1.0f/(cutoff*M_2PI);
	filter->alpha = dt/(rc+dt);
}
float apply_rc_lpf(ResistorCapacitor *filter, float sample) {
	float out = filter->prev_sample+(filter->alpha*(sample-filter->prev_sample));
	filter->prev_sample = sample;
	return out;
}