#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>

#include "../lib/constants.h"
#include "../lib/oscillator.h"
#include "../lib/filters.h"

#include "options.h"

#define SAMPLE_RATE 192000 // Don't go lower than 108 KHz, becuase it (53000*2) and (38000+15000)

#define INPUT_DEVICE "real_real_tx_audio_input.monitor"
#define OUTPUT_DEVICE "alsa_output.platform-soc_sound.stereo-fallback"
// #define ALSA_OUTPUT // Output, not input or both
#define BUFFER_SIZE 512
#define CLIPPER_THRESHOLD 0.525 // Adjust this as needed

#include <pulse/simple.h>
#include <pulse/error.h>
#ifdef ALSA_OUTPUT
#include <alsa/asoundlib.h>
#endif

#define MONO_VOLUME 0.45f // L+R Signal
#define STEREO_VOLUME 0.45f // L-R signal

#ifdef PREEMPHASIS
#define PREEMPHASIS_TAU 0.00005  // 50 microseconds, use 0.000075 if in america
#endif

#ifdef LPF
#define LPF_CUTOFF 15000
#endif

volatile sig_atomic_t to_run = 1;

float hard_clip(float sample) {
    if (sample > CLIPPER_THRESHOLD) {
        return CLIPPER_THRESHOLD;  // Clip to the upper threshold
    } else if (sample < -CLIPPER_THRESHOLD) {
        return -CLIPPER_THRESHOLD;  // Clip to the lower threshold
    } else {
        return sample;  // No clipping
    }
}

void uninterleave(const float *input, float *left, float *right, size_t num_samples) {
    // For stereo, usually it is like this: LEFT RIGHT LEFT RIGHT LEFT RIGHT so this is used to get LEFT LEFT LEFT and RIGHT RIGHT RIGHT
    for (size_t i = 0; i < num_samples/2; i++) {
        left[i] = input[i * 2];
        right[i] = input[i * 2 + 1];
    }
}

static void stop(int signum) {
    (void)signum;
    printf("\nReceived stop signal. Cleaning up...\n");
    to_run = 0;
}

int main() {
    printf("PSTCode : (Polar) Stereo encoder made by radio95 (with help of ChatGPT and Claude, thanks!). Note that this version is for the OIRT band which is in use in Russia, Belarus and other CIS countries\n");
    // Define formats and buffer atributes
    pa_sample_spec stereo_format = {
        .format = PA_SAMPLE_FLOAT32NE, //Float32 NE, or Float32 Native Endian, the float in c uses the endianess of your pc, or native endian, and float is float32, and double is float64
        .channels = 2,
        .rate = SAMPLE_RATE // Same sample rate makes it easy, leave the resampling to pipewire, it should know better
    };
    pa_sample_spec mono_format = {
        .format = PA_SAMPLE_FLOAT32NE,
        .channels = 1,
        .rate = SAMPLE_RATE
    };

    pa_buffer_attr input_buffer_atr = {
        .maxlength = buffer_maxlength,
	    .fragsize = buffer_tlength_fragsize
    };
#ifndef ALSA_OUTPUT
    pa_buffer_attr output_buffer_atr = {
        .maxlength = buffer_maxlength,
        .tlength = buffer_tlength_fragsize,
	    .prebuf = buffer_prebuf
    };
#endif

    printf("Connecting to input device... (%s)\n", INPUT_DEVICE);

    pa_simple *input_device = pa_simple_new(
        NULL,
        "PolarStereoEncoder",
        PA_STREAM_RECORD,
        INPUT_DEVICE,
        "Audio Input",
        &stereo_format,
        NULL,
        &input_buffer_atr,
        NULL
    );
    if (!input_device) {
        fprintf(stderr, "Error: cannot open input device.\n");
        return 1;
    }

    printf("Connecting to output device... (%s)\n", OUTPUT_DEVICE);

#ifndef ALSA_OUTPUT
    pa_simple *output_device = pa_simple_new(
        NULL,
        "PolarStereoEncoder",
        PA_STREAM_PLAYBACK,
        OUTPUT_DEVICE,
        "MPX",
        &mono_format,
        NULL,
        &output_buffer_atr,
        NULL
    );
    if (!output_device) {
        fprintf(stderr, "Error: cannot open output device.\n");
        pa_simple_free(input_device);
        return 1;
    }
#else
    snd_pcm_hw_params_t *output_params;
    snd_pcm_t *output_handle;
    int output_error = snd_pcm_open(&output_handle, OUTPUT_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
    if(output_error < 0) {
        fprintf(stderr, "Error: cannot open output device: %s\n", snd_strerror(output_error));
        pa_simple_free(input_device);
        return 1;
    }
    snd_pcm_hw_params_malloc(&output_params);
    snd_pcm_hw_params_any(output_handle, output_params);
    snd_pcm_hw_params_set_access(output_handle, output_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(output_handle, output_params, SND_PCM_FORMAT_FLOAT); // Same as pulse's Float32NE
    snd_pcm_hw_params_set_channels(output_handle, output_params, 1);
    unsigned int rate = SAMPLE_RATE;
    int dir;
    snd_pcm_hw_params_set_rate_near(output_handle, output_params, &rate, &dir);
    snd_pcm_uframes_t frames = BUFFER_SIZE;
    snd_pcm_hw_params_set_period_size_near(output_handle, output_params, &frames, &dir); // i don't have a clue why like this
    output_error = snd_pcm_hw_params(output_handle, output_params);
    if(output_error < 0) {
        fprintf(stderr, "Error: cannot open output device: %s\n", snd_strerror(output_error));
        snd_pcm_close(output_handle);
        pa_simple_free(input_device);
        return 1;
    }
#endif

    Oscillator stereo_osc;
    init_oscillator(&stereo_osc, 31250.0, SAMPLE_RATE);
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
    float input[BUFFER_SIZE*2]; // Input from device, interleaved stereo
    float left[BUFFER_SIZE+64], right[BUFFER_SIZE+64]; // Audio, same thing as in input but ininterleaved, ai told be there could be a buffer overflow here
    float mpx[BUFFER_SIZE]; // MPX, this goes to the output
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
            float preemphasized_left = apply_pre_emphasis(&preemp_l, lowpassed_left)*2;
            float preemphasized_right = apply_pre_emphasis(&preemp_r, lowpassed_right)*2;
            float current_left_input = hard_clip(preemphasized_left);
            float current_right_input = hard_clip(preemphasized_right);
#else
            float preemphasized_left = apply_pre_emphasis(&preemp_l, l_in)*2;
            float preemphasized_right = apply_pre_emphasis(&preemp_r, r_in)*2;
            float current_left_input = hard_clip(preemphasized_left);
            float current_right_input = hard_clip(preemphasized_right);
#endif
#else
#ifdef LPF
            float lowpassed_left = apply_low_pass_filter(&lpf_l, l_in);
            float lowpassed_right = apply_low_pass_filter(&lpf_r, r_in);
            float current_left_input = hard_clip(lowpassed_left);
            float current_right_input = hard_clip(lowpassed_right);
#else
            float current_left_input = hard_clip(l_in);
            float current_right_input = hard_clip(r_in);
#endif
#endif

            float mono = (current_left_input + current_right_input) / 2.0f; // Stereo to Mono
            float stereo = (current_left_input - current_right_input) / 2.0f; // Also Stereo to Mono but a bit diffrent

            float stereo_carrier = get_oscillator_sin_sample(&stereo_osc);
            // -14 db is somewhere around 20% of a 1 volt signal

            mpx[i] = mono * MONO_VOLUME +
                ((stereo+0.2) * stereo_carrier)*STEREO_VOLUME; // the 0.2 add DC, you know what happens then? Carrier wave
        }

#ifndef ALSA_OUTPUT
        if (pa_simple_write(output_device, mpx, sizeof(mpx), &pulse_error) < 0) {
            fprintf(stderr, "Error writing to output device: %s\n", pa_strerror(pulse_error));
            to_run = 0;
            break;
        }
#else
        snd_pcm_writei(output_handle, mpx, sizeof(mpx));
#endif
    }
    printf("Cleaning up...\n");
    pa_simple_free(input_device);
    #ifndef ALSA_OUTPUT
    pa_simple_free(output_device);
    #else
    snd_pcm_drain(output_handle);
    snd_pcm_close(output_handle);
    snd_pcm_hw_params_free(&output_params);
    #endif
    return 0;
}
