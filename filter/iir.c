#include "iir.h"

void init_preemphasis(ResistorCapacitor *filter, float tau, float sample_rate, float ref_freq) {
    float dt = 1.0f / sample_rate;
    filter->alpha = tau / (tau + dt);

    float omega = M_2PI * ref_freq / sample_rate;
    filter->gain = 1.0f / sqrtf(1.0f + filter->alpha * filter->alpha - 2.0f * filter->alpha * cosf(omega));

    filter->prev_sample = 0.0f;
}
inline float apply_preemphasis(ResistorCapacitor *filter, float sample) {
	float out = (sample - filter->alpha * filter->prev_sample) * filter->gain;
	filter->prev_sample = sample;
	return out;
}

void tilt_init(TiltCorrectionFilter* f, float correction_strength, float sr) {
    float cutoff = 1000.0f; // fixed split point

    // one-pole lowpass setup
    float alpha = expf(-2.0f * (float)M_PI * cutoff / sr);
    f->a1 = alpha;
    f->a0 = 1.0f - alpha;
    f->lp = 0.0f;

    // simple low/high gains from tilt
    float t = (correction_strength < -1.0f) ? -1.0f : (correction_strength > 1.0f ? 1.0f : correction_strength);
    f->low_gain  = 1.0f - t;
    f->high_gain = 1.0f + t;
}

float tilt(TiltCorrectionFilter* f, float in) {
    // lowpass
    f->lp = f->a0 * in + f->a1 * f->lp;
    float hp = in - f->lp;
    return f->lp * f->low_gain + hp * f->high_gain;
}