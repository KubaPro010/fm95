#pragma once
#include <math.h>
#include <stdint.h>

typedef struct {
	float targetLevel;
	float maxGain;
	float minGain;
	float attackTime;
	float releaseTime;

	float currentGain;
	float currentLevel;

	uint32_t sampleRate;
	float attackCoef;
	float releaseCoef;

	float rmsBuffer;
	float rmsAlpha;
	float rmsBeta;
} AGC;

void initAGC(AGC* agc, int sampleRate, float targetLevel, float minGain, float maxGain, float attackTime, float releaseTime);
float process_agc(AGC* agc, float sidechain);