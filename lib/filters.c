#include "filters.h"

void init_preemphasis(ResistorCapacitor *filter, float tau, float sample_rate) {
	filter->prev_sample = 0.0f;
	filter->alpha = expf(-1 / (tau*sample_rate));
	filter->gain = 1.0f / sqrtf(1.0f - filter->alpha);
}
float apply_preemphasis(ResistorCapacitor *filter, float sample) {
	float out = (sample - filter->alpha * filter->prev_sample) * filter->gain;
	filter->prev_sample = sample;
	return out;
}

float hard_clip(float sample, float threshold) {
	return fmaxf(-threshold, fminf(threshold, sample));
}

void init_lpf(LPFFilter *filter, float cutoff, int sample_rate) {
	float a = tanf(M_PI * cutoff / sample_rate);
	float a2 = a * a;
	float r, e;

	for (int i = 0; i < LPF_ORDER; i++) {
		r = sinf(M_PI * (2.0f * i + 1.0f) / (4.0f * LPF_ORDER));
		e = a2 + 2.0f * a * r + 1.0f;
		float inv_e = 1.0f / e;

		filter->A[i]  = a2 * inv_e;
		filter->d1[i] = 2.0f * (1.0f - a2) * inv_e;
		filter->d2[i] = -(a2 - 2.0f * a * r + 1.0f) * inv_e;
	}
}

float process_lpf(LPFFilter *filter, float x) {
#if USE_NEON
	float32_t input = x;

	float32x4_t y_vec = vdupq_n_f32(input);

	for (int i = 0; i < LPF_ORDER; i++) {
		float32x4_t d1_vec = vdupq_n_f32(filter->d1[i]);
		float32x4_t d2_vec = vdupq_n_f32(filter->d2[i]);
		float32x4_t w1_vec = vdupq_n_f32(filter->w1[i]);
		float32x4_t w2_vec = vdupq_n_f32(filter->w2[i]);
		float32x4_t A_vec = vdupq_n_f32(filter->A[i]);

		float32x4_t w1_term = vmulq_f32(d1_vec, w1_vec);
		float32x4_t w2_term = vmulq_f32(d2_vec, w2_vec);
		float32x4_t w0_new_vec = vaddq_f32(vaddq_f32(w1_term, w2_term), y_vec);

		float32x4_t two_w1 = vmulq_n_f32(w1_vec, 2.0f);
		float32x4_t output_term = vaddq_f32(w0_new_vec, vaddq_f32(two_w1, w2_vec));
		y_vec = vmulq_f32(A_vec, output_term);

		filter->w2[i] = filter->w1[i];
		filter->w1[i] = vgetq_lane_f32(w0_new_vec, 0);
	}

	return vgetq_lane_f32(y_vec, 0);
#else
	float y = x;
	for (int i = 0; i < LPF_ORDER; i++) {
		float w0_new = filter->d1[i] * filter->w1[i] + filter->d2[i] * filter->w2[i] + y;
		y = filter->A[i] * (w0_new + 2.0f * filter->w1[i] + filter->w2[i]);
		filter->w2[i] = filter->w1[i];
		filter->w1[i] = w0_new;
	}
	return y;
#endif
}