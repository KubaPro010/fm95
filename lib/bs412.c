#include "bs412.h"

float dbr_to_deviation(float dbr) {
	return 19000.0f * powf(10.0f, dbr / 20.0f);
}

float deviation_to_dbr(float deviation) {
	return 20*log10f(deviation/19000.0f);
}

void init_modulation_power_measure(MPXPowerMeasurement* mpx, int sample_rate) {
	mpx->i = 1;
	mpx->sample = 0;
	mpx->sample_rate = sample_rate;
}

float measure_mpx(MPXPowerMeasurement* mpx, int deviation) {
	mpx->sample += deviation;

	float avg_deviation = mpx->sample/mpx->i;
	float modulation_power = deviation_to_dbr(avg_deviation);

	if (mpx->i >= mpx->sample_rate*60) {
		mpx->sample = 0;
		mpx->i = 1;
	}	
	return modulation_power;
}