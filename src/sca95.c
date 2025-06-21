#include <getopt.h>
#include <stdio.h>

#define buffer_maxlength 12288
#define buffer_tlength_fragsize 12288
#define buffer_prebuf 8

#define DEFAULT_SCA_FREQUENCY 67000.0f
#define DEFAULT_SCA_DEVIATION 7000.0f
#define DEFAULT_SCA_CLIPPER_THRESHOLD 1.0f

#include "../modulation/fm_modulator.h"

#define DEFAULT_SAMPLE_RATE 192000

#define INPUT_DEVICE "SCA.monitor"
#define OUTPUT_DEVICE "FM_MPX"

#define BUFFER_SIZE 1024

#include "../io/audio.h"

#define DEFAULT_AUDIO_VOLUME 1.0f // Audio volume, before clipper

#define DEFAULT_VOLUME 0.1f

static volatile sig_atomic_t to_run = 1;

inline float hard_clip(float sample, float threshold) {
	return fmaxf(-threshold, fminf(threshold, sample));
}

static void stop(int signum) {
	(void)signum;
	printf("\nReceived stop signal.\n");
	to_run = 0;
}

void show_help(char *name) {
	printf(
		"Usage: \t%s\n"
		"\t-i,--input\tOverride input device [default: %s]\n"
		"\t-o,--output\tOverride output device [default: %s]\n"
		"\t-f,--sca_freq\tOverride the SCA frequency [default: %.1f]\n"
		"\t-F,--sca_dev\tOverride the SCA deviation [default: %.2f]\n"
		"\t-C,--sca_clip\tOverride the SCA clipper threshold [default: %.2f]\n"
		"\t-A,--master_vol\tSet master volume [default: %.3f]\n"
		,name
		,INPUT_DEVICE
		,OUTPUT_DEVICE
		,DEFAULT_SCA_FREQUENCY
		,DEFAULT_SCA_DEVIATION
		,DEFAULT_SCA_CLIPPER_THRESHOLD
		,DEFAULT_AUDIO_VOLUME
	);
}

int main(int argc, char **argv) {
	printf("sca95 (a SCA modulator by radio95) version 1.0\n");

	PulseInputDevice input_device;
	PulseOutputDevice output_device;

	float sca_frequency = DEFAULT_SCA_FREQUENCY;
	float sca_deviation = DEFAULT_SCA_DEVIATION;
	float sca_clipper_threshold = DEFAULT_SCA_CLIPPER_THRESHOLD;

	char audio_input_device[48] = INPUT_DEVICE;
	char audio_output_device[48] = OUTPUT_DEVICE;

	float master_volume = DEFAULT_VOLUME;
	float audio_volume = DEFAULT_AUDIO_VOLUME;

	uint32_t sample_rate = DEFAULT_SAMPLE_RATE;

	int opt;
	const char	*short_opt = "i:o:f:F:C:A:";
	struct option	long_opt[] =
	{
		{"input",       required_argument, NULL, 'i'},
		{"output",      required_argument, NULL, 'o'},
		{"sca_freq",    required_argument, NULL, 'f'},
		{"sca_dev",     required_argument, NULL, 'F'},
		{"sca_clip",    required_argument, NULL, 'C'},
		{"master_vol",     required_argument,       NULL, 'A'},
		{"output",     required_argument,       NULL, 'A'},
		{"audio_vol",     required_argument,       NULL, 'v'},

		{"help",        no_argument,       NULL, 'h'},
		{0,             0,                 0,    0}
	};

	while((opt = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1) {
		switch(opt) {
			case 'i': // Input Device
				memcpy(audio_input_device, optarg, 47);
				break;
			case 'o': // Output Device
				memcpy(audio_output_device, optarg, 47);
				break;
			case 'f': //SCA freq
				sca_frequency = strtof(optarg, NULL);
				break;
			case 'F': //SCA deviation
				sca_deviation = strtof(optarg, NULL);
				break;
			case 'C': //SCA clip
				sca_clipper_threshold = strtof(optarg, NULL);
				break;
			case 'A': // Master vol
				master_volume = strtof(optarg, NULL);
				break;
			case 'v': // Audio Volume
				audio_volume = strtof(optarg, NULL);
				break;
			case 'h':
				show_help(argv[0]);
				return 1;
		}
	}

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
	opentime_pulse_error = init_PulseInputDevice(&input_device, sample_rate, 1, "sca95", "Main Audio Input", audio_input_device, &input_buffer_atr, PA_SAMPLE_FLOAT32NE);
	if (opentime_pulse_error) {
		fprintf(stderr, "Error: cannot open input device: %s\n", pa_strerror(opentime_pulse_error));
		return 1;
	}

	printf("Connecting to output device... (%s)\n", audio_output_device);

	opentime_pulse_error = init_PulseOutputDevice(&output_device, sample_rate, 1, "sca95", "Main Audio Output", audio_output_device, &output_buffer_atr, PA_SAMPLE_FLOAT32NE);
	if (opentime_pulse_error) {
		fprintf(stderr, "Error: cannot open output device: %s\n", pa_strerror(opentime_pulse_error));
		free_PulseInputDevice(&input_device);
		return 1;
	}

	FMModulator sca_mod;
	init_fm_modulator(&sca_mod, sca_frequency, sca_deviation, sample_rate);

	signal(SIGINT, stop);
	signal(SIGTERM, stop);

	int pulse_error;

	float audio_input[BUFFER_SIZE];
	float output[BUFFER_SIZE];

	while (to_run) {
		if((pulse_error = read_PulseInputDevice(&input_device, audio_input, sizeof(audio_input)))) {
			if(pulse_error == -1) fprintf(stderr, "Main PulseInputDevice reported as uninitialized.");
			else fprintf(stderr, "Error reading from input device: %s\n", pa_strerror(pulse_error));
			to_run = 0;
			break;
		}

		for (uint16_t i = 0; i < BUFFER_SIZE; i++) {
			output[i] = modulate_fm(&sca_mod, hard_clip(audio_input[i]*audio_volume, sca_clipper_threshold))*master_volume;
		}

		if((pulse_error = write_PulseOutputDevice(&output_device, output, sizeof(output)))) {
			if(pulse_error == -1) fprintf(stderr, "Main PulseOutputDevice reported as uninitialized.");
			else fprintf(stderr, "Error writing to output device: %s\n", pa_strerror(pulse_error));
			to_run = 0;
			break;
		}
	}
	printf("Cleaning up...\n");

	free_PulseInputDevice(&input_device);
	free_PulseOutputDevice(&output_device);
	return 0;
}
