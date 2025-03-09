#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <string.h>

#define buffer_maxlength 2048
#define buffer_tlength_fragsize 2048
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

// Sequence types
#define SEQ_NONE 0
#define SEQ_29_56 1  
#define SEQ_59_55 2  
#define SEQ_TEST_HOUR 3

volatile sig_atomic_t to_run = 1;
volatile sig_atomic_t playing_sequence = 0;
volatile int sequence_position = 0;
volatile int sequence_type = SEQ_NONE;
volatile time_t last_sequence_time = 0;

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
        "   -T,--test       Enable test mode (plays full hour signal at end of every minute)\n"
        ,name
        ,OUTPUT_DEVICE
        ,FREQ
        ,SAMPLE_RATE
        ,MASTER_VOLUME
        ,OFFSET
    );
}

// Function to fill the buffer with generated signal
void generate_signal(float *output, int buffer_size, Oscillator *osc, float volume,
                    int *elapsed_samples, int total_samples, int pip_samples, 
                    int pause_samples, int beep_samples, int num_pips) {
    
    for (int i = 0; i < buffer_size; i++) {
        if (*elapsed_samples >= total_samples) {
            // End of sequence
            output[i] = 0;
            playing_sequence = 0;
        } else {
            int cycle_position = *elapsed_samples;
            int pip_cycle = pip_samples + pause_samples;
            
            if (cycle_position < num_pips * pip_cycle) {
                // Pips with pauses
                int within_cycle = cycle_position % pip_cycle;
                if (within_cycle < pip_samples) {
                    // Playing a pip
                    output[i] = get_oscillator_sin_sample(osc) * volume;
                } else {
                    // Silent pause
                    output[i] = 0;
                }
            } else if (cycle_position < num_pips * pip_cycle + beep_samples) {
                // Final beep
                output[i] = get_oscillator_sin_sample(osc) * volume;
            } else {
                // Silent after sequence
                output[i] = 0;
            }
            
            (*elapsed_samples)++;
        }
    }
}

// Check if it's time to start a sequence and which one
int check_time_for_sequence(int test_mode, int offset) {
    static time_t last_check = 0;
    static int last_minute = -1;
    
    // Only check time every 100ms to reduce system calls
    time_t now = time(NULL);
    if (now == last_check) {
        return SEQ_NONE;
    }
    
    last_check = now;
    struct tm *utc_time = gmtime(&now);
    int minute = utc_time->tm_min;
    int second = utc_time->tm_sec;
    
    // Check if we already played a sequence recently (within 1 second)
    if (difftime(now, last_sequence_time) < 1.0) {
        return SEQ_NONE;
    }
    
    // Check for 29:56 sequence
    if (minute == 29 && second == (56 + offset)) {
        last_sequence_time = now;
        return SEQ_29_56;
    }
    
    // Check for 59:55 sequence
    if (minute == 59 && second == (55 + offset)) {
        last_sequence_time = now;
        return SEQ_59_55;
    }
    
    // Check for test mode sequence (but don't repeat for the same minute)
    if (test_mode && second == (55 + offset) && minute != last_minute) {
        last_minute = minute;
        last_sequence_time = now;
        return SEQ_TEST_HOUR;
    }
    
    return SEQ_NONE;
}

int main(int argc, char **argv) {
    show_version();

    pa_simple *output_device;
    char audio_output_device[64] = OUTPUT_DEVICE;
    float master_volume = MASTER_VOLUME;
    float freq = FREQ;
    int sample_rate = SAMPLE_RATE;
    int offset = OFFSET;
    int test_mode = 0;

    // Parse command line arguments
    int opt;
    const char *short_opt = "o:F:s:v:t:Th";
    struct option long_opt[] = {
        {"output",     required_argument, NULL, 'o'},
        {"frequency",  required_argument, NULL, 'F'},
        {"samplerate", required_argument, NULL, 's'},
        {"volume",     required_argument, NULL, 'v'},
        {"offset",     required_argument, NULL, 't'},
        {"test",       no_argument,       NULL, 'T'},
        {"help",       no_argument,       NULL, 'h'},
        {0,            0,                 0,    0}
    };

    while((opt = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1) {
        switch(opt) {
            case 'o': 
                strncpy(audio_output_device, optarg, sizeof(audio_output_device) - 1);
                audio_output_device[sizeof(audio_output_device) - 1] = '\0';
                break;
            case 'F': 
                freq = strtof(optarg, NULL);
                break;
            case 's': 
                sample_rate = strtol(optarg, NULL, 10);
                break;
            case 'v': 
                master_volume = strtof(optarg, NULL);
                break;
            case 't': 
                offset = strtol(optarg, NULL, 10);
                break;
            case 'T': 
                test_mode = 1;
                break;
            case 'h':
                show_help(argv[0]);
                return 0;
        }
    }

    printf("Configuration:\n");
    printf("  Output device: %s\n", audio_output_device);
    printf("  Frequency: %.1f Hz\n", freq);
    printf("  Sample rate: %d Hz\n", sample_rate);
    printf("  Volume: %.2f\n", master_volume);
    printf("  Time offset: %d seconds\n", offset);
    printf("  Test mode: %s\n", test_mode ? "Enabled" : "Disabled");

    // Setup PulseAudio
    pa_sample_spec mono_format = {
        .format = PA_SAMPLE_FLOAT32NE,
        .channels = 1,
        .rate = sample_rate
    };

    pa_buffer_attr output_buffer_atr = {
        .maxlength = buffer_maxlength,
        .tlength = buffer_tlength_fragsize,
        .prebuf = buffer_prebuf
    };

    int pulse_error;

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
        &pulse_error
    );
    
    if (!output_device) {
        fprintf(stderr, "Error: cannot open output device: %s\n", pa_strerror(pulse_error));
        return 1;
    }

    // Setup oscillator
    Oscillator osc;
    init_oscillator(&osc, freq, sample_rate);

    signal(SIGINT, stop);
    signal(SIGTERM, stop);
    
    float output[BUFFER_SIZE];
    
    // Pre-calculate samples for each sound component
    int pip_samples = (int)((PIP_DURATION / 1000.0) * sample_rate);
    int pause_samples = (int)((PIP_PAUSE / 1000.0) * sample_rate);
    int beep_samples = (int)((BEEP_DURATION / 1000.0) * sample_rate);
    
    // Pre-calculate total sample lengths for each sequence type
    int samples_29_56 = 4 * (pip_samples + pause_samples) + beep_samples;
    int samples_59_55 = 5 * (pip_samples + pause_samples) + beep_samples;
    
    printf("Ready to play time signals.\n");
    printf("Will trigger at XX:29:%02d and XX:59:%02d\n", 56+offset, 55+offset);
    if (test_mode) {
        printf("TEST MODE: Will also play full hour signal at the end of every minute\n");
    }
    
    int elapsed_samples = 0;
    int total_sequence_samples = 0;
    int sequence_completed = 0;
    
    while (to_run) {
        // Only check for new sequence if we're not already playing one
        if (!playing_sequence) {
            int new_sequence = check_time_for_sequence(test_mode, offset);
            
            if (new_sequence != SEQ_NONE) {
                printf("Starting sequence type %d\n", new_sequence);
                playing_sequence = 1;
                sequence_type = new_sequence;
                elapsed_samples = 0;
                sequence_completed = 0;
                
                // Set total samples based on sequence type
                if (new_sequence == SEQ_29_56) {
                    total_sequence_samples = samples_29_56;
                } else {  // SEQ_59_55 or SEQ_TEST_HOUR
                    total_sequence_samples = samples_59_55;
                }
                
                // Clear the buffer when starting a new sequence
                memset(output, 0, sizeof(output));
            } else {
                // Idle state - send silence and sleep to save CPU
                // Only send silence occasionally to keep the stream open
                static int idle_counter = 0;
                if (idle_counter++ % 10 == 0) {
                    memset(output, 0, sizeof(output));
                    pa_simple_write(output_device, output, sizeof(output), &pulse_error);
                }
                
                struct timespec ts = {0, 10000000}; // 10ms sleep
                nanosleep(&ts, NULL);
                continue;
            }
        }
        
        // Generate signal for the current sequence
        int num_pips = (sequence_type == SEQ_29_56) ? 4 : 5;
        generate_signal(output, BUFFER_SIZE, &osc, master_volume, 
                       &elapsed_samples, total_sequence_samples,
                       pip_samples, pause_samples, beep_samples, num_pips);
        
        // Check if sequence just completed
        if (!playing_sequence && !sequence_completed) {
            printf("Time signal sequence completed\n");
            sequence_completed = 1;
        }
        
        // Write to audio device
        if (pa_simple_write(output_device, output, sizeof(output), &pulse_error) < 0) {
            fprintf(stderr, "Error writing to output device: %s\n", pa_strerror(pulse_error));
            to_run = 0;
            break;
        }
    }
    
    printf("Cleaning up...\n");
    pa_simple_free(output_device);
    return 0;
}