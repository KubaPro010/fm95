#include <stdio.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>

#include "../lib/constants.h"
#include "../lib/filters.h"

#include "options.h"

#define SAMPLE_RATE 32000

#define INPUT_DEVICE "real_real_tx_audio_input.monitor"
#define OUTPUT_DEVICE "alsa_output.platform-soc_sound.stereo-fallback"
#define BUFFER_SIZE 512
#define CLIPPER_THRESHOLD 0.525 // Adjust this as needed

#define MONO_VOLUME 0.6f // L+R Signal

#ifdef PREEMPHASIS
#define PREEMPHASIS_TAU 0.00005  // 50 microseconds, use 0.000075 if in america
#endif

#ifdef LPF
#define LPF_CUTOFF 15000
#endif

volatile sig_atomic_t to_run = 1;

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
    printf("MonoPass : Mono filter for FM made by radio95 (with help of ChatGPT and Claude, thanks!)\n");
    // Define formats and buffer atributes
    pa_sample_spec audio_format = {
        .format = PA_SAMPLE_FLOAT32NE, //Float32 NE, or Float32 Native Endian, the float in c uses the endianess of your pc, or native endian, and float is float32, and double is float64
        .channels = 1,
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
        "MonoPass",
        PA_STREAM_RECORD,
        INPUT_DEVICE,
        "Audio Input",
        &audio_format,
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
        "MonoPass",
        PA_STREAM_PLAYBACK,
        OUTPUT_DEVICE,
        "Audio",
        &audio_format,
        NULL,
        &output_buffer_atr,
        NULL
    );
    if (!output_device) {
        fprintf(stderr, "Error: cannot open output device.\n");
        pa_simple_free(input_device);
        return 1;
    }

#ifdef PREEMPHASIS
    ResistorCapacitor preemp;
    init_rc(&preemp, PREEMPHASIS_TAU, SAMPLE_RATE);
#endif
#ifdef LPF
    LowPassFilter lpf;
    init_low_pass_filter(&lpf, LPF_CUTOFF, SAMPLE_RATE);
#endif

    signal(SIGINT, stop);
    signal(SIGTERM, stop);
    
    int pulse_error;
    float input[BUFFER_SIZE]; // Input from device, interleaved stereo
    float mpx[BUFFER_SIZE]; // MPX, this goes to the output
    while (to_run) {
        if (pa_simple_read(input_device, input, sizeof(input), &pulse_error) < 0) {
            fprintf(stderr, "Error reading from input device: %s\n", pa_strerror(pulse_error));
            to_run = 0;
            break;
        }
    
        for (int i = 0; i < BUFFER_SIZE; i++) {
            float in = input[i];

#ifdef PREEMPHASIS
#ifdef LPF
            float lowpassed = apply_low_pass_filter(&lpf, in);
            float preemphasized = apply_pre_emphasis(&preemp, lowpassed);
            float current_input = clip(preemphasized);
#else
            float preemphasized = apply_pre_emphasis(&preemp, in);
            float current_input = clip(preemphasized);
#endif
#else
#ifdef LPF
            float lowpassed = apply_low_pass_filter(&lpf, in);
            float current_input = clip(lowpassed);
#else
            float current_input = clip(in);
#endif
#endif

            mpx[i] = current_input * MONO_VOLUME;
        }

        if (pa_simple_write(output_device, mpx, sizeof(mpx), &pulse_error) < 0) {
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
