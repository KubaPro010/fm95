#include "bs412.h"

float dbr_to_deviation(float dbr) {
	return 19000.0f * powf(10.0f, dbr / 10.0f);
}

float deviation_to_dbr(float deviation) {
	return 10 * log10f((deviation + 1e-6f) / 19000.0f);
}

void init_modulation_power_measure(MPXPowerMeasurement* mpx, int sample_rate) {
	mpx->i = 1;
	mpx->sample = 0;
	mpx->sample_rate = sample_rate;
}

float measure_mpx(MPXPowerMeasurement* mpx, float deviation) {
	mpx->sample += deviation * deviation; // rmS
	mpx->i++;

	float avg_deviation = sqrtf(mpx->sample / mpx->i); // RMs
	float modulation_power = deviation_to_dbr(avg_deviation);

	if (mpx->i >= mpx->sample_rate * 60) {
		mpx->sample = 0;
		mpx->i = 0;
	}

	return modulation_power;
}
