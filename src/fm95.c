#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#define buffer_maxlength 12288
#define buffer_tlength_fragsize 12288
#define buffer_prebuf 16

#define DEFAULT_STEREO 1
#define DEFAULT_STEREO_POLAR 0
#define DEFAULT_STEREO_SSB 0
#define DEFAULT_CLIPPER_THRESHOLD 1.0f
#define DEFAULT_ALSA_OUTPUT 0
#define DEFAULT_SCA_FREQUENCY 67000.0f
#define DEFAULT_SCA_DEVIATION 7000.0f
#define DEFAULT_SCA_CLIPPER_THRESHOLD 1.0f // Full deviation
#define DEFAULT_PREEMPHASIS_TAU 50e-6 // Europe

//#define USB

#include "../lib/constants.h"
#include "../lib/oscillator.h"
#include "../lib/filters.h"
#include "../lib/hilbert.h"
#include "../lib/fm_modulator.h"

#define SAMPLE_RATE 192000 // Don't go lower than 108 KHz, becuase it (53000*2) and (38000+15000)

#define INPUT_DEVICE "real_real_tx_audio_input.monitor"
#define OUTPUT_DEVICE "alsa_output.platform-soc_sound.stereo-fallback"
// #define MPX_DEVICE ""
// #define SCA_DEVICE ""

#define BUFFER_SIZE 512

#include <pulse/simple.h>
#include <pulse/error.h>
#include <alsa/asoundlib.h>

#define MONO_VOLUME 0.45f // L+R Signal
#define PILOT_VOLUME 0.09f // 19 KHz Pilot
#define STEREO_VOLUME 0.45f // L-R signal
#define SCA_VOLUME 0.1f
#define MPX_VOLUME 1.0f

#define LPF_CUTOFF 15000

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
    printf("fm95 (an FM Processor by radio95) version 1.0\n");
}
void show_help(char *name) {
    printf(
        "fm95 (an FM Processor by radio95)\n"
        "Usage: %s\n\n"
        "   -m,--mono       Force Mono [default: %d]\n"
        "   -s,--stereo     Force Stereo [default: %d]\n"
        "   -i,--input      Override input device [default: %s]\n"
        "   -o,--output     Override output device [default: %s]\n"
        "   -a,--alsa_out   Force alsa output [default: %d]\n"
        "   -p,--pulse_out  Force pulse output [default: %d]\n"
        "   -M,--mpx        Override MPX input device [default: %s]\n"
        "   -C,--sca        Override the SCA input device [default: %s]\n"
        "   -f,--sca_freq   Override the SCA frequency [default: %f]\n"
        "   -F,--sca_dev    Override the SCA deviation [default: %f]\n"
        "   -L,--sca_clip   Override the SCA clipper threshold [default: %f]\n"
        "   -c,--clipper    Override the clipper threshold [default: %f]\n"
        "   -P,--polar      Force Polar Stereo (does not take effect with -m%s)\n"
        "   -g,--ge         Force Zenith/GE stereo (does not take effect with -m%s)\n"
        "   -S,--ssb        Force SSB [default: %d]\n"
        "   -D,--dsb        Force DSB [default: %d]\n"
        "   -R,--preemp     Override preemphasis [default: %f]\n"
        ,name
        ,DEFAULT_STEREO^1
        ,DEFAULT_STEREO
        ,INPUT_DEVICE
        ,OUTPUT_DEVICE
        ,DEFAULT_ALSA_OUTPUT
        ,DEFAULT_ALSA_OUTPUT^1
        #ifdef MPX_DEVICE
        ,MPX_DEVICE
        #else
        ,"not set"
        #endif
        #ifdef SCA_DEVICE
        ,SCA_DEVICE
        #else
        ,"not set"
        #endif
        ,DEFAULT_SCA_FREQUENCY
        ,DEFAULT_SCA_DEVIATION
        ,DEFAULT_SCA_CLIPPER_THRESHOLD
        ,DEFAULT_CLIPPER_THRESHOLD
        ,(DEFAULT_STEREO_POLAR == 1) ? ", default" : ""
        ,(DEFAULT_STEREO_POLAR == 1) ? "" : ", default"
        ,DEFAULT_STEREO_SSB
        ,DEFAULT_STEREO_SSB^1
        ,DEFAULT_PREEMPHASIS_TAU
    );
}

int main(int argc, char **argv) {
    show_version();

    pa_simple *mpx_device;
    pa_simple *sca_device;
    pa_simple *output_device;
    snd_pcm_hw_params_t *output_params;
    snd_pcm_t *output_handle;

    float clipper_threshold = DEFAULT_CLIPPER_THRESHOLD;
    int stereo = DEFAULT_STEREO;
    int polar_stereo = DEFAULT_STEREO_POLAR;
    int ssb = DEFAULT_STEREO_SSB;

    float sca_frequency = DEFAULT_SCA_FREQUENCY;
    float sca_deviation = DEFAULT_SCA_DEVIATION;
    float sca_clipper_threshold = DEFAULT_SCA_CLIPPER_THRESHOLD;

    char audio_input_device[64] = INPUT_DEVICE;
    char audio_output_device[64] = OUTPUT_DEVICE;
    #ifndef MPX_DEVICE
    char audio_mpx_device[64] = "\0";
#else
    char audio_mpx_device[64] = MPX_DEVICE;
#endif
#ifndef SCA_DEVICE
    char audio_sca_device[64] = "\0";
#else
    char audio_sca_device[64] = SCA_DEVICE;
#endif
    int alsa_output = DEFAULT_ALSA_OUTPUT;
    float preemphasis_tau = DEFAULT_PREEMPHASIS_TAU;

    // #region Parse Arguments
    int opt;
    const char	*short_opt = "msi:o:apM:C:f:F:L:c:PgSDR:hv";
    struct option	long_opt[] =
	{
        {"mono",        no_argument,       NULL, 'm'},
        {"stereo",      no_argument,       NULL, 's'},
        {"input",       required_argument, NULL, 'i'},
        {"output",      required_argument, NULL, 'o'},
        {"alsa_out",    no_argument,       NULL, 'a'},
        {"pulse_out",   no_argument,       NULL, 'p'},
        {"mpx",         required_argument, NULL, 'M'},
        {"sca",         required_argument, NULL, 'C'},
        {"sca_freq",    required_argument, NULL, 'f'},
        {"sca_dev",     required_argument, NULL, 'F'},
        {"sca_clip",    required_argument, NULL, 'L'},
        {"clipper",     required_argument, NULL, 'c'},
        {"polar",       no_argument,       NULL, 'P'},
        {"ge",          no_argument,       NULL, 'g'},
        {"ssb",         no_argument,       NULL, 'S'},
        {"dsb",         no_argument,       NULL, 'D'},
        {"preemp",      no_argument,       NULL, 'R'},
        
        {"help",        no_argument,       NULL, 'h'},
        {"version",     no_argument,       NULL, 'v'},
        {0,             0,                 0,    0}  // No trailing comma here
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
            case 'C': //SCA in
                memcpy(audio_sca_device, optarg, 63);
                break;
            case 'f': //SCA freq
                sca_frequency = strtof(optarg, NULL);
                printf("Running with a SCA frequency of %f\n", sca_frequency);
                break;
            case 'F': //SCA deviation
                sca_deviation = strtof(optarg, NULL);
                printf("Running with a SCA deviation of %f\n", sca_deviation);
                break;
            case 'L': //SCA clip
                sca_clipper_threshold = strtof(optarg, NULL);
                printf("Running with a SCA clipper threshold of %f\n", sca_clipper_threshold);
                break;
            case 'c': //Clipper
                clipper_threshold = strtof(optarg, NULL);
                printf("Running with a clipper threshold of %f\n", clipper_threshold);
                break;
            case 'P': //Polar
                polar_stereo = 1;
                printf("Using polar stereo\n");
                break;
            case 'g': //GE
                polar_stereo = 0;
                printf("Using Zenith/GE stereo\n");
                break;
            case 'S': //SSB
                ssb = 1;
                printf("Using SSB\n");
                break;
            case 'D': //DSB
                ssb = 0;
                printf("Using DSB\n");
                break;
            case 'R': // Preemp
                preemphasis_tau = strtof(optarg, NULL)*0.000001;
                printf("Running with a premp of %f\n", preemphasis_tau);
                break;
            case 'v': // Version
                show_version();
                return 0;
            case 'h':
                show_help(argv[0]);
                return 1;
        }
    }
    // #endregion

    // #region Setup devices

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
        "fm95",
        PA_STREAM_RECORD,
        audio_input_device,
        "Main Audio Input",
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
            "fm95",
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
            pa_simple_free(input_device);
            return 1;
        }
    }
    if(strlen(audio_sca_device) != 0) {
        printf("Connecting to SCA device... (%s)\n", audio_sca_device);

        sca_device = pa_simple_new(
            NULL,
            "fm95",
            PA_STREAM_RECORD,
            audio_sca_device,
            "SCA Input",
            &mono_format,
            NULL,
            &input_buffer_atr,
            &opentime_pulse_error
        );
        if (!sca_device) {
            fprintf(stderr, "Error: cannot open SCA device: %s\n", pa_strerror(opentime_pulse_error));
            pa_simple_free(input_device);
            if(strlen(audio_mpx_device) != 0) pa_simple_free(mpx_device);
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
            if(strlen(audio_mpx_device) != 0) pa_simple_free(mpx_device);
            if(strlen(audio_sca_device) != 0) pa_simple_free(sca_device);
            return 1;
        }
    } else {
        int output_error = snd_pcm_open(&output_handle, audio_output_device, SND_PCM_STREAM_PLAYBACK, 0);
        if(output_error < 0) {
            fprintf(stderr, "Error: cannot open output device: %s\n", snd_strerror(output_error));
            pa_simple_free(input_device);
            if(strlen(audio_mpx_device) != 0) pa_simple_free(mpx_device);
            if(strlen(audio_sca_device) != 0) pa_simple_free(sca_device);
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
    // #endregion

    Oscillator pilot_osc;
    if(polar_stereo == 1) {
        init_oscillator(&pilot_osc, 31250.0, SAMPLE_RATE); // The stereo carrier itself, the stereo carrier in polar is modulated directly on 31.25 khz with a bit of a carrier
    } else {
        init_oscillator(&pilot_osc, 19000.0, SAMPLE_RATE); // Pilot, it's there to indicate stereo and as a refrence signal with the stereo carrier
    }

    FMModulator sca_mod;
    init_fm_modulator(&sca_mod, sca_frequency, sca_deviation, SAMPLE_RATE);

    HilbertTransformer hilbert; // An Hilbert shifts a signal in quadrature, generating the I/Q data
    init_hilbert(&hilbert);
    DelayLine monoDelay; // Hilbert introduces a delay of 99 samples, this should be here to sync the mono with stereo to a sample
    init_delay_line(&monoDelay, (HILBERT_TAPS-1)/2);

    ResistorCapacitor preemp_l, preemp_r;
    init_rc_tau(&preemp_l, preemphasis_tau, SAMPLE_RATE);
    init_rc_tau(&preemp_r, preemphasis_tau, SAMPLE_RATE);

    FrequencyFilter lpf_l, lpf_r;
    init_lpf(&lpf_l, LPF_CUTOFF, SAMPLE_RATE);
    init_lpf(&lpf_r, LPF_CUTOFF, SAMPLE_RATE);

    signal(SIGINT, stop);
    signal(SIGTERM, stop);
    
    int pulse_error;

    float audio_stereo_input[BUFFER_SIZE*2]; // Input from device, interleaved stereo
    float mpx_in[BUFFER_SIZE]; // Input from MPX device
    float sca_in[BUFFER_SIZE]; // Input from SCA device
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
        if(strlen(audio_sca_device) != 0) {
            if (pa_simple_read(sca_device, sca_in, sizeof(sca_in), &pulse_error) < 0) {
                fprintf(stderr, "Error reading from SCA device: %s\n", pa_strerror(pulse_error));
                to_run = 0;
                break;
            }
        }

        for (int i = 0; i < BUFFER_SIZE; i++) {
            float l_in = left[i];
            float r_in = right[i];
            float current_mpx_in = mpx_in[i];
            float current_sca_in = sca_in[i];

            float ready_l = apply_freqeuncy_filter(&lpf_l, r_in);
            float ready_r = apply_freqeuncy_filter(&lpf_r, l_in);
            ready_l = apply_pre_emphasis(&preemp_l, ready_l)*2;
            ready_r = apply_pre_emphasis(&preemp_r, ready_r)*2;
            ready_l = hard_clip(ready_l, clipper_threshold);
            ready_r = hard_clip(ready_r, clipper_threshold);

            float mono = (ready_l + ready_r) / 2.0f; // Stereo to Mono
            if(stereo == 1) {
                float stereo = (ready_l - ready_r) / 2.0f; // Also Stereo to Mono but a bit diffrent
                if(polar_stereo == 1) {
                    if(ssb) {
                        float stereo_carrier = get_oscillator_sin_multiplier_ni(&pilot_osc, 1); // Get stereo carrier via multiplication
                        float stereo_carrier_cos = get_oscillator_cos_sample(&pilot_osc); // Get Carrier Q of I/Q

                        float stereo_i, stereo_q;
                        stereo += 0.2;
                        apply_hilbert(&hilbert, stereo, &stereo_i, &stereo_q); // Compute I/Q
                        #ifdef USB
                        float signal = (stereo_i*stereo_carrier_cos+stereo_q*(stereo_carrier));
                        #else
                        float signal = (stereo_i*stereo_carrier_cos-stereo_q*(stereo_carrier));
                        #endif
                        output[i] = delay_line(&monoDelay, mono)*MONO_VOLUME +
                            signal*STEREO_VOLUME;
                        if(strlen(audio_mpx_device) != 0) output[i] += current_mpx_in*MPX_VOLUME;
                        if(strlen(audio_sca_device) != 0) output[i] += modulate_fm(&sca_mod, hard_clip(current_sca_in, sca_clipper_threshold))*SCA_VOLUME;
                    } else {
                        float stereo_carrier = get_oscillator_sin_sample(&pilot_osc);
                        output[i] = mono*MONO_VOLUME +
                            ((stereo+0.2)*stereo_carrier)*STEREO_VOLUME;
                        if(strlen(audio_mpx_device) != 0) output[i] += current_mpx_in*MPX_VOLUME;
                        if(strlen(audio_sca_device) != 0) output[i] += modulate_fm(&sca_mod, hard_clip(current_sca_in, sca_clipper_threshold))*SCA_VOLUME;
                    }
                } else {
                    if(ssb) {
                        float stereo_carrier = get_oscillator_sin_multiplier_ni(&pilot_osc, 2); // Get stereo carrier via multiplication
                        float stereo_carrier_cos = get_oscillator_cos_multiplier_ni(&pilot_osc, 2); // Get Carrier Q of I/Q
                        float pilot = get_oscillator_sin_sample(&pilot_osc);

                        float stereo_i, stereo_q;
                        apply_hilbert(&hilbert, stereo, &stereo_i, &stereo_q); // Compute I/Q

                        output[i] = delay_line(&monoDelay, mono)*MONO_VOLUME +
                            pilot*PILOT_VOLUME +
                            (stereo_i*stereo_carrier_cos+stereo_q*stereo_carrier)*STEREO_VOLUME;
                        if(strlen(audio_mpx_device) != 0) output[i] += current_mpx_in*MPX_VOLUME;
                        if(strlen(audio_sca_device) != 0) output[i] += modulate_fm(&sca_mod, hard_clip(current_sca_in, sca_clipper_threshold))*SCA_VOLUME;
                    } else {
                        float stereo_carrier = get_oscillator_sin_multiplier_ni(&pilot_osc,2);
                        float pilot = get_oscillator_sin_sample(&pilot_osc);
                        output[i] = mono*MONO_VOLUME +
                            pilot*PILOT_VOLUME +
                            (stereo*stereo_carrier)*STEREO_VOLUME;
                        if(strlen(audio_mpx_device) != 0) output[i] += current_mpx_in*MPX_VOLUME;
                        if(strlen(audio_sca_device) != 0) output[i] += modulate_fm(&sca_mod, hard_clip(current_sca_in, sca_clipper_threshold))*SCA_VOLUME;
                    }
                }
            } else {
                output[i] = mono*MONO_VOLUME; // Only Mono
                if(strlen(audio_mpx_device) != 0) output[i] += current_mpx_in*MPX_VOLUME;
                if(strlen(audio_sca_device) != 0) output[i] += modulate_fm(&sca_mod, hard_clip(current_sca_in, sca_clipper_threshold))*SCA_VOLUME;
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
    if(strlen(audio_sca_device) != 0) pa_simple_free(sca_device);
    if(alsa_output == 0) {
        pa_simple_free(output_device);
    } else {
        snd_pcm_drain(output_handle);
        snd_pcm_close(output_handle);
        snd_pcm_hw_params_free(output_params);
    }
    exit_delay_line(&monoDelay);
    return 0;
}
