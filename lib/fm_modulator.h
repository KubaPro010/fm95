#include "oscillator.h"

typedef struct
{
    float frequency;
    float deviation;
    Oscillator osc;
} FMModulator;

void init_fm_modulator(FMModulator *fm, float frequency, float deviation, float sample_rate);
float modulate_fm(FMModulator *fm, float sample);