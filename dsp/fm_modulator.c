#include "fm_modulator.h"

void init_fm_modulator(FMModulator *fm, float frequency, float deviation, float sample_rate) {
	fm->frequency = frequency;
	fm->deviation = deviation;
	fm->sample_rate = sample_rate;
	fm->osc_phase = 0.0f;
}

float modulate_fm(FMModulator *fm, float sample) {
	float inst_freq = fm->frequency+(sample*fm->deviation);
	fm->osc_phase += (M_2PI * inst_freq) / fm->sample_rate;
	fm->osc_phase -= (fm->osc_phase >= M_2PI) ? M_2PI : 0.0f;
	return sinf(fm->osc_phase);
}

void init_refrenced_fm_modulator(RefrencedFMModulator* fm, Oscillator* osc, float deviation) {
	fm->deviation = deviation;
	fm->osc = osc;
}

float refrenced_modulate_fm(RefrencedFMModulator* fm, float sample) {
    float inst_freq = sample * fm->deviation;
    float phase = fm->osc->phase + ((M_2PI * inst_freq) / fm->osc->sample_rate);
    
    if (phase >= M_2PI) {
        phase -= M_2PI;
    }
    
    return sinf(phase);
}