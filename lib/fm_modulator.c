#include "fm_modulator.h"

void init_fm_modulator(FMModulator *fm, float frequency, float deviation, float sample_rate) {
    fm->frequency = frequency;
    fm->deviation = deviation;
    init_oscillator(&fm->osc, frequency, sample_rate);
}

float modulate_fm(FMModulator *fm, float sample) {
    float inst_freq = fm->frequency+(sample*fm->deviation);
    change_oscillator_frequency(&fm->osc, inst_freq);
    return get_oscillator_sin_sample(&fm->osc);
}