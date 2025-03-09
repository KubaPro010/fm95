#include "fm_modulator.h"

void init_fm_modulator(FMModulator *fm, float frequency, float deviation, float sample_rate) {
    fm->frequency = frequency;
    fm->deviation = deviation;
    fm->sample_rate = sample_rate;
    fm->osc_phase = 0.0f;
}

float modulate_fm(FMModulator *fm, float sample) {
    float inst_freq = fm->frequency+(sample*fm->deviation);
    if (inst_freq < 0.0f) inst_freq = 0.0f;
    float out = sinf(fm->osc_phase);
    fm->osc_phase += fmodf(fm->osc_phase + ((M_2PI * inst_freq) / fm->sample_rate), M_2PI);
    return out;
}