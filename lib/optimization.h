#pragma once
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
	#include <arm_neon.h>
	#define USE_NEON 1
#else
	#define USE_NEON 0
#endif