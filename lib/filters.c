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

void init_compressor(Compressor *compressor, float threshold, float ratio, float knee, float makeup_gain, float attack, float release, float rmsTime, float sample_rate) {
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

float rms_compress(Compressor *compressor, float sample) {
    float env;
    float rmsAlpha = 1.0f - exp(-1.0f / (compressor->rmsTime * compressor->sample_rate));
    compressor->rmsEnv = (1.0f - rmsAlpha) * compressor->rmsEnv + rmsAlpha * (sample * sample);
    env = sqrtf(compressor->rmsEnv);

    float input_db = voltage_to_voltage_db(env);

    float targetBoost = 0.0f;
    if(input_db < compressor->threshold) {
        if(compressor->knee > 0.0f) {
            float delta = compressor->threshold - input_db;
            if(delta < compressor->knee / 2.0f) {
                targetBoost = (1.0f - 1.0f / compressor->ratio) * (delta * delta) / compressor->knee;
            } else {
                targetBoost = (1.0f - 1.0f / compressor->ratio) * delta;
            }
        } else {
            targetBoost = (1.0f - 1.0f / compressor->ratio) * (compressor->threshold - input_db);
        }
    } else {
        targetBoost = 0.0f;
    }

    float coeff;
    if(targetBoost > compressor->gainReduction) {
        coeff = expf(-1.0f / (compressor->attack * compressor->sample_rate));
    } else {
        coeff = expf(-1.0f / (compressor->release * compressor->sample_rate));
    }
    compressor->gainReduction = coeff * compressor->gainReduction + (1.0f - coeff) * targetBoost;

    float gain = voltage_db_to_voltage(compressor->makeup_gain + compressor->gainReduction);
    return sample * gain;
}

float peak_compress(Compressor *compressor, float sample) {
    float env = fabsf(sample);
    float input_db = voltage_to_voltage_db(env);

    float targetBoost = 0.0f;
    if(input_db < compressor->threshold) {
        if(compressor->knee > 0.0f) {
            float delta = compressor->threshold - input_db;
            if(delta < compressor->knee / 2.0f) {
                targetBoost = (1.0f - 1.0f / compressor->ratio) * (delta * delta) / compressor->knee;
            } else {
                targetBoost = (1.0f - 1.0f / compressor->ratio) * delta;
            }
        } else {
            targetBoost = (1.0f - 1.0f / compressor->ratio) * (compressor->threshold - input_db);
        }
    } else {
        targetBoost = 0.0f;
    }

    float coeff;
    if(targetBoost > compressor->gainReduction) {
        coeff = expf(-1.0f / (compressor->attack * compressor->sample_rate));
    } else {
        coeff = expf(-1.0f / (compressor->release * compressor->sample_rate));
    }
    compressor->gainReduction = coeff * compressor->gainReduction + (1.0f - coeff) * targetBoost;

    float gain = voltage_db_to_voltage(compressor->makeup_gain + compressor->gainReduction);
    return sample * gain;
}


void init_compressor_stereo(StereoCompressor *compressor, float threshold, float ratio, float knee, float makeup_gain, float attack, float release, float rmsTime, float sample_rate) {
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

float rms_compress_stereo(StereoCompressor *compressor, float l, float r, float *output_r) {
    float env_l, env_r;
    float rmsAlpha = 1.0f - exp(-1.0f / (compressor->rmsTime * compressor->sample_rate));
    compressor->rmsEnv  = (1.0f - rmsAlpha) * compressor->rmsEnv  + rmsAlpha * (l * l);
    compressor->rmsEnv2 = (1.0f - rmsAlpha) * compressor->rmsEnv2 + rmsAlpha * (r * r);
    env_l = sqrtf(compressor->rmsEnv);
    env_r = sqrtf(compressor->rmsEnv2);

    float input_db_l = voltage_to_voltage_db(env_l);
    float input_db_r = voltage_to_voltage_db(env_r);

    float targetBoost_l = 0.0f;
    if(input_db_l < compressor->threshold) {
        if(compressor->knee > 0.0f) {
            float delta = compressor->threshold - input_db_l;
            if(delta < compressor->knee / 2.0f) {
                targetBoost_l = (1.0f - 1.0f / compressor->ratio) * (delta * delta) / compressor->knee;
            } else {
                targetBoost_l = (1.0f - 1.0f / compressor->ratio) * delta;
            }
        } else {
            targetBoost_l = (1.0f - 1.0f / compressor->ratio) * (compressor->threshold - input_db_l);
        }
    } else {
        targetBoost_l = 0.0f;
    }

    float targetBoost_r = 0.0f;
    if(input_db_r < compressor->threshold) {
        if(compressor->knee > 0.0f) {
            float delta = compressor->threshold - input_db_r;
            if(delta < compressor->knee / 2.0f) {
                targetBoost_r = (1.0f - 1.0f / compressor->ratio) * (delta * delta) / compressor->knee;
            } else {
                targetBoost_r = (1.0f - 1.0f / compressor->ratio) * delta;
            }
        } else {
            targetBoost_r = (1.0f - 1.0f / compressor->ratio) * (compressor->threshold - input_db_r);
        }
    } else {
        targetBoost_r = 0.0f;
    }

    float shared_target_boost = (targetBoost_l > targetBoost_r) ? targetBoost_l : targetBoost_r;

    float coeff;
    if(shared_target_boost > compressor->gainReduction) {
        coeff = expf(-1.0f / (compressor->attack * compressor->sample_rate));
    } else {
        coeff = expf(-1.0f / (compressor->release * compressor->sample_rate));
    }
    compressor->gainReduction = coeff * compressor->gainReduction + (1.0f - coeff) * shared_target_boost;

    float gain = voltage_db_to_voltage(compressor->makeup_gain + compressor->gainReduction);
    *output_r = r * gain;
    return l * gain;
}

float peak_compress_stereo(StereoCompressor *compressor, float l, float r, float *output_r) {
    float env_l = fabsf(l);
    float env_r = fabsf(r);

    float input_db_l = voltage_to_voltage_db(env_l);
    float input_db_r = voltage_to_voltage_db(env_r);

    float targetBoost_l = 0.0f;
    if(input_db_l < compressor->threshold) {
        if(compressor->knee > 0.0f) {
            float delta = compressor->threshold - input_db_l;
            if(delta < compressor->knee / 2.0f) {
                targetBoost_l = (1.0f - 1.0f / compressor->ratio) * (delta * delta) / compressor->knee;
            } else {
                targetBoost_l = (1.0f - 1.0f / compressor->ratio) * delta;
            }
        } else {
            targetBoost_l = (1.0f - 1.0f / compressor->ratio) * (compressor->threshold - input_db_l);
        }
    } else {
        targetBoost_l = 0.0f;
    }

    float targetBoost_r = 0.0f;
    if(input_db_r < compressor->threshold) {
        if(compressor->knee > 0.0f) {
            float delta = compressor->threshold - input_db_r;
            if(delta < compressor->knee / 2.0f) {
                targetBoost_r = (1.0f - 1.0f / compressor->ratio) * (delta * delta) / compressor->knee;
            } else {
                targetBoost_r = (1.0f - 1.0f / compressor->ratio) * delta;
            }
        } else {
            targetBoost_r = (1.0f - 1.0f / compressor->ratio) * (compressor->threshold - input_db_r);
        }
    } else {
        targetBoost_r = 0.0f;
    }

    float shared_target_boost = (targetBoost_l > targetBoost_r) ? targetBoost_l : targetBoost_r;

    float coeff;
    if(shared_target_boost > compressor->gainReduction) {
        coeff = expf(-1.0f / (compressor->attack * compressor->sample_rate));
    } else {
        coeff = expf(-1.0f / (compressor->release * compressor->sample_rate));
    }
    compressor->gainReduction = coeff * compressor->gainReduction + (1.0f - coeff) * shared_target_boost;

    float gain = voltage_db_to_voltage(compressor->makeup_gain + compressor->gainReduction);
    *output_r = r * gain;
    return l * gain;
}
