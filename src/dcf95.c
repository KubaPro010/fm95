#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <math.h>

#define buffer_maxlength 2048
#define buffer_tlength_fragsize 2048
#define buffer_prebuf 32

// #define DEBUG

#include "../lib/constants.h"
#include "../lib/oscillator.h"

#define DEFAULT_FREQ 77500.0f
#define DEFAULT_SAMPLE_RATE 192000

#define OUTPUT_DEVICE "alsa_output.platform-soc_sound.stereo-fallback"

#define BUFFER_SIZE 512

#include <pulse/simple.h>
#include <pulse/error.h>

#define DEFAULT_MASTER_VOLUME 0.5f
#define DEFAULT_OFFSET 0

#define PULSE_0_DURATION 100
#define PULSE_1_DURATION 200
#define REDUCED_AMPLITUDE 0.15f
#define BIT_LENGTH 1000 // this is ms

#define DSSS_START_MS 200
#define DSSS_DURATION_MS 793
#define PHASE_SHIFT 15.6
#define CHIPS_PER_BIT 512
#define CHIP_CYCLES 120

volatile sig_atomic_t to_run = 1;
volatile sig_atomic_t transmitting = 0;
volatile int bit_position = 0;
volatile int test_mode = 0;

volatile int dcf77_bits[60];

unsigned int lfsr = 0;

static void stop(int signum) {
	(void)signum;
	printf("\nReceived stop signal.\n");
	to_run = 0;
}

unsigned int generate_chip() {
	unsigned int chip = lfsr & 1;

	lfsr >>= 1;
	if (chip || !lfsr)
		lfsr ^= 0x110;

	return chip;
}

void reset_lfsr() {
	lfsr = 0;
}

int is_cet_dst(struct tm *tm_time) {
	int month = tm_time->tm_mon + 1;
	int day = tm_time->tm_mday;
	int hour = tm_time->tm_hour;

	if (month == 3) {
		int last_sunday = 31 - ((5 + 31) % 7);
		if ((day > last_sunday) || (day == last_sunday && hour >= 2)) {
			return 1;
		}
	} else if (month > 3 && month < 10) {
		return 1;
	} else if (month == 10) {
		int last_sunday = 31 - ((5 + 31) % 7);
		if ((day < last_sunday) || (day == last_sunday && hour < 3)) {
			return 1;
		}
	}

	return 0;
}

int is_timezone_change_soon() {
	time_t now, in_an_hour;
	struct tm cet_now, cet_later;

	time(&now);
	in_an_hour = now + 3600;

	memset(&cet_now, 0, sizeof(struct tm));
	memset(&cet_later, 0, sizeof(struct tm));

	struct tm *gm_now = gmtime(&now);
	struct tm *gm_later = gmtime(&in_an_hour);

	cet_now = *gm_now;
	cet_later = *gm_later;

	cet_now.tm_hour += 1;
	cet_later.tm_hour += 1;

	int is_dst_now = is_cet_dst(&cet_now);
	int is_dst_later = is_cet_dst(&cet_later);

	if (is_dst_now) cet_now.tm_hour += 1;
	if (is_dst_later) cet_later.tm_hour += 1;

	mktime(&cet_now);
	mktime(&cet_later);

	return is_dst_now != is_dst_later;
}

void calculate_dcf77_bits(time_t now, int *bits) {
	struct tm *t = gmtime(&now);
	int cest = is_cet_dst(t);

	memset(bits, 0, 60 * sizeof(int));

	bits[16] = is_timezone_change_soon();
	bits[17] = cest;
	bits[18] = !cest;
	bits[20] = 1;

	int minutes = t->tm_min;
    for (int i = 0; i < 4; i++) {
        bits[21 + i] = (minutes % 10 >> i) & 1;
    }
    for (int i = 0; i < 3; i++) {
        bits[25 + i] = (minutes / 10 >> i) & 1;
    }

    int minute_parity = 0;
    for (int i = 21; i <= 27; i++) {
        minute_parity ^= bits[i];
    }
    bits[28] = minute_parity;

	int hours = t->tm_hour;
	if(cest) hours += 1;
	for (int i = 0; i < 4; i++) {
        bits[29 + i] = (hours % 10 >> i) & 1;
    }
    for (int i = 0; i < 2; i++) {
        bits[33 + i] = (hours / 10 >> i) & 1;
    }

	int hour_parity = 0;
    for (int i = 29; i <= 34; i++) {
        hour_parity ^= bits[i];
    }
    bits[35] = hour_parity;

	int day = t->tm_mday;
	for (int i = 0; i < 4; i++) {
        bits[36 + i] = (day % 10 >> i) & 1;
    }
    for (int i = 0; i < 2; i++) {
        bits[40 + i] = (day / 10 >> i) & 1;
    }

	int dow = t->tm_wday == 0 ? 7 : t->tm_wday;
	bits[42] = dow & 0x01;
	bits[43] = (dow >> 1) & 0x01;
	bits[44] = (dow >> 2) & 0x01;

	int month = t->tm_mon + 1;
	for (int i = 0; i < 4; i++) {
        bits[45 + i] = (month % 10 >> i) & 1;
    }
	bits[49] = (month / 10) & 0x01;

	int year = t->tm_year % 100;
	for (int i = 0; i < 4; i++) {
        bits[50 + i] = (month % year >> i) & 1;
    }
	for (int i = 0; i < 4; i++) {
        bits[54 + i] = (year / 10 >> i) & 1;
    }

	int hour_parity = 0;
	for (int i = 36; i <= 57; i++) {
		hour_parity ^= bits[i];
	}
	bits[58] = hour_parity;

	bits[59] = 2;
}

void print_dcf77_bits(const int *bits) {
	printf("DCF77 Bit Pattern: ");
	for (int i = 0; i < 60; i++) {
		printf("%d", bits[i]);
		if ((i+1) % 10 == 0) printf(" ");
	}
	printf("\n");
}

void show_version() {
	printf("dcf95 (DCF77 time signal encoder by radio95) version 1.1\n");
}

void show_help(char *name) {
	printf(
		"Usage: %s\n"
		"   -o,--output     Override output device [default: %s]\n"
		"   -F,--frequency  DCF77 Frequency [default: %.1f Hz]\n"
		"   -s,--samplerate Output Samplerate [default: %d]\n"
		"   -v,--volume     Output volume [default: %.2f]\n"
		"   -t,--offset     Time Offset [default: %ds]\n"
		"   -T,--test       Enable test mode\n"
		"   -n,--no-phase   Disable phase modulation\n"
		,name
		,OUTPUT_DEVICE
		,DEFAULT_FREQ
		,DEFAULT_SAMPLE_RATE
		,DEFAULT_MASTER_VOLUME
		,DEFAULT_OFFSET
	);
}

int main(int argc, char **argv) {
	show_version();

	pa_simple *output_device;

	char audio_output_device[64] = OUTPUT_DEVICE;

	float master_volume = DEFAULT_MASTER_VOLUME;
	float freq = DEFAULT_FREQ;
	int sample_rate = DEFAULT_SAMPLE_RATE;
	int offset = DEFAULT_OFFSET;
	int test_mode = 0;
	int no_phase = 0;

	// #region Parse Arguments
	int opt;
	const char *short_opt = "o:F:s:v:t:Tnh";
	struct option long_opt[] =
	{
		{"output",     required_argument, NULL, 'o'},
		{"frequency",  required_argument, NULL, 'F'},
		{"samplerate", required_argument, NULL, 's'},
		{"volume",     required_argument, NULL, 'v'},
		{"offset",     required_argument, NULL, 't'},
		{"test",       no_argument,       NULL, 'T'},
		{"no-phase",   no_argument,       NULL, 'n'},

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
			case 'n': // Disable phase modulation
				no_phase = 1;
				break;
			case 'h':
				show_help(argv[0]);
				return 0;
		}
	}
	// #endregion

	if(test_mode) {
		time_t now = time(NULL) + offset + 60;
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
	if (no_phase) {
		printf("  Phase modulation: Disabled\n");
	} else {
		printf("  Phase modulation: +/- %.1f degrees\n", PHASE_SHIFT);
	}

	// #region Setup devices
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

	Oscillator osc;
	init_oscillator(&osc, freq, sample_rate);

	signal(SIGINT, stop);
	signal(SIGTERM, stop);

	int pulse_error;
	float output[BUFFER_SIZE];

	int current_second = -1;
	int ms_within_second = 0;

	int dsss_start_samples = (int)((DSSS_START_MS / 1000.0) * sample_rate);
	int dsss_duration_samples = (int)((DSSS_DURATION_MS / 1000.0) * sample_rate);
	int dsss_end_samples = dsss_start_samples + dsss_duration_samples;
	float phase_shift_rad = (PHASE_SHIFT * M_PI) / 180.0;

	int current_chip_count = 0;
	int current_cycle_count = 0;
	int in_dsss_period = 0;

	int elapsed_samples = 0;

	printf("DCF77 encoder ready.\n");

	while (to_run) {
		memset(output, 0, sizeof(output));

		time_t now = time(NULL) + offset + 60;
		struct tm *t = gmtime(&now);
		int second = t->tm_sec;

		if (second == 0 && current_second != 0) {
			calculate_dcf77_bits(now, (int *)dcf77_bits);
			#ifdef DEBUG
			print_dcf77_bits((int *)dcf77_bits);
			#endif

			bit_position = 0;
			elapsed_samples = 0;
			transmitting = 1;

			#ifdef DEBUG
			printf("Starting new DCF77 transmission for %02d:%02d:%02d UTC\n",
				   t->tm_hour, t->tm_min, t->tm_sec);
			#endif
		}

		if (second != current_second) {
			current_second = second;

			reset_lfsr();
			current_chip_count = 0;
			current_cycle_count = 0;

			if (transmitting) {
				if (bit_position < 59) {
					#ifdef DEBUG
					printf("Bit %2d: %d\n", bit_position, dcf77_bits[bit_position]);
					#endif
					bit_position++;
				} else {
					bit_position = 0;
					#ifdef DEBUG
					printf("End of minute, restarting bit sequence.\n");
					#endif
				}
			}

			elapsed_samples = 0;
		}

		for (int i = 0; i < BUFFER_SIZE; i++) {
			ms_within_second = (int)((elapsed_samples * 1000.0) / sample_rate);

			int current_bit = bit_position > 0 ? bit_position - 1 : 59;

			in_dsss_period = (elapsed_samples >= dsss_start_samples &&
							  elapsed_samples < dsss_end_samples);

			float phase_offset = 0.0;

			if (in_dsss_period && transmitting && !no_phase) {
				if (current_cycle_count == 0) {
					if (current_chip_count < CHIPS_PER_BIT) {
						unsigned int chip = generate_chip();

						unsigned int modulated_chip = chip ^ dcf77_bits[current_bit];

						if (modulated_chip == 0) {
							phase_offset = phase_shift_rad;
						} else {
							phase_offset = -phase_shift_rad;
						}

						current_chip_count++;
					}
				}

				current_cycle_count = (current_cycle_count + 1) % CHIP_CYCLES;
			}

			float t = osc.phase + phase_offset;
			float carrier = sinf(t);
			advance_oscillator(&osc);

			if (transmitting) {
				if ((dcf77_bits[current_bit] == 0 && ms_within_second < PULSE_0_DURATION) ||
					(dcf77_bits[current_bit] == 1 && ms_within_second < PULSE_1_DURATION)) {
					output[i] = carrier * master_volume * REDUCED_AMPLITUDE;
				} else {
					output[i] = carrier * master_volume;
				}
			} else {
				output[i] = carrier * master_volume;
			}

			elapsed_samples++;
		}

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