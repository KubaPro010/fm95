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

void init_lpf(BiquadFilter* filter, float cutoffFreq, float qFactor, float sampleRate) {
    float x = (cutoffFreq * M_2PI) / sampleRate;
    float sinX = sin(x);
    float y = sinX / (qFactor*2.0f);
    float cosX = cos(x);
    float z = (1.0f-cosX)/2.0f;

    float _a0 = y + 1.0f;
    float _a1 = cosX * -2.0f;
    float _a2 = 1.0f - y;
    float _b0 = z;
    float _b1 = 1.0f - cosX;
    float _b2 = z;

    filter->y2 = 0;
    filter->y1 = 0;
    filter->x2 = 0;
    filter->x1 = 0;
    filter->b0 = _b0/_a0;
    filter->b1 = _b1/_a0;
    filter->b2 = _b2/_a0;
    filter->a1 = -_a1/_a0;
    filter->a2 = -_a2/_a0;
}

void init_hpf(BiquadFilter* filter, float cutoffFreq, float qFactor, float sampleRate) {
    float x = (cutoffFreq * M_2PI) / sampleRate;
    float sinX = sin(x);
    float y = sinX / (qFactor*2.0f);
    float cosX = cos(x);
    float z = (1.0f-cosX)/2.0f;

    float _a0 = y + 1.0f;
    float _a1 = cosX * -2.0f;
    float _a2 = 1.0f - y;
    float _b0 = 1.0f - z;
    float _b1 = cosX * -2.0f;
    float _b2 = 1.0f - z;

    filter->y2 = 0;
    filter->y1 = 0;
    filter->x2 = 0;
    filter->x1 = 0;
    filter->b0 = _b0/_a0;
    filter->b1 = _b1/_a0;
    filter->b2 = _b2/_a0;
    filter->a1 = -_a1/_a0;
    filter->a2 = -_a2/_a0;
}

void init_bpf(BiquadFilter* filter, float centerFreq, float qFactor, float sampleRate) {
    float x = (centerFreq * M_2PI) / sampleRate;
    float sinX = sin(x);
    float cosX = cos(x);
    
    float alpha = sinX / (2.0f * qFactor);

    float _a0 = 1.0f + alpha;
    float _a1 = -2.0f * cosX;
    float _a2 = 1.0f - alpha;
    float _b0 = alpha;
    float _b1 = 0.0f;
    float _b2 = -alpha;

    filter->y2 = 0;
    filter->y1 = 0;
    filter->x2 = 0;
    filter->x1 = 0;
    
    filter->b0 = _b0 / _a0;
    filter->b1 = _b1 / _a0;
    filter->b2 = _b2 / _a0;
    filter->a1 = -_a1 / _a0;
    filter->a2 = -_a2 / _a0;
}

float apply_frequency_filter(BiquadFilter* filter, float input) {
    float out = input*filter->b0+filter->x1*filter->b1+filter->x2*filter->b2+filter->y1*filter->a1+filter->y2*filter->a2;
    filter->y2 = filter->y1;
    filter->y1 = out;
    filter->x2 = filter->x1;
    filter->x1 = input;
    return out;
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

float voltage_db_to_voltage(float db) {
    return powf(10.0f, db / 20.0f);
}

float power_db_to_voltage(float db) {
    return powf(10.0f, db / 10.0f);
}

float voltage_to_voltage_db(float linear) {
    return 20.0f * log10f(fmaxf(linear, 1e-10f)); // Avoid log(0)
}

float voltage_to_power_db(float linear) {
    return 10.0f * log10f(fmaxf(linear, 1e-10f)); // Avoid log(0)
}

void init_compressor(Compressor *compressor, float attack, float release) {
    compressor->attack = attack;
    compressor->release = release;
    compressor->max = 0.0f;
}

float peak_compress(Compressor *compressor, float sample) {
    float sample_abs = fabsf(sample);
    if(sample_abs > compressor->max) {
        compressor->max += (sample_abs - compressor->max) / * compressor->attack;
    } else {
        compressor->max *= compressor->release;
    }
    return sample/(compressor->max+0.01);
}

float peak_compress_stereo(Compressor *compressor, float l, float r, float *output_r) {
    float l_abs = fabsf(l);
    float r_abs = fabsf(r);
    float max = (l_abs > r_abs) ? l_abs : r_abs;
    if(max > compressor->max) {
        compressor->max += (max - compressor->max) / compressor->attack;
    } else {
        compressor->max *= compressor->release;
    }
    *output_r = r/(compressor->max+0.01);
    return l/(compressor->max+0.01);
}
