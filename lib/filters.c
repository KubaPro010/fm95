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

void init_butterworth_lpf(Biquad *filter, float cutoff_freq, float sample_rate) {
    float omega =  M_2PI * cutoff_freq / sample_rate;
    float Q = 1.0f / sqrtf(2.0f);
    float alpha = sinf(omega) / (2.0f * Q);
    
    float b0 = (1.0f - cosf(omega)) / 2.0f;
    float b1 = 1.0f - cosf(omega);
    float b2 = (1.0f - cosf(omega)) / 2.0f;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cosf(omega);
    float a2 = 1.0f - alpha;
    
    filter->b0 = b0 / a0;
    filter->b1 = b1 / a0;
    filter->b2 = b2 / a0;
    filter->a1 = a1 / a0;
    filter->a2 = a2 / a0;
    
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