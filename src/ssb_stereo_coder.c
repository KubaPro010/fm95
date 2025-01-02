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
#include "../lib/hilbert.h"

// Features
#include "features.h"
//#define USB

#define SAMPLE_RATE 192000 // Don't go lower than 108 KHz, becuase it (53000*2) and (38000+15000)

#define INPUT_DEVICE "real_real_tx_audio_input.monitor"
#define OUTPUT_DEVICE "alsa_output.platform-soc_sound.stereo-fallback"
#define BUFFER_SIZE 512
#define CLIPPER_THRESHOLD 0.45 // Adjust this as needed

#define MONO_VOLUME 0.6f // L+R Signal
#define PILOT_VOLUME 0.035f // 19 KHz Pilot
#define STEREO_VOLUME 0.4f // L-R signal

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
    printf("SSB-STCode : Stereo encoder made by radio95 (with help of ChatGPT and Claude, thanks!)\n");
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
        .maxlength = 4096, // You can lower this to 512, but this is fine, it's sub-second delay, you're probably not gonna notice unless you're looking for it
	    .fragsize = 2048
    };
    pa_buffer_attr output_buffer_atr = {
        .maxlength = 4096,
        .tlength = 2048,
	    .prebuf = 0
    };

    printf("Connecting to input device... (%s)\n", INPUT_DEVICE);

    pa_simple *input_device = pa_simple_new(
        NULL,
        "StereoEncoder",
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

    pa_simple *output_device = pa_simple_new(
        NULL,
        "StereoEncoder",
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

    Oscillator pilot_osc;
    init_oscillator(&pilot_osc, 19000.0, SAMPLE_RATE); // Pilot, it's there to indicate stereo and as a refrence signal with the stereo carrier
    HilbertTransformer hilbert;
    init_hilbert(&hilbert);
    DelayLine monoDelay;
    init_delay_line(&monoDelay, 99);
#ifdef PREEMPHASIS
    Emphasis preemp_l, preemp_r;
    init_emphasis(&preemp_l, PREEMPHASIS_TAU, SAMPLE_RATE);
    init_emphasis(&preemp_r, PREEMPHASIS_TAU, SAMPLE_RATE);
#endif
#ifdef LPF
    LowPassFilter lpf_l, lpf_r;
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
            float sin38 = sinf(pilot_osc.phase*2);
            float cos38 = cosf(pilot_osc.phase*2); // Stereo carrier should be a harmonic of the pilot which is in phase, best way to generate the harmonic is to multiply the pilot's phase by two, so it is mathematically impossible for them to not be in phase
            float pilot = get_oscillator_sin_sample(&pilot_osc); // This is after because if it was before then the stereo would be out of phase by one increment, so [GET STEREO] ([GET PILOT] [INCREMENT PHASE])
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

            float mono = (current_left_input + current_right_input) / 2.0f; // Stereo to Mono

            float stereo = (current_left_input - current_right_input) / 2.0f; // Also Stereo to Mono but a bit diffrent
            float stereo_i, stereo_q;
            apply_hilbert(&hilbert, stereo, &stereo_i, &stereo_q); // I/Q, the Quadrature data is 90 degrees apart from the In-phase data
#ifdef USB
            float signal = (stereo_i*cos38+stereo_q*(sin38*0.775f)); // Compute LSB/USB, as the Hilbert isn't perfect, i'll have to a bit silence down the Q carrier in order to make it better, also, it is just perfect as FM Stereo LSB shouldn't be fully LSB
#else
            float signal = (stereo_i*cos38-stereo_q*(sin38*0.775f)); // Compute LSB/USB, as the Hilbert isn't perfect, i'll have to a bit silence down the Q carrier in order to make it better, also, it is just perfect as FM Stereo LSB shouldn't be fully LSB
#endif

            mpx[i] = delay_line(&monoDelay, mono) * MONO_VOLUME +
                pilot * PILOT_VOLUME +
                signal*STEREO_VOLUME;
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
    exit_hilbert(&hilbert);
    return 0;
}
