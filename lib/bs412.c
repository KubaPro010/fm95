#include "bs412.h"

void init_modulation_power_measure(MPXPowerMeasurement* mpx, int sample_rate) {
	mpx->i = 0;
	mpx->sample = 0;
	mpx->sample_rate = sample_rate;
}

float measure_mpx(MPXPowerMeasurement* mpx, int deviation) {
	mpx->sample += 10*log10f(deviation/19000.0f);
	mpx->i++;
	float modulation_power = mpx->sample/mpx->i;
	if (mpx->i >= mpx->sample_rate) {
		mpx->sample = 0.0f;
		mpx->i = 0;
	}	
	return modulation_power;
}