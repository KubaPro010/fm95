#include "hilbert.h"

static float generate_coefficient(int i, int nzeros) {
    if (i == nzeros/2) return 0.0f;
    
    float n = i - nzeros/2;
    // Basic Hilbert transform coefficient
    float h = (i == nzeros/2) ? 0.0f : (2.0f/(PI * n));
    
    // Apply Blackman window for better stopband attenuation
    float w = 0.42f - 0.5f * cosf(2.0f * PI * i / nzeros) + 
              0.08f * cosf(4.0f * PI * i / nzeros);
    
    return h * w;
}
void init_hilbert(HilbertTransformer *hilbert) {
    hilbert->delay = calloc(D_SIZE, sizeof(float));
    hilbert->dptr = 0;
    for(int i = 0; i < NZEROS; i++) {
        hilbert->coeffs[i] = generate_coefficient(i, NZEROS);
    }
}

void apply_hilbert(HilbertTransformer *hilbert, float sample, float *output_0deg, float *output_90deg) {
    float hilb;

    hilbert->delay[hilbert->dptr] = sample;
    hilb = 0.0f;
    for(int i = 0; i < NZEROS/2; i++) {
        hilb += (hilbert->coeffs[i] * hilbert->delay[(hilbert->dptr - i*2) & (D_SIZE - 1)]);
    }

    *output_0deg = hilbert->delay[(hilbert->dptr - 99) & (D_SIZE - 1)];
    *output_90deg = hilb;
    hilbert->dptr = (hilbert->dptr + 1) & (D_SIZE - 1);
}

void exit_hilbert(HilbertTransformer *hilbert) {
    free(hilbert->delay);
}