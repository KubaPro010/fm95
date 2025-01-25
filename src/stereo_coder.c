#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <getopt.h>

#include "options.h"

#define DEFAULT_STEREO 1
#define DEFAULT_STEREO_POLAR 0
#define DEFAULT_CLIPPER_THRESHOLD 1.0f
#define DEFAULT_ALSA_OUTPUT 0

//#define SSB
#ifdef SSB
//#define USB
#endif

#include "../lib/constants.h"
#include "../lib/oscillator.h"
#include "../lib/filters.h"
#ifdef SSB
#include "../lib/hilbert.h"
#endif

#define SAMPLE_RATE 192000 // Don't go lower than 108 KHz, becuase it (53000*2) and (38000+15000)

#define INPUT_DEVICE "real_real_tx_audio_input.monitor"
#define OUTPUT_DEVICE "alsa_output.platform-soc_sound.stereo-fallback"
// #define MPX_DEVICE ""

#define BUFFER_SIZE 512

#include <pulse/simple.h>
#include <pulse/error.h>
#include <alsa/asoundlib.h>

#define MONO_VOLUME 0.45f // L+R Signal
#define PILOT_VOLUME 0.09f // 19 KHz Pilot
#define STEREO_VOLUME 0.45f // L-R signal possibly can be set to .9 because im not sure if usb will be 2 times stronger than dsb-sc
#define MPX_VOLUME 1.0f

#ifdef PREEMPHASIS
#define PREEMPHASIS_TAU 0.00005  // 50 microseconds, use 0.000075 if in america
#endif

#ifdef LPF
#define LPF_CUTOFF 15000
#endif

volatile sig_atomic_t to_run = 1;

float hard_clip(float sample, float threshold) {
    if (sample > threshold) {
        return threshold;  // Clip to the upper threshold
    } else if (sample < -threshold) {
        return -threshold;  // Clip to the lower threshold
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

void show_version() {
    printf("FM95 (an FM Processor by radio95) version 1.0\n");
}

void show_help(char *name) {
    printf(
        "FM95 (an FM Processor by radio95)\n"
        "Usage: %s\n\n"
        "   -m,--mono       Force Mono\n"
        "   -s,--stereo     Force Stereo\n"
        "   -i,--input      Override input device\n"
        "   -o,--output     Override output device\n"
        "   -M,--mpx        Override MPX input device\n"
        "   -c,--clipper    Override the clipper threshold\n"
        ,name
    );
}

int main(int argc, char **argv) {
    show_version();
    int stereo = DEFAULT_STEREO;
    float clipper_threshold = DEFAULT_CLIPPER_THRESHOLD;
    #ifndef MPX_DEVICE
    char audio_mpx_device[64] = "\0";
    #else
    char audio_mpx_device[64] = MPX_DEVICE;
    #endif
    pa_simple *mpx_device;
    pa_simple *output_device;
    snd_pcm_hw_params_t *output_params;
    snd_pcm_t *output_handle;
    char audio_input_device[64] = INPUT_DEVICE;
    char audio_output_device[64] = OUTPUT_DEVICE;
    int alsa_output = DEFAULT_ALSA_OUTPUT;

    int opt;
    const char	*short_opt = "msi:o:apM:c:hv";
    struct option	long_opt[] =
	{
		{"mono",		no_argument, NULL, 'm'},
		{"stereo",		no_argument, NULL, 's'},
		{"input",		optional_argument, NULL, 'i'},
		{"output",		optional_argument, NULL, 'o'},
		{"alsa_out",	optional_argument, NULL, 'a'},
		{"pulse_put",	optional_argument, NULL, 'p'},
		{"mpx",	     	optional_argument, NULL, 'M'},
		{"clipper",		optional_argument, NULL, 'c'},

		{"help",	no_argument, NULL, 'h'},
		{"version",	no_argument, NULL, 'v'},
		{ 0,		0,		0,	0 }
	};

    while((opt = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1) {
        switch(opt) {
            case 'm': // Mono
                stereo = 0;
                printf("Running in Mono\n");
                break;
            case 's': // Stereo
                stereo = 1;
                printf("Running in Stereo\n");
                break;
            case 'i': // Input Device
                memcpy(audio_input_device, optarg, 63);
                break;
            case 'o': // Output Device
                memcpy(audio_output_device, optarg, 63);
                break;
            case 'a': // Alsa output
                alsa_output = 1;
                printf("Outputting via alsa\n");
                break;
            case 'p': // Pulse output
                alsa_output = 0;
                printf("Outputting via pulse\n");
                break;
            case 'M': //MPX in
                memcpy(audio_mpx_device, optarg, 63);
                break;
            case 'c': //Clipper
                clipper_threshold = strtof(optarg, NULL);
                printf("Running with a clipper threshold of %f\n", clipper_threshold);
                break;
            case 'v': // Version
                show_version();
                return 0;
            case 'h':
                show_help(argv[0]);
                return 1;
        }
    }

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
    pa_buffer_attr output_buffer_atr = {
        .maxlength = buffer_maxlength,
        .tlength = buffer_tlength_fragsize,
	    .prebuf = buffer_prebuf
    };

    int opentime_pulse_error;

    printf("Connecting to input device... (%s)\n", audio_input_device);

    pa_simple *input_device = pa_simple_new(
        NULL,
        "StereoEncoder",
        PA_STREAM_RECORD,
        audio_input_device,
        "Audio Input",
        &stereo_format,
        NULL,
        &input_buffer_atr,
        &opentime_pulse_error
    );
    if (!input_device) {
        fprintf(stderr, "Error: cannot open input device: %s\n", pa_strerror(opentime_pulse_error));
        return 1;
    }

    if(strlen(audio_mpx_device) != 0) {
        printf("Connecting to MPX device... (%s)\n", audio_mpx_device);

        mpx_device = pa_simple_new(
            NULL,
            "StereoEncoder",
            PA_STREAM_RECORD,
            audio_mpx_device,
            "MPX Input",
            &mono_format,
            NULL,
            &input_buffer_atr,
            &opentime_pulse_error
        );
        if (!mpx_device) {
            fprintf(stderr, "Error: cannot open MPX device: %s\n", pa_strerror(opentime_pulse_error));
            return 1;
        }
    }

    printf("Connecting to output device... (%s)\n", audio_output_device);

    if(alsa_output == 0) {
        output_device = pa_simple_new(
            NULL,
            "StereoEncoder",
            PA_STREAM_PLAYBACK,
            audio_output_device,
            "MPX Output",
            &mono_format,
            NULL,
            &output_buffer_atr,
            &opentime_pulse_error
        );
        if (!output_device) {
            fprintf(stderr, "Error: cannot open output device: %s\n", pa_strerror(opentime_pulse_error));
            pa_simple_free(input_device);
            return 1;
        }
    } else {
        int output_error = snd_pcm_open(&output_handle, audio_output_device, SND_PCM_STREAM_PLAYBACK, 0);
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
            snd_pcm_hw_params_free(output_params);
            return 1;
        }
    }

    Oscillator pilot_osc;
    init_oscillator(&pilot_osc, 19000.0, SAMPLE_RATE); // Pilot, it's there to indicate stereo and as a refrence signal with the stereo carrier

#ifdef SSB
    HilbertTransformer hilbert; // An Hilbert shifts a signal in quadrature, generating the I/Q data
    init_hilbert(&hilbert);
    DelayLine monoDelay; // Hilbert introduces a delay of 99 samples, this should be here to sync the mono with stereo to a sample
    init_delay_line(&monoDelay, 99);
#endif
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
    float audio_stereo_input[BUFFER_SIZE*2]; // Input from device, interleaved stereo
    float mpx_in[BUFFER_SIZE]; // Input from MPX device
    float left[BUFFER_SIZE+64], right[BUFFER_SIZE+64]; // Audio, same thing as in input but uninterleaved, ai told be there could be a buffer overflow here
    float output[BUFFER_SIZE]; // MPX, this goes to the output
    while (to_run) {
        if (pa_simple_read(input_device, audio_stereo_input, sizeof(audio_stereo_input), &pulse_error) < 0) {
            fprintf(stderr, "Error reading from input device: %s\n", pa_strerror(pulse_error));
            to_run = 0;
            break;
        }
        uninterleave(audio_stereo_input, left, right, BUFFER_SIZE*2);
        if(strlen(audio_mpx_device) != 0) {
            if (pa_simple_read(mpx_device, mpx_in, sizeof(mpx_in), &pulse_error) < 0) {
                fprintf(stderr, "Error reading from MPX device: %s\n", pa_strerror(pulse_error));
                to_run = 0;
                break;
            }
        }

        for (int i = 0; i < BUFFER_SIZE; i++) {
            float l_in = left[i];
            float r_in = right[i];
            float multiplex_in = mpx_in[i];

#ifdef PREEMPHASIS
#ifdef LPF
            float lowpassed_left = apply_low_pass_filter(&lpf_l, l_in);
            float lowpassed_right = apply_low_pass_filter(&lpf_r, r_in);
            float preemphasized_left = apply_pre_emphasis(&preemp_l, lowpassed_left)*2;
            float preemphasized_right = apply_pre_emphasis(&preemp_r, lowpassed_right)*2;
            float current_left_input = hard_clip(preemphasized_left, clipper_threshold);
            float current_right_input = hard_clip(preemphasized_right, clipper_threshold);
#else
            float preemphasized_left = apply_pre_emphasis(&preemp_l, l_in)*2;
            float preemphasized_right = apply_pre_emphasis(&preemp_r, r_in)*2;
            float current_left_input = hard_clip(preemphasized_left, clipper_threshold);
            float current_right_input = hard_clip(preemphasized_right, clipper_threshold);
#endif
#else
#ifdef LPF
            float lowpassed_left = apply_low_pass_filter(&lpf_l, l_in);
            float lowpassed_right = apply_low_pass_filter(&lpf_r, r_in);
            float current_left_input = hard_clip(lowpassed_left, clipper_threshold);
            float current_right_input = hard_clip(lowpassed_right, clipper_threshold);
#else
            float current_left_input = hard_clip(l_in, clipper_threshold);
            float current_right_input = hard_clip(r_in, clipper_threshold);
#endif
#endif

            float mono = (current_left_input + current_right_input) / 2.0f; // Stereo to Mono
            if(stereo == 1) {
                float stereo = (current_left_input - current_right_input) / 2.0f; // Also Stereo to Mono but a bit diffrent
                float stereo_carrier = get_oscillator_sin_multiplier_ni(&pilot_osc, 2); // Get stereo carrier via multiplication
#ifdef SSB
                float stereo_carrier_cos = get_oscillator_cos_multiplier_ni(&pilot_osc, 2) // Get Carrier Q of I/Q
                float pilot = get_oscillator_sin_sample(&pilot_osc);

                float stereo_i, stereo_q;
                apply_hilbert(&hilbert, stereo, &stereo_i, &stereo_q); // Compute I/Q
                #ifdef USB
                float signal = (stereo_i*cos38+stereo_q*(sin38*0.775f));
                #else
                float signal = (stereo_i*cos38-stereo_q*(sin38*0.775f));
                #endif
                output[i] = mono*MONO_VOLUME +
                    pilot*PILOT_VOLUME +
                    signal*STEREO_VOLUME
                    ;
#else
                float pilot = get_oscillator_sin_sample(&pilot_osc);
                output[i] = mono*MONO_VOLUME +
                    pilot*PILOT_VOLUME +
                    (stereo*stereo_carrier)*STEREO_VOLUME;
                if(strlen(audio_mpx_device) != 0) output[i] += multiplex_in*MPX_VOLUME;
#endif
            } else {
                output[i] = mono*MONO_VOLUME; // Only Mono
            }
        }

        if(alsa_output == 0) {
            if (pa_simple_write(output_device, output, sizeof(output), &pulse_error) < 0) {
                fprintf(stderr, "Error writing to output device: %s\n", pa_strerror(pulse_error));
                to_run = 0;
                break;
            }
        } else {
            snd_pcm_writei(output_handle, output, sizeof(output));
        }
    }
    printf("Cleaning up...\n");
    pa_simple_free(input_device);
    if(strlen(audio_mpx_device) != 0) pa_simple_free(mpx_device);
    if(alsa_output == 0) {
        pa_simple_free(output_device);
    } else {
        snd_pcm_drain(output_handle);
        snd_pcm_close(output_handle);
        snd_pcm_hw_params_free(output_params);
    }
#ifdef SSB
    exit_hilbert(&hilbert);
    exit_delay_line(&monoDelay);
#endif
    return 0;
}
