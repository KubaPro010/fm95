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

void init_lpf(Biquad* filter, float sample_rate, float cutoff_freq) {
    float omega = 2.0f * M_PI * cutoff_freq / sample_rate;
    float alpha = sinf(omega) / (2.0f * 0.707f);
    float cos_omega = cosf(omega);

	float norm = 1.0f / (1.0f + alpha);
	filter->b0 = (1.0f - cos_omega) * 0.5f * norm;
	filter->b1 = (1.0f - cos_omega) * norm;
	filter->b2 = filter->b0;
	filter->a1 = -2.0f * cos_omega * norm;
	filter->a2 = (1.0f - alpha) * norm;
    
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