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

void tilt_init(TiltCorrectionFilter* filter, float alpha) {
    // Leaky integrator for DC estimation: dc[n] = alpha*dc[n-1] + (1-alpha)*x[n]
    // Tilt correction: y[n] = x[n] - dc[n]
    
    if (correction_strength >= 1.0f) {
        correction_strength = 0.99999f;
    }
    if (correction_strength < 0.0f) {
        correction_strength = 0.0f;
    }
    
    filter->alpha = alpha;  // Leaky integrator coefficient
    filter->dc_estimate = 0.0f;          // Running DC estimate
}

float tilt_correct(TiltCorrectionFilter* filter, float input) {
    // Update DC estimate using leaky integrator
    filter->dc_estimate = filter->alpha * filter->dc_estimate + (1.0f - filter->alpha) * input;
    
    // Remove estimated DC component
    float output = input - filter->dc_estimate;
    
    return output;
}
