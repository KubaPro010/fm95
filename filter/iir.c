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
    // This filter is a first-order IIR low-shelf filter.
    // The difference equation is: y[n] = b0*x[n] + b1*x[n-1] - a1*y[n-1]
    // We simplify it to y[n] = x[n] - a1*y[n-1] which acts as a leaky integrator.
    
    // The "correction_strength" is our leaky factor. It is the pole of the filter.
    // A value close to 1.0 places the pole very close to the unit circle,
    // providing a large boost to low frequencies (and DC).
    
    if (correction_strength >= 1.0f) {
        correction_strength = 0.99999f; // Prevent instability
    }

    filter->b0 = 1.0f;
    filter->b1 = 0.0f;
    filter->a1 = -correction_strength; // The feedback coefficient

    // Reset filter state
    filter->x_prev = 0.0f;
    filter->y_prev = 0.0f;
}

float tilt(TiltCorrectionFilter* filter, float input_sample) {
    // Apply the difference equation: y[n] = b0*x[n] + b1*x[n-1] - a1*y[n-1]
    float output_sample = filter->b0 * input_sample + filter->b1 * filter->x_prev - filter->a1 * filter->y_prev;

    // Important: Prevent output from running away due to DC offset accumulation
    // This is a simple guard. If the filter becomes unstable or the output
    // grows too large, it gets reset. For square waves, the absolute value of the
    // output should not significantly exceed the absolute value of the input.
    if (fabsf(output_sample) > 2.0f * fabsf(input_sample) && fabsf(input_sample) > 0.001f) {
       // This condition indicates the filter state might be diverging. Resetting it.
       // You may need to adjust the '2.0f' factor based on your signal.
       filter->y_prev = 0; 
       output_sample = input_sample;
    }


    // Update the state for the next iteration
    filter->x_prev = input_sample;
    filter->y_prev = output_sample;
    
    return output_sample;
}