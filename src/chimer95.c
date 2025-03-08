#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <string.h>

#define buffer_maxlength 12288
#define buffer_tlength_fragsize 12288
#define buffer_prebuf 32

#include "../lib/constants.h"
#include "../lib/oscillator.h"

#define FREQ 1000.0f
#define SAMPLE_RATE 4000

#define OUTPUT_DEVICE "alsa_output.platform-soc_sound.stereo-fallback"

#define BUFFER_SIZE 512

#include <pulse/simple.h>
#include <pulse/error.h>

#define MASTER_VOLUME 0.5f // Volume
#define OFFSET 0 // Offset in seconds

// Define pip and beep durations in milliseconds
#define PIP_DURATION 100 // 100ms pip
#define PIP_PAUSE 900    // 900ms pause between pips
#define BEEP_DURATION 500 // 500ms beep

volatile sig_atomic_t to_run = 1;
volatile sig_atomic_t playing_sequence = 0;
volatile int sequence_position = 0;
volatile int sequence_type = 0; // 0 = none, 1 = 29:56, 2 = 59:55
volatile time_t last_sequence_time = 0; // Track when we last played a sequence

static void stop(int signum) {
    (void)signum;
    printf("\nReceived stop signal.\n");
    to_run = 0;
}

void show_version() {
    printf("chimer95 (gts time signal encoder by radio95) version 1.0\n");
}

void show_help(char *name) {
    printf(
        "Usage: %s\n"
        "   -o,--output     Override output device [default: %s]\n"
        "   -F,--frequency  GTS Frequency [default: %.1f Hz]\n"
        "   -s,--samplerate Output Samplerate [default: %d]\n"
        "   -v,--volume     Output volume [default: %.2f]\n"
        "   -t,--offset     GTS Offset [default: %d s]\n"
        ,name
        ,OUTPUT_DEVICE
        ,FREQ
        ,SAMPLE_RATE
        ,MASTER_VOLUME
        ,OFFSET
    );
}

int main(int argc, char **argv) {
    show_version();

    pa_simple *output_device;
    char audio_output_device[64] = OUTPUT_DEVICE;
    float master_volume = MASTER_VOLUME;
    float freq = FREQ;
    int sample_rate = SAMPLE_RATE;
    int offset = OFFSET;

    // #region Parse Arguments
    int opt;
    const char *short_opt = "o:F:s:v:t:h";
    struct option long_opt[] =
    {
        {"output",     required_argument, NULL, 'o'},
        {"frequency",  required_argument, NULL, 'F'},
        {"samplerate", required_argument, NULL, 's'},
        {"volume",     required_argument, NULL, 'v'},
        {"offset",     required_argument, NULL, 't'}, // Changed from 'o' to 't' to avoid duplicate
        
        {"help",       no_argument,       NULL, 'h'},
        {0,            0,                 0,    0}
    };

    while((opt = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1) {
        switch(opt) {
            case 'o': // Output Device
                memcpy(audio_output_device, optarg, 63);
                audio_output_device[63] = '\0'; // Ensure null-termination
                break;
            case 'F': // Frequency
                freq = strtof(optarg, NULL);
                break;
            case 's': // Sample rate
                sample_rate = strtol(optarg, NULL, 10);
                break;
            case 'v': // Volume
                master_volume = strtof(optarg, NULL);
                break;
            case 't': // Offset (changed from 'o' to 't')
                offset = strtol(optarg, NULL, 10);
                break;
            case 'h':
                show_help(argv[0]);
                return 0; // Return 0 for help, not 1
        }
    }
    // #endregion

    printf("Configuration:\n");
    printf("  Output device: %s\n", audio_output_device);
    printf("  Frequency: %.1f Hz\n", freq);
    printf("  Sample rate: %d Hz\n", sample_rate);
    printf("  Volume: %.2f\n", master_volume);
    printf("  Time offset: %d seconds\n", offset);

    // #region Setup devices

    // Define formats and buffer atributes
    pa_sample_spec mono_format = {
        .format = PA_SAMPLE_FLOAT32NE,
        .channels = 1,
        .rate = sample_rate
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

    printf("Connecting to output device... (%s)\n", audio_output_device);

    output_device = pa_simple_new(
        NULL,
        "chimer95",
        PA_STREAM_PLAYBACK,
        audio_output_device,
        "GTS Output",
        &mono_format,
        NULL,
        &output_buffer_atr,
        &opentime_pulse_error
    );
    if (!output_device) {
        fprintf(stderr, "Error: cannot open output device: %s\n", pa_strerror(opentime_pulse_error));
        return 1;
    }
    // #endregion

    // #region Setup Filters/Modulaltors/Oscillators
    Oscillator osc;
    init_oscillator(&osc, freq, sample_rate);
    // #endregion

    signal(SIGINT, stop);
    signal(SIGTERM, stop);
    
    int pulse_error;
    float output[BUFFER_SIZE]; // MPX, this goes to the output
    
    // Parameters for the time signals
    int elapsed_samples = 0;
    int total_sequence_samples = 0;
    int sequence_completed = 0; // Flag to track if we've already reported completion

    // For 29:56 - Play pip ... pip ... pip ... pip ... beep (4.5 seconds total)
    // Each pip is 0.1s with 0.9s pause, and beep is 0.5s
    // Total: 4 pips + 4 pauses + 1 beep = 0.1*4 + 0.9*4 + 0.5 = 4.5 seconds
    int samples_29_56 = (int)(4.5 * sample_rate);
    
    // For 59:55 - Play pip ... at start and same pattern (5.5 seconds total)
    // This adds one more pip and pause to the start
    // Total: 5 pips + 5 pauses + 1 beep = 0.1*5 + 0.9*5 + 0.5 = 5.5 seconds
    int samples_59_55 = (int)(5.5 * sample_rate);

    // Calculate number of samples for each element
    int pip_samples = (int)((PIP_DURATION / 1000.0) * sample_rate);
    int pause_samples = (int)((PIP_PAUSE / 1000.0) * sample_rate);
    int beep_samples = (int)((BEEP_DURATION / 1000.0) * sample_rate);
    
    printf("Ready to play time signals.\n");
    printf("Will trigger at XX:29:%02d and XX:59:%02d\n", 56+offset, 55+offset);
    
    while (to_run) {
        // Clear the output buffer
        memset(output, 0, sizeof(output));
        
        time_t now = time(NULL);
        struct tm *utc_time = gmtime(&now);
        int minute = utc_time->tm_min;
        int second = utc_time->tm_sec;
        
        // Check if we need to start a time signal sequence
        // Only start a new sequence if we're not already playing one and 
        // if we haven't played this exact sequence already (using the timestamp check)
        if (minute == 29 && second == (56+offset) && !playing_sequence && difftime(now, last_sequence_time) >= 1.0) {
            printf("Starting 29:56 time signal sequence\n");
            playing_sequence = 1;
            sequence_type = 1; // 29:56 pattern
            elapsed_samples = 0;
            total_sequence_samples = samples_29_56;
            sequence_completed = 0;
            last_sequence_time = now;
        } else if (minute == 59 && second == (55+offset) && !playing_sequence && difftime(now, last_sequence_time) >= 1.0) {
            printf("Starting 59:55 time signal sequence\n");
            playing_sequence = 1;
            sequence_type = 2; // 59:55 pattern
            elapsed_samples = 0;
            total_sequence_samples = samples_59_55;
            sequence_completed = 0;
            last_sequence_time = now;
        }
        
        // If we're playing a sequence, generate the appropriate sounds
        if (playing_sequence) {
            for (int i = 0; i < BUFFER_SIZE; i++) {
                if (elapsed_samples >= total_sequence_samples) {
                    // End of sequence
                    if (!sequence_completed) {
                        printf("Time signal sequence completed\n");
                        sequence_completed = 1;
                    }
                    playing_sequence = 0;
                    output[i] = 0;
                } else {
                    // Determine if we should be playing a pip, beep, or silence
                    if (sequence_type == 1) { // 29:56 pattern: pip ... pip ... pip ... pip ... beep
                        int cycle_position = elapsed_samples;
                        int pip_cycle = pip_samples + pause_samples;
                        
                        if (cycle_position < 4 * pip_cycle) { // Four pips with pauses
                            int within_cycle = cycle_position % pip_cycle;
                            if (within_cycle < pip_samples) {
                                // Playing a pip
                                output[i] = get_oscillator_sin_sample(&osc) * master_volume;
                            } else {
                                // Silent pause
                                output[i] = 0;
                            }
                        } else if (cycle_position < 4 * pip_cycle + beep_samples) {
                            // Final beep
                            output[i] = get_oscillator_sin_sample(&osc) * master_volume;
                        } else {
                            // Silent after sequence
                            output[i] = 0;
                        }
                    } else if (sequence_type == 2) { // 59:55 pattern: pip ... pip ... pip ... pip ... pip ... beep
                        int cycle_position = elapsed_samples;
                        int pip_cycle = pip_samples + pause_samples;
                        
                        if (cycle_position < 5 * pip_cycle) { // Five pips with pauses
                            int within_cycle = cycle_position % pip_cycle;
                            if (within_cycle < pip_samples) {
                                // Playing a pip
                                output[i] = get_oscillator_sin_sample(&osc) * master_volume;
                            } else {
                                // Silent pause
                                output[i] = 0;
                            }
                        } else if (cycle_position < 5 * pip_cycle + beep_samples) {
                            // Final beep
                            output[i] = get_oscillator_sin_sample(&osc) * master_volume;
                        } else {
                            // Silent after sequence
                            output[i] = 0;
                        }
                    }
                    
                    elapsed_samples++;
                }
            }
        }
        
        if (pa_simple_write(output_device, output, sizeof(output), &pulse_error) < 0) {
            fprintf(stderr, "Error writing to output device: %s\n", pa_strerror(pulse_error));
            to_run = 0;
            break;
        }
        
        // Small delay to prevent CPU hogging when idle
        if (!playing_sequence) {
            struct timespec ts = {0, 10000000}; // 10ms pause when not playing
            nanosleep(&ts, NULL);
        }
    }
    
    printf("Cleaning up...\n");
    pa_simple_free(output_device);
    return 0;
}