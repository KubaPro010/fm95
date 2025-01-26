#pragma once

#include <stdlib.h>
#include <string.h>
#include "constants.h"
#include <math.h>

#define HILBERT_TAPS 95

typedef struct {
    float coeffs[HILBERT_TAPS];
    float delay[HILBERT_TAPS];
    int index;
} HilbertTransformer;

void init_hilbert(HilbertTransformer* filter);
void apply_hilbert(HilbertTransformer* filter, float input, float* inphase, float* quadrature);