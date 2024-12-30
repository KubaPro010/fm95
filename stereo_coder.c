#include <stdio.h>
#include <pulse/simple.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <signal.h>

#define INPUT_DEVICE "real_real_tx_audio_input.monitor"
#define OUTPUT_DEVICE "alsa_output.platform-soc_sound.stereo-fallback"
#define BUFFER_SIZE 512
#define CLIPPER_THRESHOLD 0.45 // Adjust this as needed

#define MONO_VOLUME 0.5f // L+R Signal
#define PILOT_VOLUME 0.02f // 19 KHz Pilot
#define STEREO_VOLUME 0.4f // L-R signal

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

const float format_scale = 1.0f / 32768.0f;
void stereo_s16le_to_float(const int16_t *input, float *left, float *right, size_t num_samples) {
    for (size_t i = 0; i < num_samples/2; i++) {
        left[i] = input[i * 2] * format_scale;
        right[i] = input[i * 2 + 1] * format_scale;
    }
}

void float_array_to_s16le(const float *input, int16_t *output, size_t num_samples) {
    for (size_t i = 0; i < num_samples; i++) {
        output[i] = (int16_t)((fminf(fmaxf(input[i], -1.0f), 1.0f)) * 32767.0f);
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

static void stop(int signum) {
    (void)signum;
    printf("\nReceived stop signal. Cleaning up...\n");
    to_run = 0;
}

int main() {
    printf("STCode : Stereo encoder made by radio95 (with help of ChatGPT and Claude, thanks!)\n");
    const float SAMPLE_RATE = 192000.0f; // Don't go lower than 108 KHz, becuase it (53000*2) and (38000+15000)
    const float PILOT_FREQ = 19000.0f;
    const float STEREO_FREQ = 38000.0f;

    // Define formats and buffer atributes
    pa_sample_spec stereo_format = {
        .format = PA_SAMPLE_S16LE,
        .channels = 2,
        .rate = SAMPLE_RATE
    };
    pa_sample_spec mono_format = {
        .format = PA_SAMPLE_S16LE,
        .channels = 1,
        .rate = SAMPLE_RATE
    };

    pa_buffer_attr input_buffer_atr = {
        .maxlength = 4096,
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

    signal(SIGINT, stop);
    signal(SIGTERM, stop);
    
    int16_t input[BUFFER_SIZE*2]; // Input from device
    float left[BUFFER_SIZE], right[BUFFER_SIZE]; // Audio
    float mpx[BUFFER_SIZE]; // MPX
    int16_t output[BUFFER_SIZE]; // Output to device
    while (to_run) {
        if (pa_simple_read(input_device, input, sizeof(input), NULL) < 0) {
            fprintf(stderr, "Error reading from input device.\n");
            break;
        }
        stereo_s16le_to_float(input, left, right, BUFFER_SIZE*2);

        for (int i = 0; i < BUFFER_SIZE; i++) {
            float pilot = get_next_sample(&pilot_osc);
            float stereo_carrier = get_next_sample(&stereo_osc);

            float current_left = clip(left[i]);
            float current_right = clip(right[i]);

            float mono = (current_left + current_right) / 2.0f;
            float stereo = (current_left - current_right) / 2.0f;

            mpx[i] = mono * MONO_VOLUME +
                pilot * PILOT_VOLUME +
                (stereo * stereo_carrier) * STEREO_VOLUME;
        }

        float_array_to_s16le(mpx, output, BUFFER_SIZE);
        if (pa_simple_write(output_device, output, sizeof(output), NULL) < 0) {
            fprintf(stderr, "Error writing to output device.\n");
            break;
        }
    }
    printf("Cleaning up...\n");
    pa_simple_free(input_device);
    pa_simple_free(output_device);
    return 0;
}
