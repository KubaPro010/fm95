#include "filters.h"

void init_preemphasis(ResistorCapacitor *filter, float tau, float sample_rate) {
    filter->prev_sample = 0.0f;
    filter->alpha = exp(-1 / (tau*sample_rate));
}
float apply_preemphasis(ResistorCapacitor *filter, float sample) {
    float out = sample-filter->alpha*filter->prev_sample;
    filter->prev_sample = sample;
    return out;
}

float hard_clip(float sample, float threshold) {
    return fmaxf(-threshold, fminf(threshold, sample));
}

void init_pll(PLL *pll, float reference_frequency, float target_frequency, float sample_rate) {
    pll->reference_frequency = reference_frequency;
    pll->target_frequency = target_frequency;
    pll->multiplier = target_frequency / reference_frequency;
    pll->sample_rate = sample_rate;
    
    pll->phase_error = 0.0f;
    pll->loop_filter = 0.0f;
    pll->vco_phase = 0.0f;
    pll->vco_frequency = target_frequency;
    
    pll->loop_gain = 0.01f;
    pll->filter_coefficient = 0.1f;
}

float update_pll(PLL *pll, float reference_signal) {
    float vco_divided_phase = fmodf(pll->vco_phase / pll->multiplier, M_2PI);
    float reference_phase = acosf(reference_signal); // Extract phase from reference signal
    
    pll->phase_error = sinf(reference_phase - vco_divided_phase);
    
    pll->loop_filter = pll->loop_filter * (1.0f - pll->filter_coefficient) + 
                       pll->phase_error * pll->filter_coefficient * pll->loop_gain;
    
    pll->vco_frequency = pll->target_frequency + pll->loop_filter;
    
    float phase_increment = (M_2PI * pll->vco_frequency) / pll->sample_rate;
    pll->vco_phase = fmodf(pll->vco_phase + phase_increment, M_2PI * pll->multiplier);
    
    return sinf(pll->vco_phase);
}