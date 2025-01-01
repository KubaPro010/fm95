#include <stdlib.h>
#include <math.h>
#include "constants.h"

#define D_SIZE 256
#define NZEROS 200

typedef struct {
    float* delay;
    int dptr;
    float coeffs[NZEROS]
} HilbertTransformer;

void init_hilbert(HilbertTransformer *hilbert);
void apply_hilbert(HilbertTransformer *hilbert, float sample, float *output_0deg, float *output_90deg);
void exit_hilbert(HilbertTransformer *hilbert);