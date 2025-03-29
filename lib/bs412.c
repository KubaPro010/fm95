#include "bs412.h"

void init_modulation_power_measure(MPXPowerMeasurement* mpx, int sample_rate) {
	mpx->i = 0;
	mpx->sample = 0;
	mpx->sample_rate = sample_rate;
}

float measure_mpx(MPXPowerMeasurement* mpx, int deviation) {
	mpx->sample += 10*log10f(deviation/19000.0f);
	
	float div = mpx->i;
	if(div == 0) div = 1;
	float modulation_power = mpx->sample/div;

	mpx->i++;
	if (mpx->i >= mpx->sample_rate) {
		mpx->sample = modulation_power;
		mpx->i = 0;
	}	
	return modulation_power;
}

float dbr_to_deviation(float dbr) {
	return 19000.0f * powf(10.0f, dbr / 10.0f);
}