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

#define FREQ 77500.0f          // DCF77 frequency is 77.5 kHz
#define SAMPLE_RATE 192000     // Higher sample rate for the carrier

#define OUTPUT_DEVICE "alsa_output.platform-soc_sound.stereo-fallback"

#define BUFFER_SIZE 512

#include <pulse/simple.h>
#include <pulse/error.h>

#define MASTER_VOLUME 0.5f      // Volume
#define OFFSET 0                // Offset in seconds

// DCF77 specific parameters
#define PULSE_0_DURATION 100    // 100ms for binary 0
#define PULSE_1_DURATION 200    // 200ms for binary 1
#define REDUCED_AMPLITUDE 0.15f // Reduced to 15% of normal amplitude during pulses
#define BIT_LENGTH 1000         // 1 second per bit

volatile sig_atomic_t to_run = 1;
volatile sig_atomic_t transmitting = 0;
volatile int bit_position = 0;
volatile int test_mode = 0;     // 0 = normal, 1 = test mode

// DCF77 bits array (59 bits, indexed 0-58)
volatile int dcf77_bits[60];    // 60th position is for the 1-second pause

static void stop(int signum) {
    (void)signum;
    printf("\nReceived stop signal.\n");
    to_run = 0;
}

int is_timezone_change_soon() {
    time_t now, in_an_hour;
    struct tm local_now, local_later;

    // Get current time
    time(&now);
    local_now = *localtime(&now);

    // Get time an hour from now
    in_an_hour = now + 3600; // 3600 seconds = 1 hour
    local_later = *localtime(&in_an_hour);

    // Return 1 if a time zone change is about to happen, otherwise 0
    return local_now.tm_isdst != local_later.tm_isdst;
}

// Function to calculate DCF77 bits based on current time
void calculate_dcf77_bits(time_t now, int *bits) {
    struct tm *t = localtime(&now); // Use local time instead of UTC
    
    // Initialize all bits to 0
    memset(bits, 0, 60 * sizeof(int));
    
    //bit[15] = 0; // Helper antenna
    bits[16] = is_timezone_change_soon();
    if(t->tm_isdst) {
        bits[17] = 1;
    } else {
        bits[18] = 1;
    }
    bits[20] = 1;
    
    // Bits 20-27: Minutes (BCD encoded)
    int minutes = t->tm_min;
    bits[21] = (minutes % 10) & 0x01;
    bits[22] = ((minutes % 10) >> 1) & 0x01;
    bits[23] = ((minutes % 10) >> 2) & 0x01;
    bits[24] = ((minutes % 10) >> 3) & 0x01;
    bits[25] = ((minutes / 10) & 0x01);
    bits[26] = ((minutes / 10) >> 1) & 0x01;
    bits[27] = ((minutes / 10) >> 2) & 0x01;
    
    // Bit 28: Even parity for minutes
    int parity = 0;
    for (int i = 21; i <= 27; i++) {
        parity ^= bits[i];
    }
    bits[28] = parity;
    
    // Bits 29-34: Hours (BCD encoded)
    int hours = t->tm_hour;
    bits[29] = (hours % 10) & 0x01;
    bits[30] = ((hours % 10) >> 1) & 0x01;
    bits[31] = ((hours % 10) >> 2) & 0x01;
    bits[32] = ((hours % 10) >> 3) & 0x01;
    bits[33] = ((hours / 10) & 0x01);
    bits[34] = ((hours / 10) >> 1) & 0x01;
    
    // Bit 35: Even parity for hours
    parity = 0;
    for (int i = 29; i <= 34; i++) {
        parity ^= bits[i];
    }
    bits[35] = parity;
    
    // Bits 36-41: Day of month (1-31, BCD encoded)
    int day = t->tm_mday;
    bits[36] = (day % 10) & 0x01;
    bits[37] = ((day % 10) >> 1) & 0x01;
    bits[38] = ((day % 10) >> 2) & 0x01;
    bits[39] = ((day % 10) >> 3) & 0x01;
    bits[40] = ((day / 10) & 0x01);
    bits[41] = ((day / 10) >> 1) & 0x01;
    
    // Bits 42-44: Day of week (1=Monday, 7=Sunday)
    int dow = t->tm_wday == 0 ? 7 : t->tm_wday; // Convert Sunday from 0 to 7
    bits[42] = dow & 0x01;
    bits[43] = (dow >> 1) & 0x01;
    bits[44] = (dow >> 2) & 0x01;
    
    // Bits 45-49: Month (1-12, BCD encoded)
    int month = t->tm_mon + 1; // tm_mon is 0-11
    bits[45] = (month % 10) & 0x01;
    bits[46] = ((month % 10) >> 1) & 0x01;
    bits[47] = ((month % 10) >> 2) & 0x01;
    bits[48] = ((month % 10) >> 3) & 0x01;
    bits[49] = (month / 10) & 0x01;
    
    // Bits 50-57: Year within century (0-99, BCD encoded)
    int year = t->tm_year % 100; // Get last two digits of year
    bits[50] = (year % 10) & 0x01;
    bits[51] = ((year % 10) >> 1) & 0x01;
    bits[52] = ((year % 10) >> 2) & 0x01;
    bits[53] = ((year % 10) >> 3) & 0x01;
    bits[54] = ((year / 10) & 0x01);
    bits[55] = ((year / 10) >> 1) & 0x01;
    bits[56] = ((year / 10) >> 2) & 0x01;
    bits[57] = ((year / 10) >> 3) & 0x01;
    
    // Bit 58: Even parity for date bits
    parity = 0;
    for (int i = 36; i <= 57; i++) {
        parity ^= bits[i];
    }
    bits[58] = parity;
    
    // Bit 59: Always 0 (no pulse during minute marker)
    bits[59] = 0; 
}

// Print the current DCF77 bit pattern (for debugging)
void print_dcf77_bits(const int *bits) {
    printf("DCF77 Bit Pattern: ");
    for (int i = 0; i < 60; i++) {
        printf("%d", bits[i]);
        if ((i+1) % 10 == 0) printf(" "); // Space every 10 bits
    }
    printf("\n");
}

void show_version() {
    printf("dcf95 (DCF77 time signal encoder by radio95) version 1.0\n");
}

void show_help(char *name) {
    printf(
        "Usage: %s\n"
        "   -o,--output     Override output device [default: %s]\n"
        "   -F,--frequency  DCF77 Frequency [default: %.1f Hz]\n"
        "   -s,--samplerate Output Samplerate [default: %d]\n"
        "   -v,--volume     Output volume [default: %.2f]\n"
        "   -t,--offset     Time Offset [default: %d s]\n"
        "   -T,--test       Enable test mode (continuously generates signal)\n"
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
    int test_mode = 0; // Test mode flag

    // #region Parse Arguments
    int opt;
    const char *short_opt = "o:F:s:v:t:Th";
    struct option long_opt[] =
    {
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
            case 't': // Offset
                offset = strtol(optarg, NULL, 10);
                break;
            case 'T': // Test mode
                test_mode = 1;
                break;
            case 'h':
                show_help(argv[0]);
                return 0;
        }
    }
    // #endregion

    if(test_mode) {
        time_t now = time(NULL) + offset;
        calculate_dcf77_bits(now, (int *)dcf77_bits);
        print_dcf77_bits((int *)dcf77_bits);
        return 0;
    }

    printf("Configuration:\n");
    printf("  Output device: %s\n", audio_output_device);
    printf("  Frequency: %.1f Hz\n", freq);
    printf("  Sample rate: %d Hz\n", sample_rate);
    printf("  Volume: %.2f\n", master_volume);
    printf("  Time offset: %d seconds\n", offset);

    // #region Setup devices
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
        "dcf95",
        PA_STREAM_PLAYBACK,
        audio_output_device,
        "DCF77 Output",
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

    // #region Setup Oscillator
    Oscillator osc;
    init_oscillator(&osc, freq, sample_rate);
    // #endregion

    signal(SIGINT, stop);
    signal(SIGTERM, stop);
    
    int pulse_error;
    float output[BUFFER_SIZE]; // Output buffer
    
    // DCF77 parameters
    int elapsed_samples = 0;
    int current_second = -1;
    int ms_within_second = 0;
    int last_bit = -1;
    
    // Pre-calculate samples for different durations
    int bit_samples = (int)((BIT_LENGTH / 1000.0) * sample_rate);
    int pulse_0_samples = (int)((PULSE_0_DURATION / 1000.0) * sample_rate);
    int pulse_1_samples = (int)((PULSE_1_DURATION / 1000.0) * sample_rate);
    
    printf("DCF77 encoder ready.\n");
    printf("Will transmit time signal continuously.\n");
    
    // Main loop
    while (to_run) {
        // Clear the output buffer
        memset(output, 0, sizeof(output));
        
        // Get current time
        time_t now = time(NULL) + offset;
        struct tm *t = localtime(&now);
        int second = t->tm_sec;
        
        // Check if we're at the start of a new minute
        if (second == 0 && current_second != 0) {
            // Calculate the DCF77 bits for the new minute
            calculate_dcf77_bits(now, (int *)dcf77_bits);
            print_dcf77_bits((int *)dcf77_bits);
            
            // Reset counters for the new minute
            bit_position = 0;
            elapsed_samples = 0;
            transmitting = 1;
            
            printf("Starting new DCF77 transmission for %02d:%02d:%02d UTC\n", 
                   t->tm_hour, t->tm_min, t->tm_sec);
        }
        
        // Update the current second if it has changed
        if (second != current_second) {
            current_second = second;
            
            // Update the bit position at the start of each second
            if (transmitting) {
                if (bit_position < 59) {
                    printf("Bit %2d: %d\n", bit_position, dcf77_bits[bit_position]);
                    bit_position++;
                } else {
                    // End of minute (59 bits + 1 second pause)
                    bit_position = 0;
                    printf("End of minute, restarting bit sequence.\n");
                }
            }
            
            // Reset sample counter at the start of each second
            elapsed_samples = 0;
        }
        
        // Generate the DCF77 signal
        for (int i = 0; i < BUFFER_SIZE; i++) {
            // Get base carrier signal
            float carrier = get_oscillator_sin_sample(&osc);
            
            // Process in test mode or normal mode
            if (test_mode) {
                // In test mode, generate a repeating pattern regardless of time
                int cycle_position = elapsed_samples % bit_samples;
                int test_bit = (elapsed_samples / bit_samples) % 2; // Alternate 0 and 1
                
                if ((test_bit == 0 && cycle_position < pulse_0_samples) || 
                    (test_bit == 1 && cycle_position < pulse_1_samples)) {
                    // Reduced amplitude during pulse
                    output[i] = carrier * master_volume * REDUCED_AMPLITUDE;
                } else {
                    // Full amplitude otherwise
                    output[i] = carrier * master_volume;
                }
            } else if (transmitting) {
                // Calculate milliseconds within the current second
                ms_within_second = (int)((elapsed_samples * 1000.0) / sample_rate);
                
                // Get the current bit (between 0-58)
                int current_bit = bit_position > 0 ? bit_position - 1 : 59;
                
                // Determine if we should output reduced amplitude
                if ((dcf77_bits[current_bit] == 0 && ms_within_second < PULSE_0_DURATION) || 
                    (dcf77_bits[current_bit] == 1 && ms_within_second < PULSE_1_DURATION)) {
                    // Reduced amplitude during pulse
                    output[i] = carrier * master_volume * REDUCED_AMPLITUDE;
                } else {
                    // Full amplitude otherwise
                    output[i] = carrier * master_volume;
                }
            } else {
                // Not transmitting (should not happen in normal operation)
                output[i] = carrier * master_volume;
            }
            
            elapsed_samples++;
        }
        
        // Output the audio buffer
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