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
	float32x4_t x_vec = vdupq_n_f32(x);
	float32x4_t y_vec = x_vec;

	float32x4_t *A_vec = (float32x4_t*)filter->A;
	float32x4_t *d1_vec = (float32x4_t*)filter->d1;
	float32x4_t *d2_vec = (float32x4_t*)filter->d2;
	float32x4_t *w1_vec = (float32x4_t*)filter->w1;
	float32x4_t *w2_vec = (float32x4_t*)filter->w2;

	for (int i = 0; i < LPF_ORDER; i += 4) {
		float32x4_t w1_term = vmulq_f32(*d1_vec, *w1_vec);
		float32x4_t w2_term = vmulq_f32(*d2_vec, *w2_vec);
		float32x4_t w0_new = vaddq_f32(vaddq_f32(w1_term, w2_term), y_vec);

		float32x4_t two_w1 = vmulq_n_f32(*w1_vec, 2.0f);
		y_vec = vmulq_f32(*A_vec, vaddq_f32(vaddq_f32(w0_new, two_w1), *w2_vec));

		*w2_vec = *w1_vec;
		*w1_vec = w0_new;

		A_vec++;
		d1_vec++;
		d2_vec++;
		w1_vec++;
		w2_vec++;
	}

	float32x2_t y_low = vget_low_f32(y_vec);
	float32x2_t y_high = vget_high_f32(y_vec);
	float32x2_t y_sum = vadd_f32(y_low, y_high);
	y_sum = vpadd_f32(y_sum, y_sum);

	return vget_lane_f32(y_sum, 0);
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