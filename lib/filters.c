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
    
    float damping = 0.707;
    pll->kp = 2.0f * damping * loop_filter_bandwidth;
    pll->ki = loop_filter_bandwidth * loop_filter_bandwidth;
    
    pll->sample_rate = sample_rate;
    pll->quadrature_mode = quadrature_mode;
}

float apply_pll(PLL *pll, float ref_sample, float input_sample) {
    float phase_error;
    if (pll->quadrature_mode) {
        phase_error = ref_sample * sinf(pll->phase) - input_sample * cosf(pll->phase);
    } else {
        phase_error = ref_sample * input_sample * sinf(pll->phase);
    }
    
    float filter_output = pll->kp * phase_error + pll->loop_filter_state;
    pll->loop_filter_state += pll->ki * phase_error * (1/pll->sample_rate);
    
    pll->freq = pll->freq + filter_output;
    
    pll->phase += M_2PI * pll->freq * (1.0f/pll->sample_rate);
    if (pll->phase > M_2PI) {
        pll->phase -= M_2PI;
    }
    
    return cosf(pll->phase);
}