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

void init_pll(PLL *pll, float freq, float loop_filter_bandwidth, float damping, int quadrature_mode, int sample_rate) {
	pll->phase = 0.0f;
	pll->freq = freq;
	pll->loop_filter_state = 0.0f;
	pll->kp = M_2PI * loop_filter_bandwidth;
	pll->ki = (4.0f*damping*damping) * pll->kp * pll->kp;
	pll->sample_rate = sample_rate;
	pll->quadrature_mode = quadrature_mode;
}

float apply_pll(PLL *pll, float ref_sample) {
	float phase_error;

	float vco_output = sinf(pll->phase);
	if (pll->quadrature_mode) vco_output = sinf(pll->phase + (M_PI / 2.0f));

	phase_error = ref_sample * vco_output;

	pll->loop_filter_state += pll->ki * phase_error / pll->sample_rate;
	float loop_output = pll->loop_filter_state + pll->kp * phase_error;

	float freq_adjustment = loop_output / M_2PI;
	float instantaneous_freq = pll->freq + freq_adjustment;

	pll->phase += M_2PI * instantaneous_freq / pll->sample_rate;

	while (pll->phase >= M_2PI) {
		pll->phase -= M_2PI;
	}
	while (pll->phase < 0.0f) {
		pll->phase += M_2PI;
	}

	return vco_output;
}