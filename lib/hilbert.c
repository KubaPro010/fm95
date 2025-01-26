#include "hilbert.h"

void compute_hilbert_coeffs(float* coeffs, int taps) {
    int mid = taps / 2;
    for (int i = 0; i < taps; i++) {
        if ((i - mid) % 2 == 0) {
            coeffs[i] = 0.0f;
        } else {
            coeffs[i] = 2.0f / (PI * (i - mid));
        }
    }
}

// Initialize the Hilbert transformer
void init_hilbert(HilbertTransformer* filter) {
    compute_hilbert_coeffs(filter->coeffs, HILBERT_TAPS);
    memset(filter->delay, 0, sizeof(filter->delay));
    filter->index = 0;
}

// Apply the Hilbert transformer
void apply_hilbert(HilbertTransformer* filter, float input, float* inphase, float* quadrature) {
    // Insert the new sample into the circular buffer
    filter->delay[filter->index] = input;

    // Compute the in-phase and quadrature components
    float i_sum = 0.0f; // In-phase (0-degree output)
    float q_sum = 0.0f; // Quadrature (90-degree output)

    int coeff_index = 0;
    for (int i = filter->index; coeff_index < HILBERT_TAPS; coeff_index++) {
        i_sum += filter->delay[i] * (coeff_index == HILBERT_TAPS / 2 ? 1.0f : 0.0f);
        q_sum += filter->delay[i] * filter->coeffs[coeff_index];

        i = (i > 0) ? i - 1 : HILBERT_TAPS - 1;
    }

    // Update the index for the next sample
    filter->index = (filter->index + 1) % HILBERT_TAPS;

    *inphase = i_sum;
    *quadrature = q_sum;
}