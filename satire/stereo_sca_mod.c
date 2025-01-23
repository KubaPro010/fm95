#include <stdio.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>

#include "../lib/constants.h"
#include "../lib/oscillator.h"
#include "../lib/filters.h"
#include "../lib/fm_modulator.h"

#include "options.h"

#define SAMPLE_RATE 192000

#define INPUT_DEVICE "SCA.monitor"
#define OUTPUT_DEVICE "alsa_output.platform-soc_sound.stereo-fallback"
#define BUFFER_SIZE 512
#define CLIPPER_THRESHOLD 0.75 // Adjust this as needed, this also limits deviation, so if you set this to 0.5 then the deviation will be limited to half

#define MONO_VOLUME 0.075f // Mono Volume
#define STEREO_VOLUME 0.025f // Stereo Volume

#ifdef PREEMPHASIS
#define PREEMPHASIS_TAU 0.00005  // 50 microseconds, use 0.000075 if in america
#endif

#ifdef LPF
#define LPF_CUTOFF 8000
#endif

volatile sig_atomic_t to_run = 1;

void uninterleave(const float *input, float *left, float *right, size_t num_samples) {
    // For stereo, usually it is like this: LEFT RIGHT LEFT RIGHT LEFT RIGHT so this is used to get LEFT LEFT LEFT and RIGHT RIGHT RIGHT
    for (size_t i = 0; i < num_samples/2; i++) {
        left[i] = input[i * 2];
        right[i] = input[i * 2 + 1];
    }
}

float clip(float sample) {
    if (sample > CLIPPER_THRESHOLD) {
        return CLIPPER_THRESHOLD;  // Clip to the upper threshold
    } else if (sample < -CLIPPER_THRESHOLD) {
        return -CLIPPER_THRESHOLD;  // Clip to the lower threshold
    } else {
        return sample;  // No clipping
    }
}

static void stop(int signum) {
    (void)signum;
    printf("\nReceived stop signal. Cleaning up...\n");
    to_run = 0;
}

int main() {
    printf("StereoSCAMod : Stereo SCA Modulator (based on the SCA encoder SCAMod) made by radio95 (with help of ChatGPT and Claude, thanks!)\n");

    // Define formats and buffer atributes
    pa_sample_spec mono_audio_format = {
        .format = PA_SAMPLE_FLOAT32LE,
        .channels = 1,
        .rate = SAMPLE_RATE // Same sample rate makes it easy, leave the resampling to pipewire, it should know better
    };
    pa_sample_spec stereo_audio_format = {
        .format = PA_SAMPLE_FLOAT32LE,
        .channels = 2,
        .rate = SAMPLE_RATE // Same sample rate makes it easy, leave the resampling to pipewire, it should know better
    };

    pa_buffer_attr input_buffer_atr = {
        .maxlength = buffer_maxlength,
	    .fragsize = buffer_tlength_fragsize
    };
    pa_buffer_attr output_buffer_atr = {
        .maxlength = buffer_maxlength,
        .tlength = buffer_tlength_fragsize,
	    .prebuf = buffer_prebuf
    };

    printf("Connecting to input device... (%s)\n", INPUT_DEVICE);

    pa_simple *input_device = pa_simple_new(
        NULL,
        "StereoSCAMod",
        PA_STREAM_RECORD,
        INPUT_DEVICE,
        "Audio Input",
        &stereo_audio_format,
        NULL,
        &input_buffer_atr,
        NULL
    );
    if (!input_device) {
        fprintf(stderr, "Error: cannot open input device.\n");
        return 1;
    }

    printf("Connecting to output device... (%s)\n", OUTPUT_DEVICE);

    pa_simple *output_device = pa_simple_new(
        NULL,
        "StereoSCAMod",
        PA_STREAM_PLAYBACK,
        OUTPUT_DEVICE,
        "Signal",
        &mono_audio_format,
        NULL,
        &output_buffer_atr,
        NULL
    );
    if (!output_device) {
        fprintf(stderr, "Error: cannot open output device.\n");
        pa_simple_free(input_device);
        return 1;
    }

    FMModulator mod_mono, mod_stereo;
    init_fm_modulator(&mod_mono, 67000, 6000, SAMPLE_RATE);
    init_fm_modulator(&mod_stereo, 80000, 6000, SAMPLE_RATE);
#ifdef PREEMPHASIS
    ResistorCapacitor preemp_l, preemp_r;
    init_rc_tau(&preemp_l, PREEMPHASIS_TAU, SAMPLE_RATE);
    init_rc_tau(&preemp_r, PREEMPHASIS_TAU, SAMPLE_RATE);
#endif
#ifdef LPF
    ResistorCapacitor lpf_l, lpf_r;
    init_low_pass_filter(&lpf_l, LPF_CUTOFF, SAMPLE_RATE);
    init_low_pass_filter(&lpf_r, LPF_CUTOFF, SAMPLE_RATE);
#endif

    signal(SIGINT, stop);
    signal(SIGTERM, stop);
    
    int pulse_error;
    float input[BUFFER_SIZE*2]; // Input from device
    float left[BUFFER_SIZE+64], right[BUFFER_SIZE+64]; // Audio, same thing as in input but ininterleaved, ai told be there could be a buffer overflow here
    float signal[BUFFER_SIZE]; // this goes to the output
    while (to_run) {
        if (pa_simple_read(input_device, input, sizeof(input), &pulse_error) < 0) {
            fprintf(stderr, "Error reading from input device: %s\n", pa_strerror(pulse_error));
            to_run = 0;
            break;
        }
        uninterleave(input, left, right, BUFFER_SIZE*2);

        for (int i = 0; i < BUFFER_SIZE; i++) {
            float l_in = left[i];
            float r_in = right[i];

#ifdef PREEMPHASIS
#ifdef LPF
            float lowpassed_left = apply_low_pass_filter(&lpf_l, l_in);
            float lowpassed_right = apply_low_pass_filter(&lpf_r, r_in);
            float preemphasized_left = apply_pre_emphasis(&preemp_l, lowpassed_left);
            float preemphasized_right = apply_pre_emphasis(&preemp_r, lowpassed_right);
            float current_left_input = clip(preemphasized_left);
            float current_right_input = clip(preemphasized_right);
#else
            float preemphasized_left = apply_pre_emphasis(&preemp_l, l_in);
            float preemphasized_right = apply_pre_emphasis(&preemp_r, r_in);
            float current_left_input = clip(preemphasized_left);
            float current_right_input = clip(preemphasized_right);
#endif
#else
#ifdef LPF
            float lowpassed_left = apply_low_pass_filter(&lpf_l, l_in);
            float lowpassed_right = apply_low_pass_filter(&lpf_r, r_in);
            float current_left_input = clip(lowpassed_left);
            float current_right_input = clip(lowpassed_right);
#else
            float current_left_input = clip(l_in);
            float current_right_input = clip(r_in);
#endif
#endif
            float mono = (current_left_input+current_right_input)/2.0f;
            float stereo = (current_left_input-current_right_input)/2.0f;

            signal[i] = modulate_fm(&mod_mono, mono)*MONO_VOLUME+
                modulate_fm(&mod_stereo, stereo)*STEREO_VOLUME;
        }

        if (pa_simple_write(output_device, signal, sizeof(signal), &pulse_error) < 0) {
            fprintf(stderr, "Error writing to output device: %s\n", pa_strerror(pulse_error));
            to_run = 0;
            break;
        }
    }
    printf("Cleaning up...\n");
    pa_simple_free(input_device);
    pa_simple_free(output_device);
    return 0;
}
