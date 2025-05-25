#pragma once
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
	#ifdef DEBUG
	#pragma message("Using ARM NEON optimizations")
	#endif
	#include <arm_neon.h>
	#define USE_NEON 1
#else
	#ifdef DEBUG
	#pragma message("ARM NEON optimizations not available")
	#endif
	#define USE_NEON 0
#endif