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

void init_pll(PLL *pll, int interpolation, int decimation, float freq, float loop_filter_bandwidth, int quadrature_mode, int sample_rate) {
	pll->phase = 0.0f;
	pll->freq = freq;
	pll->loop_filter_state = 0.0f;
	pll->kp = M_2PI * loop_filter_bandwidth;
	pll->ki = 0.25f * pll->kp * pll->kp;
	pll->sample_rate = sample_rate;
	pll->quadrature_mode = quadrature_mode;
	pll->last_output = 0.0f;
	pll->interpolation = interpolation;
	pll->decimation = decimation;
}

float apply_pll(PLL *pll, float ref_sample) {
	float phase_error;

	float vco_phase = pll->phase;
	if(pll->quadrature_mode) vco_phase += M_PI/2.0f;

	float vco_output = sinf(pll->phase);
	if (pll->quadrature_mode) vco_output = sinf(pll->phase + (M_PI / 2.0f)); // 90 degrees

	phase_error = ref_sample * pll->last_output;

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

	pll->last_output = sinf((vco_phase*pll->interpolation)/pll->decimation);

	return pll->last_output;
}