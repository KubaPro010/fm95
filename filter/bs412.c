#include "bs412.h"

#define LOG2_19000 log2f(19000.0f)

inline float dbr_to_deviation(float dbr) {
	return 19000.0f * powf(2.0f, dbr * 0.332193f);
}

inline float deviation_to_dbr(float deviation) {
	if (deviation < 1e-6f) return -100.0f;
	return 10.0f * (log2f(deviation) - LOG2_19000) * 0.30103f;
}

void init_modulation_power_measure(MPXPowerMeasurement* mpx, int sample_rate) {
	mpx->sample_counter = 0;
	mpx->sample = 0;
	mpx->sample_rate = sample_rate;
	#ifdef BS412_DEBUG
	debug_printf("Initialized MPX power measurement with sample rate: %d\n", sample_rate);
	#endif
}

float measure_mpx(MPXPowerMeasurement* mpx, float deviation) {
	mpx->sample += deviation * deviation; // rmS
	mpx->sample_counter++;

	float inv_counter = 1.0f / mpx->sample_counter;
	float avg_deviation = sqrtf(mpx->sample * inv_counter); // RMs
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
