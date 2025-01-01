#include "hilbert.h"

void init_hilbert(HilbertTransformer *hilbert) {
    hilbert->delay = calloc(D_SIZE, sizeof(float));
    hilbert->dptr = 0;
}

void apply_hilbert(HilbertTransformer *hilbert, float sample, float *output_0deg, float *output_90deg) {
    float hilb;

    hilbert->delay[hilbert->dptr] = sample;
    hilb = 0.0f;
    for(int i = 0; i < NZEROS/2; i++) {
        hilb += (xcoeffs[i] * hilbert->delay[(hilbert->dptr - i*2) & (D_SIZE - 1)]);
    }

    *output_0deg = hilbert->delay[(hilbert->dptr - 99) & (D_SIZE - 1)];
    *output_90deg = hilb;
    hilbert->dptr = (hilbert->dptr + 1) & (D_SIZE - 1);
}

void exit_hilbert(HilbertTransformer *hilbert) {
    free(hilbert->delay);
}