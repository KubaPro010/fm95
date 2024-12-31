#include "constants.h"
#include "math.h"

typedef struct {
    float phase;
    float phase_increment;
    float sample_rate;
} Oscillator;

void init_oscillator(Oscillator *osc, float frequency, float sample_rate);
void change_oscillator_frequency(Oscillator *osc, float frequency);
float get_oscillator_sin_sample(Oscillator *osc);
float get_oscillator_cos_sample(Oscillator *osc);
