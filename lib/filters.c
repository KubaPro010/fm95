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
    float y = x;

#if USE_NEON  // Use NEON if available
    float32x4_t v_y = vdupq_n_f32(y);  // Load input into all lanes

    for (int i = 0; i < LPF_ORDER; i += 4) {  // Process 4 biquads at a time
        float32x4_t v_w1 = vld1q_f32(&filter->w1[i]);
        float32x4_t v_w2 = vld1q_f32(&filter->w2[i]);
        float32x4_t v_d1 = vld1q_f32(&filter->d1[i]);
        float32x4_t v_d2 = vld1q_f32(&filter->d2[i]);
        float32x4_t v_A  = vld1q_f32(&filter->A[i]);

        // Compute w0 = d1 * w1 + d2 * w2 + y
        float32x4_t v_w0 = vmlaq_f32(vmulq_f32(v_d1, v_w1), v_d2, v_w2);
        v_w0 = vaddq_f32(v_w0, v_y);

        // Compute y = A * (w0 + 2*w1 + w2)
        float32x4_t v_tw1 = vaddq_f32(v_w1, v_w1);  // 2*w1
        float32x4_t v_sum = vaddq_f32(vaddq_f32(v_w0, v_tw1), v_w2);
        v_y = vmulq_f32(v_A, v_sum);  // Multiply by A

        // Store updated values
        vst1q_f32(&filter->w2[i], v_w1);
        vst1q_f32(&filter->w1[i], v_w0);
    }
    
    return vgetq_lane_f32(v_y, 0);  // Return first lane of vector

#else  // Scalar fallback if NEON is not available
    for (int i = 0; i < LPF_ORDER; i++) {
        float w0_new = filter->d1[i] * filter->w1[i] + filter->d2[i] * filter->w2[i] + y;
        y = filter->A[i] * (w0_new + 2.0f * filter->w1[i] + filter->w2[i]);
        filter->w2[i] = filter->w1[i];
        filter->w1[i] = w0_new;
    }
    return y;
#endif
}