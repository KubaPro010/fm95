#include <stdio.h>
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
// #define ALSA_OUTPUT // Output, not input or both
#define BUFFER_SIZE 512
#define CLIPPER_THRESHOLD 1 // Adjust this as needed, this also limits deviation, so if you set this to 0.5 then the deviation will be limited to half

#include <pulse/simple.h>
#include <pulse/error.h>
#ifdef ALSA_OUTPUT
#include <alsa/asoundlib.h>
#endif

#define VOLUME 0.1f // SCA Volume
#define VOLUME_AUDIO 1.0f // SCA Audio volume
#define FREQUENCY 67000 // SCA Frequency
#define DEVIATION 7000 // SCA Deviation

#ifdef PREEMPHASIS
#define PREEMPHASIS_TAU 0.00005  // 50 microseconds, use 0.000075 if in america
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

static void stop(int signum) {
    (void)signum;
    printf("\nReceived stop signal. Cleaning up...\n");
    to_run = 0;
}

int main() {
    printf("SCAMod : SCA Modulator (based on the Stereo encoder STCode) made by radio95 (with help of ChatGPT and Claude, thanks!)\n");

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
        "SCAMod",
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
    #ifndef ALSA_OUTPUT
    pa_simple *output_device = pa_simple_new(
        NULL,
        "SCAMod",
        PA_STREAM_PLAYBACK,
        OUTPUT_DEVICE,
        "Signal",
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

    FMModulator mod;
    init_fm_modulator(&mod, FREQUENCY, DEVIATION, SAMPLE_RATE);
#ifdef PREEMPHASIS
    ResistorCapacitor preemp;
    init_rc_tau(&preemp, PREEMPHASIS_TAU, SAMPLE_RATE);
#endif

    signal(SIGINT, stop);
    signal(SIGTERM, stop);
    
    int pulse_error;
    float input[BUFFER_SIZE]; // Input from device
    float signal[BUFFER_SIZE]; // this goes to the output
    while (to_run) {
        if (pa_simple_read(input_device, input, sizeof(input), &pulse_error) < 0) {
            fprintf(stderr, "Error reading from input device: %s\n", pa_strerror(pulse_error));
            to_run = 0;
            break;
        }

        for (int i = 0; i < BUFFER_SIZE; i++) {
            float in = input[i];

#ifdef PREEMPHASIS
            float preemphasized = apply_pre_emphasis(&preemp, in)*2;
            float current_input = hard_clip(preemphasized);
#else
            float current_input = hard_clip(in);
#endif

            signal[i] = modulate_fm(&mod, current_input)*VOLUME;
        }

#ifndef ALSA_OUTPUT
        if (pa_simple_write(output_device, signal, sizeof(signal), &pulse_error) < 0) {
            fprintf(stderr, "Error writing to output device: %s\n", pa_strerror(pulse_error));
            to_run = 0;
            break;
        }
#else
        snd_pcm_writei(output_handle, signal, sizeof(signal));
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
