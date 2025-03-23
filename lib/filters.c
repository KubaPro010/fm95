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

void init_pll(PLL *pll, float output_freq, float reference_freq, float loop_filter_bandwidth, int quadrature_mode, int sample_rate) {
    pll->phase = 0.0f;
    pll->freq = output_freq;
    pll->ref_freq = reference_freq;
    pll->loop_filter_state = 0.0f;
    pll->kp = 2.0f * M_PI * loop_filter_bandwidth;
    pll->ki = 0.25f * pll->kp * pll->kp;
    pll->sample_rate = sample_rate;
    pll->quadrature_mode = quadrature_mode;
}

float apply_pll(PLL *pll, float ref_sample, float input_sample) {
    float phase_error;
    
    float output = sinf(pll->phase);
    
    if (pll->quadrature_mode) {
        output = sinf(pll->phase + (M_PI / 2.0f));
    }
    
    phase_error = ref_sample * input_sample;
    
    pll->loop_filter_state += pll->ki * phase_error / pll->sample_rate;
    float loop_output = pll->loop_filter_state + pll->kp * phase_error;
    
    float freq_adjustment = loop_output / (2.0f * M_PI);
    float instantaneous_freq = pll->freq + freq_adjustment;
    
    pll->phase += 2.0f * M_PI * instantaneous_freq / pll->sample_rate;
    
    while (pll->phase >= 2.0f * M_PI) {
        pll->phase -= 2.0f * M_PI;
    }
    while (pll->phase < 0.0f) {
        pll->phase += 2.0f * M_PI;
    }
    
    return output;
}