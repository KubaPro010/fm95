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

void tilt_init(TiltCorrectionFilter* filter, float cutoff_freq, float sample_rate) {
    // High-pass filter: H(z) = (1 - z^-1) / (1 - α*z^-1)
    float dt = 1.0f / sample_rate;
    float tau = 1.0f / (2.0f * M_PI * cutoff_freq);
    float alpha = tau / (tau + dt);
    
    filter->alpha = alpha;
    filter->x_prev = 0.0f;
    filter->y_prev = 0.0f;
}

float tilt(TiltCorrectionFilter* filter, float input) {
    // High-pass filter: y[n] = α*y[n-1] + (x[n] - x[n-1])
    float output = filter->alpha * filter->y_prev + (input - filter->x_prev);
    
    filter->x_prev = input;
    filter->y_prev = output;
    
    return output;
}
