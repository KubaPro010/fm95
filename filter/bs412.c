#include "bs412.h"

#define LOG2_19000 log2f(19000.0f)

inline float dbr_to_deviation(float dbr) {
	return 19000.0f * powf(2.0f, dbr * 0.332193f);
}

inline float deviation_to_dbr(float deviation) {
	if (deviation < 1e-6f) return -100.0f;
	return 10.0f * (log2f(deviation) - LOG2_19000) * 0.30103f;
}

void init_bs412(BS412Compressor* mpx, float mpx_deviation, float target_power, float attack, float release, float max, int sample_rate) {
	mpx->mpx_deviation = mpx_deviation;
	mpx->average_counter = 0;
	mpx->average = 0;
	mpx->sample_rate = sample_rate;
	mpx->attack = expf(-1.0f / (attack * sample_rate));
	mpx->release = expf(-1.0f / (release * sample_rate));
	mpx->target = target_power;
	mpx->gain = 0.0f;
	mpx->max = max;
	#ifdef BS412_DEBUG
	debug_printf("Initialized MPX power measurement with sample rate: %d\n", sample_rate);
	#endif
}

float bs412_compress(BS412Compressor* mpx, float sample) {
	mpx->average += sample * sample * mpx->mpx_deviation * mpx->mpx_deviation;
	mpx->average_counter++;

	float avg_power = mpx->average / mpx->average_counter;
	float avg_deviation = sqrtf(avg_power);
	float modulation_power = deviation_to_dbr(avg_deviation);

	#ifdef BS412_DEBUG
	if(mpx->average_counter % mpx->sample_rate == 0) {
		debug_printf("MPX power: %.2f dBr (%.0f)\n", modulation_power, avg_deviation);
	}
	#endif

	if (mpx->average_counter >= mpx->sample_rate * 60) {
		#ifdef BS412_DEBUG
		debug_printf("Resetting MPX power measurement\n");
		#endif
		mpx->average = avg_power;
		mpx->average_counter = 1;
	}

	float target_gain = powf(10.0f, (mpx->target - modulation_power) / 20.0f);
	if (modulation_power > mpx->target) {
		mpx->gain = mpx->attack * mpx->gain + (1.0f - mpx->attack) * target_gain;
	} else {
		mpx->gain = mpx->release * mpx->gain + (1.0f - mpx->release) * target_gain;
	}
	
	mpx->gain = fmaxf(0.0f, fminf(mpx->max, mpx->gain));

	return sample * mpx->gain;
}
