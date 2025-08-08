#include "iir.h"

void init_preemphasis(ResistorCapacitor *filter, float tau, float sample_rate, float ref_freq) {
    float dt = 1.0f / sample_rate;
    filter->alpha = tau / (tau + dt);

    float omega = M_2PI * ref_freq / sample_rate;
    filter->gain = 1.0f / sqrtf(1.0f + filter->alpha * filter->alpha - 2.0f * filter->alpha * cosf(omega));

    filter->prev_sample = 0.0f;
}
inline float apply_preemphasis(ResistorCapacitor *filter, float sample) {
	float out = (sample - filter->alpha * filter->prev_sample) * filter->gain;
	filter->prev_sample = sample;
	return out;
}

void tilt_init(TiltCorrectionFilter* filter, float correction_strength) {
    filter->alpha = 0.9999f;
    filter->gain = correction_strength;  // Can be > 1.0
    filter->x_prev = 0.0f;
    filter->y_prev = 0.0f;
}

float tilt(TiltCorrectionFilter* filter, float input) {
    // High-pass filter
    float hp_out = filter->alpha * (filter->y_prev + input - filter->x_prev);
    
    // Apply gain and add back to original
    float output = input + filter->gain * hp_out;
    
    filter->x_prev = input;
    filter->y_prev = hp_out;
    
    return output;
}