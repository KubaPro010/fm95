#include "oscillator.h"

void init_oscillator(Oscillator *osc, float frequency, float sample_rate) {
	osc->phase = 0.0f;
	osc->phase_increment = (M_2PI * frequency) / sample_rate;
	osc->sample_rate = sample_rate;
}

inline void change_oscillator_frequency(Oscillator *osc, float frequency) {
	osc->phase_increment = (M_2PI * frequency) / osc->sample_rate;
}

float get_oscillator_sin_sample(Oscillator *osc) {
	float sample = sinf(osc->phase);
	advance_oscillator(osc);
	return sample;
}

float get_oscillator_cos_sample(Oscillator *osc) {
	float sample = cosf(osc->phase);
	advance_oscillator(osc);
	return sample;
}

float get_oscillator_sin_multiplier_ni(Oscillator *osc, float multiplier) {
	float new_phase = osc->phase * multiplier;
	new_phase -= (new_phase >= M_2PI) ? M_2PI : 0.0f;
	return sinf(new_phase);
}

float get_oscillator_cos_multiplier_ni(Oscillator *osc, float multiplier) {
	float new_phase = osc->phase * multiplier;
	new_phase -= (new_phase >= M_2PI) ? M_2PI : 0.0f;
	return cosf(new_phase);
}

inline void advance_oscillator(Oscillator *osc) {
	osc->phase += osc->phase_increment;
	if (osc->phase >= M_2PI) osc->phase -= M_2PI;
}