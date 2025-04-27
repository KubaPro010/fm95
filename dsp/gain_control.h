#pragma once
#include <math.h>

typedef struct {
	float targetLevel;
	float maxGain;
	float minGain;
	float attackTime;
	float releaseTime;

	float currentGain;
	float currentLevel;

	int sampleRate;
	float attackCoef;
	float releaseCoef;

	float rms_buffer;
} AGC;

void initAGC(AGC* agc, int sampleRate, float targetLevel, float minGain, float maxGain, float attackTime, float releaseTime);
float process_agc_stereo(AGC* agc, float left, float right, float *right_out);