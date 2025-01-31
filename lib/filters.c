#include "filters.h"

void init_preemphasis(BiquadFilter *filter, float tau, float sample_rate) {
    /*
    premphasis should go like this:
    cutoff: +0
    2cutoff: +6
    3cutoff: +12
    4cutoff: +18
    ...
    */

    // Calculate the cutoff frequency from tau (f_c = 1/(2πτ))
    float cutoff_freq = 1.0f / (2.0f * M_PI * tau);
    
    // Pre-warp the cutoff frequency for the bilinear transform
    float omega = 2.0f * M_PI * cutoff_freq / sample_rate;
    float K = tanf(omega / 2.0f);

    // Calculate filter coefficients for 1st-order high-pass (converted to biquad)
    float alpha = 1.0f + K;
    filter->b0 = 1.0f / alpha;
    filter->b1 = -filter->b0;
    filter->b2 = 0.0f;
    filter->a1 = (K - 1.0f) / alpha;
    filter->a2 = 0.0f;

    // Reset state variables
    filter->x1 = 0.0f;
    filter->x2 = 0.0f;
    filter->y1 = 0.0f;
    filter->y2 = 0.0f;
}
float apply_preemphasis(BiquadFilter *filter, float input) {
    // Direct Form I implementation
    float output = filter->b0 * input
                 + filter->b1 * filter->x1
                 + filter->b2 * filter->x2
                 - filter->a1 * filter->y1
                 - filter->a2 * filter->y2;

    // Update state variables
    filter->x2 = filter->x1;
    filter->x1 = input;
    filter->y2 = filter->y1;
    filter->y1 = output;

    return output;
}

void init_lpf(FrequencyFilter* filter, float cutoffFreq, float sampleRate) {
    float nyquist = sampleRate / 2.0f;
    float fc = cutoffFreq / nyquist;
    
    // Blackman window for sharp transition
    for (int n = 0; n < FILTER_TAPS; n++) {
        float m = n - (FILTER_TAPS - 1.0f) / 2.0f;
        
        // Sinc function
        float sinc = (m == 0) ? 1.0f : sinf(M_PI * m * fc) / (M_PI * m);
        
        // Blackman window
        float window = 0.42f - 0.5f * cosf(2.0f * M_PI * n / (FILTER_TAPS - 1)) 
                     + 0.08f * cosf(4.0f * M_PI * n / (FILTER_TAPS - 1));
        
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
        float sinc = (m == 0) ? 1.0f : -sinf(M_PI * m * fc) / (M_PI * m);
        
        // Blackman window
        float window = 0.42f - 0.5f * cosf(2.0f * M_PI * n / (FILTER_TAPS - 1)) 
                     + 0.08f * cosf(4.0f * M_PI * n / (FILTER_TAPS - 1));
        
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

float apply_frequency_filter(FrequencyFilter* filter, float input) {
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

float hard_clip(float sample, float threshold) {
    if (sample > threshold) {
        return threshold;  // Clip to the upper threshold
    } else if (sample < -threshold) {
        return -threshold;  // Clip to the lower threshold
    } else {
        return sample;  // No clipping
    }
}
float soft_clip(float sample, float threshold) {
    if (fabs(sample) <= threshold) {
        return sample; // Linear region
    } else {
        float sign = (sample > 0) ? 1.0f : -1.0f;
        return sign * (threshold + (1.0f - threshold) * pow(fabs(sample) - threshold, 0.5f));
    }
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
    delay_line->buffer = NULL;
}