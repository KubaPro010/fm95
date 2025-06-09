#include "gain_control.h"

void initAGC(AGC* agc, int sampleRate, float targetLevel, float minGain, float maxGain, float attackTime, float releaseTime) {
	agc->sampleRate = sampleRate;
	agc->targetLevel = targetLevel;
	agc->minGain = minGain;
	agc->maxGain = maxGain;
	agc->attackTime = attackTime;
	agc->releaseTime = releaseTime;

	agc->attackCoef = expf(-1.0f / (sampleRate * attackTime));
	agc->releaseCoef = expf(-1.0f / (sampleRate * releaseTime));

	agc->currentGain = 1.0f;
	agc->currentLevel = 0.0f;

	agc->rms_buffer = 0.0f;
	agc->rmsAlpha = expf(-1.0f / (sampleRate * 0.04f));
}

float process_agc(AGC* agc, float sidechain) {
	float x2 = sidechain * sidechain;

	agc->rms_buffer = agc->rmsAlpha * agc->rms_buffer + (1.0f - agc->rmsAlpha) * x2;
	const float instantLevel = sqrtf(agc->rms_buffer);
	
	const float levelAlpha = (instantLevel > agc->currentLevel) ? agc->attackCoef : agc->releaseCoef;
	agc->currentLevel = levelAlpha * agc->currentLevel + (1.0f - levelAlpha) * instantLevel;

	float desiredGain = agc->targetLevel / (agc->currentLevel + 1e-10f);
	desiredGain = fminf(fmaxf(desiredGain, agc->minGain), agc->maxGain);

	const float gainAlpha = (desiredGain > agc->currentGain) ? agc->attackCoef : agc->releaseCoef;
	agc->currentGain = gainAlpha * agc->currentGain + (1.0f - gainAlpha) * desiredGain;

	return agc->currentGain;
}