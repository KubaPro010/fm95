#include <stdio.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>

#define buffer_maxlength 1024
#define buffer_tlength_fragsize 1024
#define buffer_prebuf 0

#include "../dsp/oscillator.h"

#define DEFAULT_FREQ 1000.0f
#define DEFAULT_SAMPLE_RATE 4000

#define OUTPUT_DEVICE "FM_MPX"

#define BUFFER_SIZE 256

#include "../io/audio.h"

#define DEFAULT_MASTER_VOLUME 0.5f
#define DEFAULT_OFFSET 0

#define PIP_DURATION 100
#define PIP_PAUSE 900
#define BEEP_DURATION 500

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

void show_help(char *name) {
	printf(
		"Usage:\t%s\n"
		"\t-o,--output\tOverride output device [default: %s]\n"
		"\t-F,--frequency\tGTS Frequency [default: %.1f Hz]\n"
		"\t-s,--samplerate\tOutput Samplerate [default: %d]\n"
		"\t-v,--volume\tOutput volume [default: %.2f]\n"
		"\t-t,--offset\tGTS Offset [default: %d s]\n"
		"\t-T,--test\tEnable test mode (plays full hour signal at end of every minute)\n"
		,name
		,OUTPUT_DEVICE
		,DEFAULT_FREQ
		,DEFAULT_SAMPLE_RATE
		,DEFAULT_MASTER_VOLUME
		,DEFAULT_OFFSET
	);
}

void generate_signal(float *output, Oscillator *osc, float volume, int *elapsed_samples, int total_samples, int pip_samples, int pause_samples, int beep_samples, int num_pips) {
	for (int i = 0; i < BUFFER_SIZE; i++) {
		if (*elapsed_samples >= total_samples) {
			output[i] = 0;
			playing_sequence = 0;
		} else {
			int cycle_position = *elapsed_samples;
			int pip_cycle = pip_samples + pause_samples;

			if (cycle_position < num_pips * pip_cycle) {
				if ((cycle_position % pip_cycle) < pip_samples) output[i] = get_oscillator_sin_sample(osc) * volume;
				else output[i] = 0;
			} else if (cycle_position < num_pips * pip_cycle + beep_samples) output[i] = get_oscillator_sin_sample(osc) * volume;
			else output[i] = 0;

			(*elapsed_samples)++;
		}
	}
}

int check_time_for_sequence(int test_mode, int16_t offset) {
	static time_t last_check = 0;
	static int last_minute = -1;

	time_t now = time(NULL);
	if (now == last_check) return SEQ_NONE;

	last_check = now;
	struct tm *utc_time = gmtime(&now);
	int minute = utc_time->tm_min;
	int second = utc_time->tm_sec;

	if (difftime(now, last_sequence_time) < 1.0) return SEQ_NONE;

	last_sequence_time = now;
	if (minute == 29 && second == (56 + offset)) return SEQ_29_56;
	if (minute == 59 && second == (55 + offset)) return SEQ_59_55;
	if (test_mode && second == (55 + offset) && minute != last_minute) {
		last_minute = minute;
		return SEQ_TEST_HOUR;
	}

	return SEQ_NONE;
}

typedef struct
{
	float master_volume;
	float freq;
	uint32_t sample_rate;
	int16_t offset;
	bool test_mode;
} Chimer95_Config;
typedef struct
{
	PulseOutputDevice output_device;
} Chimer95_Runtime;

int run_chimer95(const Chimer95_Config config, Chimer95_Runtime* runtime) {
	int pulse_error;

	Oscillator osc;
	init_oscillator(&osc, config.freq, config.sample_rate);

	float output[BUFFER_SIZE];

	int pip_samples = (int)((PIP_DURATION / 1000.0) * config.sample_rate);
	int pause_samples = (int)((PIP_PAUSE / 1000.0) * config.sample_rate);
	int beep_samples = (int)((BEEP_DURATION / 1000.0) * config.sample_rate);

	int samples_29_56 = 4 * (pip_samples + pause_samples) + beep_samples;
	int samples_59_55 = 5 * (pip_samples + pause_samples) + beep_samples;

	printf("Ready to play time signals.\n");
	printf("Will trigger at XX:29:%02d and XX:59:%02d\n", 56+config.offset, 55+config.offset);
	if (config.test_mode) printf("TEST MODE: Will also play full hour signal at the end of every minute\n");

	int elapsed_samples = 0;
	int total_sequence_samples = 0;
	int sequence_completed = 0;

	while (to_run) {
		if (!playing_sequence) {
			int new_sequence = check_time_for_sequence(config.test_mode, config.offset);

			if (new_sequence != SEQ_NONE) {
				playing_sequence = 1;
				sequence_type = new_sequence;
				elapsed_samples = 0;
				sequence_completed = 0;

				if (new_sequence == SEQ_29_56) total_sequence_samples = samples_29_56;
				else total_sequence_samples = samples_59_55;

				memset(output, 0, sizeof(output));
			} else {
				static int idle_counter = 0;
				if (idle_counter++ % 10 == 0) {
					memset(output, 0, sizeof(output));
					if((pulse_error = write_PulseOutputDevice(&runtime->output_device, output, sizeof(output)))) {
						fprintf(stderr, "Error writing to output device: %s\n", pa_strerror(pulse_error));
						to_run = 0;
						break;
					}
				}

				struct timespec ts = {0, 5000000}; // 5ms sleep
				nanosleep(&ts, NULL);
				continue;
			}
		}

		int num_pips = (sequence_type == SEQ_29_56) ? 4 : 5;
		generate_signal(output, &osc, config.master_volume,
					   &elapsed_samples, total_sequence_samples,
					   pip_samples, pause_samples, beep_samples, num_pips);

		if (!playing_sequence && !sequence_completed) sequence_completed = 1;

		if((pulse_error = write_PulseOutputDevice(&runtime->output_device, output, sizeof(output)))) {
			fprintf(stderr, "Error writing to output device: %s\n", pa_strerror(pulse_error));
			to_run = 0;
			break;
		}
	}

	return 0;
}

int main(int argc, char **argv) {
	printf("chimer95 (GTS time signal encoder by radio95) version 1.2\n");

	char audio_output_device[64] = OUTPUT_DEVICE;

	Chimer95_Config config = {
		.master_volume = DEFAULT_MASTER_VOLUME,
		.freq = DEFAULT_FREQ,
		.sample_rate = DEFAULT_SAMPLE_RATE,
		.offset = DEFAULT_OFFSET,
		.test_mode = 0
	};

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
				config.freq = strtof(optarg, NULL);
				break;
			case 's':
				config.sample_rate = strtol(optarg, NULL, 10);
				break;
			case 'v':
				config.master_volume = strtof(optarg, NULL);
				break;
			case 't':
				config.offset = strtoul(optarg, NULL, 10);
				break;
			case 'T':
				config.test_mode = 1;
				break;
			case 'h':
				show_help(argv[0]);
				return 0;
		}
	}

	printf("Configuration:\n");
	printf("\tOutput device: %s\n", audio_output_device);
	printf("\tFrequency: %.1f Hz\n", config.freq);
	printf("\tSample rate: %d Hz\n", config.sample_rate);
	printf("\tVolume: %.2f\n", config.master_volume);
	printf("\tTime offset: %d seconds\n", config.offset);
	printf("\tTest mode: %s\n", config.test_mode ? "Enabled" : "Disabled");

	// Setup PulseAudio
	pa_buffer_attr output_buffer_atr = {
		.maxlength = buffer_maxlength,
		.tlength = buffer_tlength_fragsize,
		.prebuf = buffer_prebuf
	};

	Chimer95_Runtime runtime;
	memset(&runtime, 0, sizeof(runtime));

	printf("Connecting to output device... (%s)\n", audio_output_device);

	int pulse_error = init_PulseOutputDevice(&runtime.output_device, config.sample_rate, 1, "chimer95", "Main Audio Output", audio_output_device, &output_buffer_atr, PA_SAMPLE_FLOAT32NE);
	if (pulse_error) {
		fprintf(stderr, "Error: cannot open output device: %s\n", pa_strerror(pulse_error));
		return 1;
	}

	signal(SIGINT, stop);
	signal(SIGTERM, stop);

	int ret = run_chimer95(config, &runtime);
	printf("Cleaning up...\n");
	free_PulseOutputDevice(&runtime.output_device);
	return ret;
}