#include "filters.h"

void init_preemphasis(ResistorCapacitor *filter, float tau, float sample_rate) {
	filter->prev_sample = 0.0f;
	filter->alpha = expf(-1 / (tau*sample_rate));
	filter->gain = 1.0f / sqrtf(1.0f - filter->alpha);
}
float apply_preemphasis(ResistorCapacitor *filter, float sample) {
	float out = (sample - filter->alpha * filter->prev_sample) * filter->gain;
	filter->prev_sample = sample;
	return out;
}

float hard_clip(float sample, float threshold) {
	return fmaxf(-threshold, fminf(threshold, sample));
}

int init_chebyshev_lpf(Biquad* filter, float sample_rate, float cutoff_freq, float ripple_db, int order) {
    float eps = sqrt(pow(10, ripple_db/10.0) - 1.0);
    float omega_c = 2.0f * M_PI * cutoff_freq / sample_rate;
    
    float poles[10];
    for (int k = 1; k <= order; k++) {
        float phi = ((2*k - 1) * M_PI) / (2.0 * order);
        float real = -sinh((1.0/order) * asinh(1.0/eps)) * sin(phi);
        float imag = cosh((1.0/order) * asinh(1.0/eps)) * cos(phi);
        
        float s_real = real * omega_c;
        float s_imag = imag * omega_c;
        
        poles[k-1] = s_real;
    }
    
    float alpha = tan(omega_c/2.0);
    float a = 1.0f + alpha;
    
    filter->b0 = alpha * alpha / (a * a);
    filter->b1 = 2.0f * filter->b0;
    filter->b2 = filter->b0;
    
    filter->a1 = 2.0f * (alpha * alpha - 1.0f) / (a * a);
    filter->a2 = (1.0f - alpha) / (1.0f + alpha);
    
    filter->x1 = filter->x2 = 0.0f;
    filter->y1 = filter->y2 = 0.0f;
}

float biquad(Biquad *filter, float input) {
    float output = filter->b0 * input 
                 + filter->b1 * filter->x1 
                 + filter->b2 * filter->x2
                 - filter->a1 * filter->y1 
                 - filter->a2 * filter->y2;
    
    filter->x2 = filter->x1;
    filter->x1 = input;
    
    filter->y2 = filter->y1;
    filter->y1 = output;
    
    return output;
}