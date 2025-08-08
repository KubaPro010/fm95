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
    // Allow correction_strength > 1.0 for aggressive correction
    filter->alpha = 0.9999f;  // Fixed time constant for DC tracking
    filter->gain = alpha;  // Separate gain parameter
    filter->dc_estimate = 0.0f;
}

float tilt_correct(TiltCorrectionFilter* filter, float input) {
    // Track the baseline/DC level
    filter->dc_estimate = filter->alpha * filter->dc_estimate + (1.0f - filter->alpha) * input;
    
    // Calculate the deviation from baseline
    float deviation = input - filter->dc_estimate;
    
    // Apply correction gain and add back to baseline
    float output = filter->dc_estimate + deviation * filter->gain;
    
    return output;
}
