#include "filters.h"

void init_emphasis(Emphasis *pe, float tau, float sample_rate) {
    pe->prev_sample = 0.0f;
    pe->alpha = exp(-1 / (tau * sample_rate));
}

float apply_pre_emphasis(Emphasis *pe, float sample) {
    float audio = sample-pe->alpha*pe->prev_sample;
    pe->prev_sample = audio;
    return audio*2;
}

void init_low_pass_filter(LowPassFilter *lp, float cutoff_frequency, float sample_rate) {
    float rc = 1/(M_2PI*cutoff_frequency);
    lp->alpha = sample_rate/(sample_rate+rc);
    lp->prev_sample = 0.0f;
}

float apply_low_pass_filter(LowPassFilter *lp, float sample) {
    float output = lp->alpha*sample+(1-lp->alpha)*lp->prev_sample;
    lp->prev_sample = output;
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