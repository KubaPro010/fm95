#include "bs412.h"

float dbr_to_deviation(float dbr) {
	return 19000.0f * powf(10.0f, dbr / 10.0f);
}

float deviation_to_dbr(float deviation) {
	if(deviation == 0.0f) return -100.0f;
	return 10 * log10f(deviation / 19000.0f);
}

void init_modulation_power_measure(MPXPowerMeasurement* mpx, int sample_rate) {
	mpx->sample_counter = 0;
	mpx->sample = 0;
	mpx->sample_rate = sample_rate;
}

float measure_mpx(MPXPowerMeasurement* mpx, float deviation) {
	mpx->sample += deviation * deviation; // rmS
	mpx->sample_counter++;

	float avg_deviation = sqrtf(mpx->sample / mpx->sample_counter); // RMs
	float modulation_power = deviation_to_dbr(avg_deviation);

	#ifdef BS412_DEBUG
	if(mpx->sample_counter % mpx->sample_rate == 0) {
		debug_printf("MPX power: %f dBr\n", modulation_power);
	}
	#endif

	if (mpx->sample_counter >= mpx->sample_rate * 60) {
		#ifdef BS412_DEBUG
		debug_printf("Resetting MPX power measurement\n");
		#endif
		mpx->sample = avg_deviation * avg_deviation;
		mpx->sample_counter = 1;
	}

	return modulation_power;
}
