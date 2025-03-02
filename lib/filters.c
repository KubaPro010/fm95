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

static float compute_gain_reduction(float input_db, float threshold, float ratio, float knee) {
    float gain_reduction = 0.0f;
    
    if (knee > 0.0f && input_db > (threshold - knee / 2.0f) && input_db < (threshold + knee / 2.0f)) {
        float knee_range = input_db - (threshold - knee / 2.0f);
        float knee_factor = knee_range * knee_range / (2.0f * knee);
        gain_reduction = (ratio - 1.0f) * knee_factor / ratio;
    } else if (input_db > threshold) {
        gain_reduction = (threshold - input_db) * (1.0f - 1.0f / ratio);
    }
    
    return gain_reduction;
}

void init_compressor(Compressor *compressor, float threshold, float ratio, float knee, 
                     float makeup_gain, float attack, float release, float rmsTime, float sample_rate) {
    compressor->threshold = threshold;
    compressor->ratio = ratio;
    compressor->knee = knee;
    compressor->makeup_gain = makeup_gain;
    compressor->attack = attack;
    compressor->release = release;
    compressor->sample_rate = sample_rate;
    compressor->gainReduction = 0.0f;
    compressor->rmsEnv = 0.0f;
    compressor->rmsTime = rmsTime;
}

float peak_compress(Compressor *compressor, float sample) {
    float input_level_db = linear_to_db(fabsf(sample));
    
    float desired_gain_reduction = compute_gain_reduction(input_level_db, 
                                                         compressor->threshold, 
                                                         compressor->ratio, 
                                                         compressor->knee);
    
    float attack_coef = expf(-1.0f / (compressor->sample_rate * compressor->attack));
    float release_coef = expf(-1.0f / (compressor->sample_rate * compressor->release));
    
    float coef = (fabsf(desired_gain_reduction) > fabsf(compressor->gainReduction)) ? attack_coef : release_coef;
    
    compressor->gainReduction = desired_gain_reduction + coef * (compressor->gainReduction - desired_gain_reduction);
    
    float gain = db_to_linear(compressor->gainReduction + compressor->makeup_gain);
    
    return sample * gain;
}

float rms_compress(Compressor *compressor, float sample) {
    float rms_coef = expf(-1.0f / (compressor->sample_rate * compressor->rmsTime));
    float squared_input = sample * sample;
    
    compressor->rmsEnv = squared_input + rms_coef * (compressor->rmsEnv - squared_input);
    
    float input_level_db = linear_to_db(sqrtf(fmaxf(compressor->rmsEnv, 1e-9f)));
    
    float desired_gain_reduction = compute_gain_reduction(input_level_db, 
                                                         compressor->threshold, 
                                                         compressor->ratio, 
                                                         compressor->knee);
    
    float attack_coef = expf(-1.0f / (compressor->sample_rate * compressor->attack));
    float release_coef = expf(-1.0f / (compressor->sample_rate * compressor->release));
    
    float coef = (fabsf(desired_gain_reduction) > fabsf(compressor->gainReduction)) ? attack_coef : release_coef;
    
    compressor->gainReduction = desired_gain_reduction + coef * (compressor->gainReduction - desired_gain_reduction);
    
    float gain = db_to_linear(compressor->gainReduction + compressor->makeup_gain);
    
    return sample * gain;
}

void init_compressor_stereo(StereoCompressor *compressor, float threshold, float ratio, 
                           float knee, float makeup_gain, float attack, float release, 
                           float rmsTime, float sample_rate) {
    compressor->threshold = threshold;
    compressor->ratio = ratio;
    compressor->knee = knee;
    compressor->makeup_gain = makeup_gain;
    compressor->attack = attack;
    compressor->release = release;
    compressor->sample_rate = sample_rate;
    compressor->gainReduction = 0.0f;
    compressor->rmsEnv = 0.0f;
    compressor->rmsEnv2 = 0.0f;
    compressor->rmsTime = rmsTime;
}

float peak_compress_stereo(StereoCompressor *compressor, float l, float r, float *output_r) {
    float max_level = fmaxf(fabsf(l), fabsf(r));
    
    float input_level_db = linear_to_db(max_level);
    
    float desired_gain_reduction = compute_gain_reduction(input_level_db, 
                                                         compressor->threshold, 
                                                         compressor->ratio, 
                                                         compressor->knee);
    
    float attack_coef = expf(-1.0f / (compressor->sample_rate * compressor->attack));
    float release_coef = expf(-1.0f / (compressor->sample_rate * compressor->release));
    
    float coef = (fabsf(desired_gain_reduction) > fabsf(compressor->gainReduction)) ? attack_coef : release_coef;
    
    compressor->gainReduction = desired_gain_reduction + coef * (compressor->gainReduction - desired_gain_reduction);
    
    float gain = db_to_linear(compressor->gainReduction + compressor->makeup_gain);
    
    *output_r = r * gain;
    return l * gain;
}

float rms_compress_stereo(StereoCompressor *compressor, float l, float r, float *output_r) {
    float rms_coef = expf(-1.0f / (compressor->sample_rate * compressor->rmsTime));
    float squared_input1 = l * l;
    float squared_input2 = r * r;
    
    compressor->rmsEnv = squared_input1 + rms_coef * (compressor->rmsEnv - squared_input1);
    compressor->rmsEnv2 = squared_input2 + rms_coef * (compressor->rmsEnv2 - squared_input2);
    
    float max_rms = fmaxf(compressor->rmsEnv, compressor->rmsEnv2);
    
    float input_level_db = linear_to_db(sqrtf(fmaxf(max_rms, 1e-9f)));
    
    float desired_gain_reduction = compute_gain_reduction(input_level_db, 
                                                         compressor->threshold, 
                                                         compressor->ratio, 
                                                         compressor->knee);
    
    float attack_coef = expf(-1.0f / (compressor->sample_rate * compressor->attack));
    float release_coef = expf(-1.0f / (compressor->sample_rate * compressor->release));
    
    float coef = (fabsf(desired_gain_reduction) > fabsf(compressor->gainReduction)) ? attack_coef : release_coef;
    
    compressor->gainReduction = desired_gain_reduction + coef * (compressor->gainReduction - desired_gain_reduction);
    
    float gain = db_to_linear(compressor->gainReduction + compressor->makeup_gain);
    
    *output_r = r * gain;
    return l * gain;
}