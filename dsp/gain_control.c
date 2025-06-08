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
}

float process_agc(AGC* agc, float sidechain) {
	float x2 = sidechain * sidechain;

	float rmsAlpha = expf(-1.0f / (agc->sampleRate * 0.04));
	agc->rms_buffer = rmsAlpha * agc->rms_buffer + (1.0f - rmsAlpha) * x2;
	float instantLevel = sqrtf(agc->rms_buffer);

	float alpha = (instantLevel > agc->currentLevel) ? agc->attackCoef : agc->releaseCoef;
	agc->currentLevel = alpha * agc->currentLevel + (1.0f - alpha) * instantLevel;

	float desiredGain = agc->targetLevel / fmaxf(agc->currentLevel, 1e-10f);
	desiredGain = fminf(fmaxf(desiredGain, agc->minGain), agc->maxGain);

	float gainAlpha = (desiredGain > agc->currentGain) ? agc->attackCoef : agc->releaseCoef;
	agc->currentGain = gainAlpha * agc->currentGain + (1.0f - gainAlpha) * desiredGain;

	return agc->currentGain;
}