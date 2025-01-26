#include "filters.h"

void init_rc(ResistorCapacitor *rc, float alpha) {
    rc->prev_sample = 0.0f;
    rc->alpha = alpha;
}

void init_rc_tau(ResistorCapacitor *rc, float tau, float sample_rate) {
    rc->prev_sample = 0.0f;
    rc->alpha = exp(-1 / (tau * sample_rate));
}

float apply_pre_emphasis(ResistorCapacitor *rc, float sample) {
    float audio = sample-rc->alpha*rc->prev_sample;
    rc->prev_sample = audio;
    return audio;
}

void init_lpf(FrequencyFilter* filter, float cutoffFreq, float sampleRate) {
    float nyquist = sampleRate / 2.0f;
    float fc = cutoffFreq / nyquist;
    
    // Blackman window for sharp transition
    for (int n = 0; n < FILTER_TAPS; n++) {
        float m = n - (FILTER_TAPS - 1.0f) / 2.0f;
        
        // Sinc function
        float sinc = (m == 0) ? 1.0f : sinf(PI * m * fc) / (PI * m);
        
        // Blackman window
        float window = 0.42f - 0.5f * cosf(2.0f * PI * n / (FILTER_TAPS - 1)) 
                     + 0.08f * cosf(4.0f * PI * n / (FILTER_TAPS - 1));
        
        filter->coeffs[n] = sinc * window;
    }
    
    // Normalize
    float sum = 0;
    for (int i = 0; i < FILTER_TAPS; i++) sum += filter->coeffs[i];
    for (int i = 0; i < FILTER_TAPS; i++) filter->coeffs[i] /= sum;
    
    // Clear delay line
    memset(filter->delay, 0, sizeof(filter->delay));
    filter->index = 0;
}

void init_hpf(FrequencyFilter* filter, float cutoffFreq, float sampleRate) {
    float nyquist = sampleRate / 2.0f;
    float fc = cutoffFreq / nyquist;
    
    // Blackman window for sharp transition
    for (int n = 0; n < FILTER_TAPS; n++) {
        float m = n - (FILTER_TAPS - 1.0f) / 2.0f;
        
        // Sinc function
        float sinc = (m == 0) ? -1.0f : sinf(PI * m * fc) / (PI * m);
        
        // Blackman window
        float window = 0.42f - 0.5f * cosf(2.0f * PI * n / (FILTER_TAPS - 1)) 
                     + 0.08f * cosf(4.0f * PI * n / (FILTER_TAPS - 1));
        
        filter->coeffs[n] = sinc * window;
    }

    filter->coeffs[FILTER_TAPS/2] += 1.0f;
    
    // Normalize
    float sum = 0;
    for (int i = 0; i < FILTER_TAPS; i++) sum += filter->coeffs[i];
    for (int i = 0; i < FILTER_TAPS; i++) filter->coeffs[i] /= sum;
    
    // Clear delay line
    memset(filter->delay, 0, sizeof(filter->delay));
    filter->index = 0;
}

float apply_freqeuncy_filter(FrequencyFilter* filter, float input) {
    // Shift delay line
    filter->delay[filter->index] = input;
    
    // Compute output
    float output = 0;
    int j = filter->index;
    for (int i = 0; i < FILTER_TAPS; i++) {
        output += filter->coeffs[i] * filter->delay[j];
        j = (j + 1) % FILTER_TAPS;
    }
    
    // Update index
    filter->index = (filter->index + FILTER_TAPS - 1) % FILTER_TAPS;
    
    return output;
}

void init_delay_line(DelayLine *delay_line, int max_delay) {
    delay_line->buffer = (float *)calloc(max_delay, sizeof(float));
    delay_line->size = max_delay;
    delay_line->write_idx = 0;
    delay_line->read_idx = 0;
    delay_line->delay = 0;
}

void set_delay_line(DelayLine *delay_line, int new_delay) {
    if (new_delay >= delay_line->size) {
        new_delay = delay_line->size - 1;
    }
    if (new_delay < 0) {
        new_delay = 0;
    }
    
    delay_line->delay = new_delay;
    delay_line->read_idx = (delay_line->write_idx - new_delay + delay_line->size) % delay_line->size;
}

float delay_line(DelayLine *delay_line, float in) {
    float out;
    
    // Read the delayed sample
    out = delay_line->buffer[delay_line->read_idx];
    
    // Write the new sample
    delay_line->buffer[delay_line->write_idx] = in;
    
    // Update indices
    delay_line->write_idx = (delay_line->write_idx + 1) % delay_line->size;
    delay_line->read_idx = (delay_line->read_idx + 1) % delay_line->size;
    
    return out;
}

void exit_delay_line(DelayLine *delay_line) {
    free(delay_line->buffer);
}