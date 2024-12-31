#include "math.h"
#include <string.h>
#include "constants.h"

#define FIR_PHASES 32
#define FIR_TAPS 32

typedef struct {
    float alpha;
    float prev_sample;
} Emphasis;

void init_emphasis(Emphasis *pe, float tau, float sample_rate);
float apply_pre_emphasis(Emphasis *pe, float sample);

typedef struct {
    float low_pass_fir[FIR_PHASES][FIR_TAPS];
    float sample_buffer[FIR_TAPS];
    int buffer_index;
} LowPassFilter;

void init_low_pass_filter(LowPassFilter *lp, float cutoff_frequency, float sample_rate);
float apply_low_pass_filter(LowPassFilter *lp, float sample);