#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#define buffer_maxlength 12288
#define buffer_tlength_fragsize 12288
#define buffer_prebuf 8

#define DEFAULT_STEREO 1
#define DEFAULT_STEREO_POLAR 0
#define DEFAULT_CLIPPER_THRESHOLD 1.0f
#define DEFAULT_SCA_FREQUENCY 67000.0f
#define DEFAULT_SCA_DEVIATION 7000.0f
#define DEFAULT_SCA_CLIPPER_THRESHOLD 1.0f
#define DEFAULT_PREEMPHASIS_TAU 50e-6 // Europe, the freedomers use 75µs
#define DEFAULT_MPX_POWER 3.0f // dbr, this is for BS412, simplest bs412
#define DEFAULT_MPX_DEVIATION 75000.0f // for BS412

#include "../lib/oscillator.h"
#include "../lib/filters.h"
#include "../lib/fm_modulator.h"
#include "../lib/optimization.h"
#include "../lib/bs412.h"
#include "../lib/gain_control.h"

#define DEFAULT_SAMPLE_RATE 192000

#define INPUT_DEVICE "FM_Audio.monitor"
#define OUTPUT_DEVICE "alsa_output.platform-soc_sound.stereo-fallback"
#define RDS_DEVICE "RDS.monitor"
#define MPX_DEVICE "FM_MPX.monitor"
// #define SCA_DEVICE ""

#define BUFFER_SIZE 2048

#include <pulse/simple.h>
#include <pulse/error.h>

#define DEFAULT_MASTER_VOLUME 1.0f // Volume of everything combined, for calibration
#define DEFAULT_AUDIO_VOLUME 1.0f // Audio volume, before clipper

#define MONO_VOLUME 0.45f
#define PILOT_VOLUME 0.09f
#define STEREO_VOLUME 0.3f
#define RDS_VOLUME 0.06f
#define RDS2_VOLUME 0.045f
#define SCA_VOLUME 0.1f
#define MPX_VOLUME 1.0f
#define MPX_CLIPPER_THRESHOLD 1.0f

static volatile sig_atomic_t to_run = 1;

void uninterleave(const float *input, float *left, float *right, size_t num_samples) {
#if USE_NEON
	size_t i = 0;
	for (; i + 3 < num_samples / 2; i += 4) {
		float32x4x2_t input_vec = vld2q_f32(input + i * 2);
		vst1q_f32(left + i, input_vec.val[0]);
		vst1q_f32(right + i, input_vec.val[1]);
	}
	for (; i < num_samples / 2; i++) {
		left[i] = input[i * 2];
		right[i] = input[i * 2 + 1];
	}
#else
	for (size_t i = 0; i < num_samples / 2; i++) {
		left[i] = input[i * 2];
		right[i] = input[i * 2 + 1];
	}
#endif
}

static void stop(int signum) {
	(void)signum;
	printf("\nReceived stop signal.\n");
	to_run = 0;
}

void show_version() {
	printf("fm95 (an FM Processor by radio95) version 1.5\n");
}
void show_help(char *name) {
	printf(
		"Usage: %s\n"
		"   -s,--stereo		Force Stereo [default: %d]\n"
		"   -i,--input		Override input device [default: %s]\n"
		"   -o,--output		Override output device [default: %s]\n"
		"   -M,--mpx		Override MPX input device [default: %s]\n"
		"   -r,--rds		Override RDS95 input device [default: %s]\n"
		"   -C,--sca		Override the SCA input device [default: %s]\n"
		"   -f,--sca_freq	Override the SCA frequency [default: %.1f]\n"
		"   -F,--sca_dev	Override the SCA deviation [default: %.2f]\n"
		"   -L,--sca_clip	Override the SCA clipper threshold [default: %.2f]\n"
		"   -c,--clipper	Override the clipper threshold [default: %.2f]\n"
		"   -P,--polar		Force Polar Stereo (does not take effect with -m%s)\n"
		"   -R,--preemp		Override preemphasis [default: %.2f µs]\n"
		"   -V,--calibrate	Enable Calibration mode [default: off]\n"
		"   -p,--power		Set the MPX power [default: %.1f]\n"
		"   -d,--mpx_dev	Set the MPX deviation [default: %.1f]\n"
		"   -A,--master_vol	Set master volume [default: %.3f]\n"
		"   -v,--audio_vol	Set audio volume [default: %.3f]\n"
		,name
		,DEFAULT_STEREO
		,INPUT_DEVICE
		,OUTPUT_DEVICE
		#ifdef MPX_DEVICE
		,MPX_DEVICE
		#else
		,"not set"
		#endif
		#ifdef RDS_DEVICE
		,RDS_DEVICE
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
		,DEFAULT_PREEMPHASIS_TAU/0.000001
		,DEFAULT_MPX_POWER
		,DEFAULT_MPX_DEVIATION
		,DEFAULT_MASTER_VOLUME
		,DEFAULT_AUDIO_VOLUME
	);
}

int main(int argc, char **argv) {
	show_version();

	pa_simple *mpx_device = NULL;
	pa_simple *rds_device = NULL;
	pa_simple *sca_device = NULL;

	pa_simple *input_device;
	pa_simple *output_device;

	float clipper_threshold = DEFAULT_CLIPPER_THRESHOLD;
	int stereo = DEFAULT_STEREO;
	int polar_stereo = DEFAULT_STEREO_POLAR;

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
#ifndef RDS_DEVICE
	char audio_rds_device[64] = "\0";
#else
	char audio_rds_device[64] = RDS_DEVICE;
#endif
#ifndef SCA_DEVICE
	char audio_sca_device[64] = "\0";
#else
	char audio_sca_device[64] = SCA_DEVICE;
#endif
	float preemphasis_tau = DEFAULT_PREEMPHASIS_TAU;

	int calibration_mode = 0;
	float mpx_power = DEFAULT_MPX_POWER;
	float mpx_deviation = DEFAULT_MPX_DEVIATION;
	float master_volume = DEFAULT_MASTER_VOLUME;
	float audio_volume = DEFAULT_AUDIO_VOLUME;

	int sample_rate = DEFAULT_SAMPLE_RATE;

	// #region Parse Arguments
	int opt;
	const char	*short_opt = "s::i:o:M:r:C:f:F:L:c:P::R:Vp:d:A:v:h";
	struct option	long_opt[] =
	{
		{"stereo",      optional_argument, NULL, 's'},
		{"input",       required_argument, NULL, 'i'},
		{"output",      required_argument, NULL, 'o'},
		{"mpx",         required_argument, NULL, 'M'},
		{"rds",         required_argument, NULL, 'r'},
		{"sca",         required_argument, NULL, 'C'},
		{"sca_freq",    required_argument, NULL, 'f'},
		{"sca_dev",     required_argument, NULL, 'F'},
		{"sca_clip",    required_argument, NULL, 'L'},
		{"clipper",     required_argument, NULL, 'c'},
		{"polar",       optional_argument,       NULL, 'P'},
		{"preemp",      required_argument,       NULL, 'R'},
		{"calibrate",     no_argument,       NULL, 'V'},
		{"power",     required_argument,       NULL, 'p'},
		{"mpx_dev",     required_argument,       NULL, 'd'},
		{"master_vol",     required_argument,       NULL, 'A'},
		{"audio_vol",     required_argument,       NULL, 'v'},

		{"help",        no_argument,       NULL, 'h'},
		{0,             0,                 0,    0}
	};

	while((opt = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1) {
		switch(opt) {
			case 's': // Stereo
				if(optarg) {
					stereo = atoi(optarg);
				} else {
					stereo = 1;
				}
				break;
			case 'i': // Input Device
				memcpy(audio_input_device, optarg, 63);
				break;
			case 'o': // Output Device
				memcpy(audio_output_device, optarg, 63);
				break;;
			case 'M': //MPX in
				memcpy(audio_mpx_device, optarg, 63);
				break;
			case 'r': // RDS in
				memcpy(audio_rds_device, optarg, 63);
				break;
			case 'C': //SCA in
				memcpy(audio_sca_device, optarg, 63);
				break;
			case 'f': //SCA freq
				sca_frequency = strtof(optarg, NULL);
				break;
			case 'F': //SCA deviation
				sca_deviation = strtof(optarg, NULL);
				break;
			case 'L': //SCA clip
				sca_clipper_threshold = strtof(optarg, NULL);
				break;
			case 'c': //Clipper
				clipper_threshold = strtof(optarg, NULL);
				break;
			case 'P': //Polar
				if(optarg) {
					polar_stereo = atoi(optarg);
				} else {
					polar_stereo = 1;
				}
				break;
			case 'R': // Preemp
				preemphasis_tau = strtof(optarg, NULL)*0.000001;
				break;
			case 'V': // Calibration
				calibration_mode = 1;
				break;
			case 'p': // Power
				mpx_power = strtof(optarg, NULL);
				break;
			case 'd': // MPX deviation
				mpx_deviation = strtof(optarg, NULL);
				if (mpx_deviation < 38000) {
					fprintf(stderr, "Warning: MPX deviation cannot be lower than 38000. Setting to 38000.\n");
					mpx_deviation = 38000;
				}
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
	// #endregion

	int mpx_on = (strlen(audio_mpx_device) != 0);
	int rds_on = (strlen(audio_rds_device) != 0);
	int sca_on = (strlen(audio_sca_device) != 0);

	// #region Setup devices

	// Define formats and buffer atributes
	pa_sample_spec stereo_format = {
		.format = PA_SAMPLE_FLOAT32NE,
		.channels = 2,
		.rate = sample_rate
	};
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

	printf("Connecting to input device... (%s)\n", audio_input_device);

	input_device = pa_simple_new(
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

	if(mpx_on) {
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
	if(rds_on) {
		printf("Connecting to RDS95 device... (%s)\n", audio_rds_device);

		rds_device = pa_simple_new(
			NULL,
			"fm95",
			PA_STREAM_RECORD,
			audio_rds_device,
			"RDS Input",
			&stereo_format,
			NULL,
			&input_buffer_atr,
			&opentime_pulse_error
		);
		if (!rds_device) {
			fprintf(stderr, "Error: cannot open RDS device: %s\n", pa_strerror(opentime_pulse_error));
			pa_simple_free(input_device);
			if(mpx_on) pa_simple_free(mpx_device);
			return 1;
		}
	}

	if(sca_on) {
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
			if(mpx_on) pa_simple_free(mpx_device);
			if(rds_on) pa_simple_free(rds_device);
			return 1;
		}
	}

	printf("Connecting to output device... (%s)\n", audio_output_device);

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
		if(mpx_on) pa_simple_free(mpx_device);
		if(rds_on) pa_simple_free(rds_device);
		if(sca_on) pa_simple_free(sca_device);
		return 1;
	}
	// #endregion

	if(calibration_mode) {
		Oscillator osc;
		init_oscillator(&osc, 400, sample_rate);

		signal(SIGINT, stop);
		signal(SIGTERM, stop);
		int pulse_error;
		float output[BUFFER_SIZE];

		while(to_run) {
			for (int i = 0; i < BUFFER_SIZE; i++) {
				output[i] = get_oscillator_sin_sample(&osc)*master_volume;
			}
			if (pa_simple_write(output_device, output, sizeof(output), &pulse_error) < 0) {
				fprintf(stderr, "Error writing to output device: %s\n", pa_strerror(pulse_error));
				to_run = 0;
				break;
			}
		}
		printf("Cleaning up...\n");
		pa_simple_free(input_device);
		if(mpx_on) pa_simple_free(mpx_device);
		if(rds_on) pa_simple_free(rds_device);
		if(sca_on) pa_simple_free(sca_device);
		pa_simple_free(output_device);
		return 0;
	}

	Oscillator osc;
	init_oscillator(&osc, polar_stereo ? 31250.0 : 4750, sample_rate);

	FMModulator sca_mod;
	init_fm_modulator(&sca_mod, sca_frequency, sca_deviation, sample_rate);

	ResistorCapacitor preemp_l, preemp_r;
	init_preemphasis(&preemp_l, preemphasis_tau, sample_rate);
	init_preemphasis(&preemp_r, preemphasis_tau, sample_rate);

	MPXPowerMeasurement power;
	init_modulation_power_measure(&power, sample_rate);
	MPXPowerMeasurement mpx_only_power;
	init_modulation_power_measure(&mpx_only_power, sample_rate);

	AGC agc;
	//            fs          target   min    max   attack  relese
	initAGC(&agc, sample_rate, 0.625f, 0.0f, 1.25f, 0.025f, 0.25f);

	signal(SIGINT, stop);
	signal(SIGTERM, stop);

	int pulse_error;

	float audio_stereo_input[BUFFER_SIZE*2];

	float rds1_rds2_in[BUFFER_SIZE*2] = {0};
	float rds1_in[BUFFER_SIZE] = {0};
	float rds2_in[BUFFER_SIZE] = {0};

	float mpx_in[BUFFER_SIZE] = {0};
	float sca_in[BUFFER_SIZE] = {0};
	float left[BUFFER_SIZE], right[BUFFER_SIZE];
	float output[BUFFER_SIZE];

	while (to_run) {
		if (pa_simple_read(input_device, audio_stereo_input, sizeof(audio_stereo_input), &pulse_error) < 0) {
			fprintf(stderr, "Error reading from input device: %s\n", pa_strerror(pulse_error));
			to_run = 0;
			break;
		}
		uninterleave(audio_stereo_input, left, right, BUFFER_SIZE*2);
		if(mpx_on) {
			if (pa_simple_read(mpx_device, mpx_in, sizeof(mpx_in), &pulse_error) < 0) {
				fprintf(stderr, "Error reading from MPX device: %s\n", pa_strerror(pulse_error));
				to_run = 0;
				break;
			}
		}
		if(rds_on) {
			if (pa_simple_read(rds_device, rds1_rds2_in, sizeof(rds1_rds2_in), &pulse_error) < 0) {
				fprintf(stderr, "Error reading from RDS95 device: %s\n", pa_strerror(pulse_error));
				to_run = 0;
				break;
			}
			uninterleave(rds1_rds2_in, rds1_in, rds2_in, BUFFER_SIZE*2);
		}
		if(sca_on) {
			if (pa_simple_read(sca_device, sca_in, sizeof(sca_in), &pulse_error) < 0) {
				fprintf(stderr, "Error reading from SCA device: %s\n", pa_strerror(pulse_error));
				to_run = 0;
				break;
			}
		}

		for (int i = 0; i < BUFFER_SIZE; i++) {
			float mpx = 0.0f;
			float audio = 0.0f;

			float l_in = left[i];
			float r_in = right[i];
			float current_mpx_in = mpx_in[i];
			float current_rds_in = rds1_in[i];
			float current_rds2_in = rds2_in[i];
			float current_sca_in = sca_in[i];

			float ready_l = apply_preemphasis(&preemp_l, l_in);
			float ready_r = apply_preemphasis(&preemp_r, r_in);
			ready_l = process_agc_stereo(&agc, ready_l, ready_r, &ready_r);
			ready_l = hard_clip(ready_l*audio_volume, clipper_threshold);
			ready_r = hard_clip(ready_r*audio_volume, clipper_threshold);

			float mono = (ready_l + ready_r) / 2.0f;
			audio = mono*MONO_VOLUME;
			float stereo_carrier = 0.0f;
			if(stereo) {
				float stereo = (ready_l - ready_r) / 2.0f;
				stereo_carrier = get_oscillator_sin_multiplier_ni(&osc, polar_stereo ? 1 : 8);

				if(polar_stereo) {
					audio += ((stereo+0.2)*stereo_carrier)*STEREO_VOLUME;
				} else {
					float pilot = get_oscillator_sin_multiplier_ni(&osc, 4);
					mpx += pilot*PILOT_VOLUME;
					audio += (stereo*stereo_carrier)*STEREO_VOLUME;
				}
			}
			if(rds_on && polar_stereo == 0) {
				float rds_carrier = get_oscillator_cos_multiplier_ni(&osc, 12);
				mpx += (current_rds_in*rds_carrier)*RDS_VOLUME;
				if(!sca_on) {
					float rds2_carrier_66 = get_oscillator_cos_multiplier_ni(&osc, 14);
					mpx += (current_rds2_in*rds2_carrier_66)*RDS2_VOLUME;
				}
			}
			if(mpx_on) mpx += hard_clip(current_mpx_in, MPX_CLIPPER_THRESHOLD)*MPX_VOLUME;
			if(sca_on) mpx += modulate_fm(&sca_mod, hard_clip(current_sca_in, sca_clipper_threshold))*SCA_VOLUME;

			float mpx_only = measure_mpx(&mpx_only_power, mpx * mpx_deviation);
			float mpower = measure_mpx(&power, (audio+mpx) * mpx_deviation);
			mpower = deviation_to_dbr(dbr_to_deviation(mpower) - dbr_to_deviation(mpx_only));
			if (mpower > mpx_power) {
				float excess_power = mpower - mpx_power;
				audio *= (dbr_to_deviation(-excess_power)/mpx_deviation);
			}

			audio = hard_clip(audio, 1-mpx);

			output[i] = (audio+mpx)*master_volume;
			if(rds_on || stereo) advance_oscillator(&osc);
		}

		if (pa_simple_write(output_device, output, sizeof(output), &pulse_error) < 0) {
			fprintf(stderr, "Error writing to output device: %s\n", pa_strerror(pulse_error));
			to_run = 0;
			break;
		}
	}
	printf("Cleaning up...\n");
	pa_simple_free(input_device);
	if(mpx_on) pa_simple_free(mpx_device);
	if(rds_on) pa_simple_free(rds_device);
	if(sca_on) pa_simple_free(sca_device);
	pa_simple_free(output_device);
	return 0;
}
