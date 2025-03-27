#include "oscillator.h"

void init_oscillator(Oscillator *osc, float frequency, float sample_rate) {
	osc->phase = 0.0f;
	osc->phase_increment = (M_2PI * frequency) / sample_rate;
	osc->sample_rate = sample_rate;
}

void change_oscillator_frequency(Oscillator *osc, float frequency) {
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

void advance_oscillator(Oscillator *osc) {
	#if USE_NEON  // Use NEON if available
		float32x4_t v_phase = vdupq_n_f32(osc->phase);
		float32x4_t v_increment = vdupq_n_f32(osc->phase_increment);
		float32x4_t v_twopi = vdupq_n_f32(M_2PI);
	
		v_phase = vaddq_f32(v_phase, v_increment);
		uint32x4_t v_mask = vcgeq_f32(v_phase, v_twopi);  // Check if phase >= 2π
		float32x4_t v_wrapped = vsubq_f32(v_phase, v_twopi);
		v_phase = vbslq_f32(v_mask, v_wrapped, v_phase);
	
		osc->phase = vgetq_lane_f32(v_phase, 0);
	
	#else  // Scalar fallback if NEON is not available
		osc->phase += osc->phase_increment;
		if (osc->phase >= M_2PI) {
			osc->phase -= M_2PI;
		}
	#endif
	}