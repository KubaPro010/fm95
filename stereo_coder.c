#include <stdio.h>
#include <pulse/simple.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>

// Features
// #define PREEMPHASIS

#define SAMPLE_RATE 192000 // Don't go lower than 108 KHz, becuase it (53000*2) and (38000+15000)

#define INPUT_DEVICE "real_real_tx_audio_input.monitor"
#define OUTPUT_DEVICE "alsa_output.platform-soc_sound.stereo-fallback"
#define BUFFER_SIZE 512
#define CLIPPER_THRESHOLD 0.45 // Adjust this as needed

#define MONO_VOLUME 0.5f // L+R Signal
#define PILOT_VOLUME 0.02f // 19 KHz Pilot
#define STEREO_VOLUME 0.4f // L-R signal

#ifdef PREEMPHASIS
#define PREEMPHASIS_TAU 0.00005  // 50 microseconds, use 0.000075 if in america
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


#define M_2PI (3.14159265358979323846 * 2.0)

// Track phase continuously to maintain frequency accuracy
typedef struct {
    float phase;
    float phase_increment;
} Oscillator;

void init_oscillator(Oscillator *osc, float frequency, float sample_rate) {
    osc->phase = 0.0f;
    osc->phase_increment = (M_2PI * frequency) / sample_rate;
}

float get_next_sample(Oscillator *osc) {
    float sample = sinf(osc->phase);
    osc->phase += osc->phase_increment;
    if (osc->phase >= M_2PI) {
        osc->phase -= M_2PI;
    }
    return sample;
}

#ifdef PREEMPHASIS
typedef struct {
    float prev_sample;
    float alpha;
} PreEmphasis;

void init_pre_emphasis(PreEmphasis *pe) {
    pe->prev_sample = 0.0f;
    pe->alpha = exp(-1 / (PREEMPHASIS_TAU*SAMPLE_RATE));
}
float apply_pre_emphasis(PreEmphasis *pe, float sample) {
    float output = sample - pe->alpha * pe->prev_sample;
    pe->prev_sample = output;
    return output;
}
#endif

static void stop(int signum) {
    (void)signum;
    printf("\nReceived stop signal. Cleaning up...\n");
    to_run = 0;
}

int main() {
    printf("STCode : Stereo encoder made by radio95 (with help of ChatGPT and Claude, thanks!)\n");
    const float PILOT_FREQ = 19000.0f; // Don't touch this
    const float STEREO_FREQ = 38000.0f; // This too

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

    Oscillator pilot_osc, stereo_osc;
    init_oscillator(&pilot_osc, PILOT_FREQ, SAMPLE_RATE);
    init_oscillator(&stereo_osc, STEREO_FREQ, SAMPLE_RATE);
#ifdef PREEMPHASIS
    PreEmphasis preemp_l, preemp_r;
    init_pre_emphasis(&preemp_l);
    init_pre_emphasis(&preemp_r);
#endif

    signal(SIGINT, stop);
    signal(SIGTERM, stop);
    
    float input[BUFFER_SIZE*2]; // Input from device, interleaved stereo
    float left[BUFFER_SIZE], right[BUFFER_SIZE]; // Audio, same thing as in input but ininterleaved
    float mpx[BUFFER_SIZE]; // MPX, this goes to the output
    while (to_run) {
        if (pa_simple_read(input_device, input, sizeof(input), NULL) < 0) {
            fprintf(stderr, "Error reading from input device.\n");
            break;
        }
        uninterleave(input, left, right, BUFFER_SIZE*2);

        for (int i = 0; i < BUFFER_SIZE; i++) {
            float pilot = get_next_sample(&pilot_osc);
            float stereo_carrier = get_next_sample(&stereo_osc);
            float l_in = left[i];
            float r_in = right[i];

#ifdef PREEMPHASIS
            float preemphasized_left = apply_pre_emphasis(&preemp_l, l_in);
            float preemphasized_right = apply_pre_emphasis(&preemp_r, r_in);
            float current_left_input = clip(preemphasized_left);
            float current_right_input = clip(preemphasized_right);
#else
            float current_left_input = clip(r_in);
            float current_right_input = clip(l_in);
#endif

            float mono = (current_left_input + current_right_input) / 2.0f;
            float stereo = (current_left_input - current_right_input) / 2.0f;

            mpx[i] = mono * MONO_VOLUME +
                pilot * PILOT_VOLUME +
                (stereo * stereo_carrier) * STEREO_VOLUME;
        }

        if (pa_simple_write(output_device, mpx, sizeof(mpx), NULL) < 0) {
            fprintf(stderr, "Error writing to output device.\n");
            break;
        }
    }
    printf("Cleaning up...\n");
    pa_simple_free(input_device);
    pa_simple_free(output_device);
    return 0;
}
