#include <stdio.h>
#include <pulse/simple.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <signal.h>

#define INPUT_DEVICE "real_real_tx_audio_input.monitor"
#define OUTPUT_DEVICE "alsa_output.platform-soc_sound.stereo-fallback"
#define RDS_INPUT "RDS.monitor"
#define BUFFER_SIZE 512

#define MONO_VOLUME 0.5f // L+R Signal
#define PILOT_VOLUME 0.025f // 19 KHz Pilot
#define STEREO_VOLUME 0.275f // L-R signal
#define RDS_VOLUME 0.035f // RDS Signal

volatile sig_atomic_t to_run = 1;

const float format_scale = 1.0f / 32768.0f;
void stereo_s16le_to_float(const int16_t *input, float *left, float *right, size_t num_samples) {
    for (size_t i = 0; i < num_samples/2; i++) {
        left[i] = input[i * 2] * format_scale;
        right[i] = input[i * 2 + 1] * format_scale;
    }
}
void mono_s16le_to_float(const int16_t *input, float *output, size_t num_samples) {
    for (size_t i = 0; i < num_samples; i++) {
        output[i] = input[i] * format_scale;
    }
}

void float_array_to_s16le(const float *input, int16_t *output, size_t num_samples) {
    for (size_t i = 0; i < num_samples; i++) {
        output[i] = (int16_t)((fminf(fmaxf(input[i], -1.0f), 1.0f)) * 32767.0f);
    }
}

#ifndef M_2PI
#define M_2PI (3.14159265358979323846 * 2.0)
#endif
#ifndef M_PI_2
#define M_PI_2 (3.14159265358979323846 / 2.0)
#endif

typedef struct {
    float phase;
    float phase_increment;
    float quadrature_phase;
} Oscillator;

void init_oscillator(Oscillator *osc, float frequency, float sample_rate) {
    osc->phase = 0.0f;
    osc->quadrature_phase = M_PI_2;  // 90 degrees phase shift
    osc->phase_increment = (M_2PI * frequency) / sample_rate;
}

float get_next_sample(Oscillator *osc, int quadrature) {
    float sample;
    if (quadrature) {
        sample = sinf(osc->quadrature_phase);
        osc->quadrature_phase += osc->phase_increment;
        if (osc->quadrature_phase >= M_2PI) {
            osc->quadrature_phase -= M_2PI;
        }
    } else {
        sample = sinf(osc->phase);
        osc->phase += osc->phase_increment;
        if (osc->phase >= M_2PI) {
            osc->phase -= M_2PI;
        }
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
    const float SAMPLE_RATE = 192000.0f; // Don't go lower than 176 KHz
    const float PILOT_FREQ = 19000.0f;
    const float STEREO_FREQ = 38000.0f;
    const float RDS_FREQ = 57000.0f;

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

    printf("Connecting to input devices... (%s, %s)\n", INPUT_DEVICE, RDS_INPUT);

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
    pa_simple *input_device_rds = pa_simple_new(
        NULL,
        "StereoEncoder",
        PA_STREAM_RECORD,
        RDS_INPUT,
        "RDS Input",
        &mono_format,
        NULL,
        &input_buffer_atr,
        NULL
    );
    if (!input_device_rds) {
        fprintf(stderr, "Error: cannot open input device.\n");
        pa_simple_free(input_device);
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
        pa_simple_free(input_device_rds);
        return 1;
    }

    Oscillator pilot_osc, stereo_osc, rds_osc;
    init_oscillator(&pilot_osc, PILOT_FREQ, SAMPLE_RATE);
    init_oscillator(&stereo_osc, STEREO_FREQ, SAMPLE_RATE);
    init_oscillator(&rds_osc, RDS_FREQ, SAMPLE_RATE);

    signal(SIGINT, stop);
    signal(SIGTERM, stop);
    
    int16_t input[BUFFER_SIZE*2];
    int16_t input_rds[BUFFER_SIZE];
    float rds_data[BUFFER_SIZE];
    float left[BUFFER_SIZE], right[BUFFER_SIZE];
    float mpx[BUFFER_SIZE];
    int16_t output[BUFFER_SIZE];
    while (to_run) {
        if (pa_simple_read(input_device, input, sizeof(input), NULL) < 0) {
            fprintf(stderr, "Error reading from input device.\n");
            break;
        }
        if (pa_simple_read(input_device_rds, input_rds, sizeof(input_rds), NULL) < 0) {
            fprintf(stderr, "Error reading from input device.\n");
            break;
        }
        stereo_s16le_to_float(input, left, right, BUFFER_SIZE*2);
        mono_s16le_to_float(input_rds, rds_data, BUFFER_SIZE);

        for (int i = 0; i < BUFFER_SIZE; i++) {
            float pilot = get_next_sample(&pilot_osc, 0);
            float stereo_carrier = get_next_sample(&stereo_osc, 0);
            float rds_carrier = get_next_sample(&rds_osc, 1);

            float mono = (left[i] + right[i]) / 2.0f;
            float stereo = (left[i] - right[i]) / 2.0f;
            float rds_sample = rds_data[i];

            mpx[i] = mono*MONO_VOLUME +
                     (stereo * stereo_carrier)*STEREO_VOLUME +
                     (pilot * PILOT_VOLUME) +
                     (rds_sample * rds_carrier)*RDS_VOLUME;
        }

        float_array_to_s16le(mpx, output, BUFFER_SIZE);
        if (pa_simple_write(output_device, output, sizeof(output), NULL) < 0) {
            fprintf(stderr, "Error writing to output device.\n");
            break;
        }
    }
    printf("Cleaning up...\n");
    pa_simple_free(input_device);
    pa_simple_free(input_device_rds);
    pa_simple_free(output_device);
    return 0;
}
