#include "oscillator.h"

void init_oscillator(Oscillator *osc, float frequency, float sample_rate) {
    osc->phase = 0.0f;
    osc->phase_increment = (M_2PI * frequency) / sample_rate;
    osc->sample_rate = sample_rate;
}

void change_oscillator_frequency(Oscillator *osc, float frequency) {
    osc->phase_increment = (M_2PI * frequency) / osc->sample_rate;
}

float get_oscillator_sin_sample(Oscillator *osc) {
    float sample = sinf(osc->phase);
    advance_oscillator(osc);
    return sample;
}

float get_oscillator_cos_sample(Oscillator *osc) {
    float sample = cosf(osc->phase);
    advance_oscillator(osc);
    return sample;
}

float get_oscillator_sin_multiplier_ni(Oscillator *osc, float multiplier) {
    float temp_phase_increment = osc->phase_increment * multiplier;
    float temp_phase = fmodf(osc->phase + temp_phase_increment, M_2PI);
    return sinf(temp_phase);
}

float get_oscillator_cos_multiplier_ni(Oscillator *osc, float multiplier) {
    float temp_phase_increment = osc->phase_increment * multiplier;
    float temp_phase = fmodf(osc->phase + temp_phase_increment, M_2PI);
    return cosf(temp_phase);
}

void advance_oscillator(Oscillator *osc) {
    osc->phase = fmodf(osc->phase + osc->phase_increment, M_2PI);
}