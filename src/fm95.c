#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <liquid/liquid.h>

#define LPF_ORDER 15

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
#define DEFAULT_DEVIATION 75000.0f // another way to set the volume

#include "../dsp/oscillator.h"
#include "../dsp/filters.h"
#include "../dsp/fm_modulator.h"
#include "../lib/optimization.h"
#include "../dsp/bs412.h"
#include "../dsp/gain_control.h"

#define DEFAULT_SAMPLE_RATE 192000

#define INPUT_DEVICE "FM_Audio.monitor"
#define OUTPUT_DEVICE "alsa_output.platform-soc_sound.stereo-fallback"
#define RDS_DEVICE "RDS.monitor"
#define MPX_DEVICE "FM_MPX.monitor"
#define SCA_DEVICE "\0" // Disabled

#define BUFFER_SIZE 2048

#include "../io/audio.h"

#define DEFAULT_MASTER_VOLUME 1.0f // Volume of everything combined, for calibration
#define DEFAULT_AUDIO_VOLUME 1.0f // Audio volume, before clipper

#define MONO_VOLUME 0.45f // 45%
#define PILOT_VOLUME 0.09f // 9%
#define STEREO_VOLUME 0.3f // 30%
#define RDS_VOLUME 0.06f // 6%
#define RDS2_VOLUME 0.045f // 4.5%
#define SCA_VOLUME 0.1f // 10%, needs to be high because this is analog

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
	printf("fm95 (an FM Processor by radio95) version 1.6\n");
}
void show_help(char *name) {
	printf(
		"Usage: \t%s\n"
		"\t-s,--stereo\tForce Stereo [default: %d]\n"
		"\t-i,--input\tOverride input device [default: %s]\n"
		"\t-o,--output\tOverride output device [default: %s]\n"
		"\t-M,--mpx\tOverride MPX input device [default: %s]\n"
		"\t-r,--rds\tOverride RDS95 input device [default: %s]\n"
		"\t-C,--sca\tOverride the SCA input device [default: %s]\n"
		"\t-f,--sca_freq\tOverride the SCA frequency [default: %.1f]\n"
		"\t-F,--sca_dev\tOverride the SCA deviation [default: %.2f]\n"
		"\t-L,--sca_clip\tOverride the SCA clipper threshold [default: %.2f]\n"
		"\t-c,--clipper\tOverride the clipper threshold [default: %.2f]\n"
		"\t-P,--polar\tForce Polar Stereo (does not take effect with -m%s)\n"
		"\t-R,--preemp\tOverride preemphasis [default: %.2f µs]\n"
		"\t-V,--calibrate\tEnable Calibration mode [default: off]\n"
		"\t-p,--power\tSet the MPX power [default: %.1f]\n"
		"\t-d,--mpx_dev\tSet the MPX deviation [default: %.1f]\n"
		"\t-A,--master_vol\tSet master volume [default: %.3f]\n"
		"\t-v,--audio_vol\tSet audio volume [default: %.3f]\n"
		"\t-D,--deviation\tSet audio volume, but with the deviation (100%% being 75000) [default: %.1f]\n"
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
		,DEFAULT_DEVIATION
	);
}

int main(int argc, char **argv) {
	show_version();

	PulseInputDevice mpx_device, rds_device, sca_device;

	PulseInputDevice input_device;
	PulseOutputDevice output_device;

	float clipper_threshold = DEFAULT_CLIPPER_THRESHOLD;
	uint8_t stereo = DEFAULT_STEREO;
	uint8_t polar_stereo = DEFAULT_STEREO_POLAR;

	float sca_frequency = DEFAULT_SCA_FREQUENCY;
	float sca_deviation = DEFAULT_SCA_DEVIATION;
	float sca_clipper_threshold = DEFAULT_SCA_CLIPPER_THRESHOLD;

	char audio_input_device[64] = INPUT_DEVICE;
	char audio_output_device[64] = OUTPUT_DEVICE;
	char audio_mpx_device[64] = MPX_DEVICE;
	char audio_rds_device[64] = RDS_DEVICE;
	char audio_sca_device[64] = SCA_DEVICE;
	float preemphasis_tau = DEFAULT_PREEMPHASIS_TAU;

	uint8_t calibration_mode = 0;
	float mpx_power = DEFAULT_MPX_POWER;
	float mpx_deviation = DEFAULT_MPX_DEVIATION;
	float master_volume = DEFAULT_MASTER_VOLUME;
	float audio_volume = DEFAULT_AUDIO_VOLUME;

	uint32_t sample_rate = DEFAULT_SAMPLE_RATE;

	// #region Parse Arguments
	int opt;
	const char	*short_opt = "s::i:o:M:r:C:f:F:L:c:P::R:Vp:d:A:v:D:h";
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
		{"deviation",     required_argument,       NULL, 'D'},

		{"help",        no_argument,       NULL, 'h'},
		{0,             0,                 0,    0}
	};

	while((opt = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1) {
		switch(opt) {
			case 's': // Stereo
				if(optarg) stereo = atoi(optarg);
				else stereo = 1;
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
				if(optarg) polar_stereo = atoi(optarg);
				else polar_stereo = 1;
				break;
			case 'R': // Preemp
				preemphasis_tau = strtof(optarg, NULL)*1.0e-6f;
				break;
			case 'V': // Calibration
				calibration_mode = 1;
				break;
			case 'p': // Power
				mpx_power = strtof(optarg, NULL);
				break;
			case 'd': // MPX deviation
				mpx_deviation = strtof(optarg, NULL);
				break;
			case 'A': // Master vol
				master_volume = strtof(optarg, NULL);
				break;
			case 'v': // Audio Volume
				audio_volume = strtof(optarg, NULL);
				break;
			case 'D': // Deviation
				master_volume *= (strtof(optarg, NULL)/75000.0f);
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
	opentime_pulse_error = init_PulseInputDevice(&input_device, sample_rate, 2, "fm95", "Main Audio Input", audio_input_device, &input_buffer_atr);
	if (opentime_pulse_error) {
		fprintf(stderr, "Error: cannot open input device: %s\n", pa_strerror(opentime_pulse_error));
		return 1;
	}

	if(mpx_on) {
		printf("Connecting to MPX device... (%s)\n", audio_mpx_device);

		opentime_pulse_error = init_PulseInputDevice(&mpx_device, sample_rate, 1, "fm95", "MPX Input", audio_mpx_device, &input_buffer_atr);
		if (opentime_pulse_error) {
			fprintf(stderr, "Error: cannot open MPX device: %s\n", pa_strerror(opentime_pulse_error));
			free_PulseInputDevice(&input_device);
			return 1;
		}
	}
	if(rds_on) {
		printf("Connecting to RDS95 device... (%s)\n", audio_rds_device);

		opentime_pulse_error = init_PulseInputDevice(&rds_device, sample_rate, 2, "fm95", "RDS95 Input", audio_rds_device, &input_buffer_atr);
		if (opentime_pulse_error) {
			fprintf(stderr, "Error: cannot open RDS device: %s\n", pa_strerror(opentime_pulse_error));
			free_PulseInputDevice(&input_device);
			if(mpx_on) free_PulseInputDevice(&mpx_device);
			return 1;
		}
	}

	if(sca_on) {
		printf("Connecting to SCA device... (%s)\n", audio_sca_device);

		opentime_pulse_error = init_PulseInputDevice(&sca_device, sample_rate, 1, "fm95", "SCA Input", audio_sca_device, &input_buffer_atr);
		if (opentime_pulse_error) {
			fprintf(stderr, "Error: cannot open SCA device: %s\n", pa_strerror(opentime_pulse_error));
			free_PulseInputDevice(&input_device);
			if(mpx_on) free_PulseInputDevice(&mpx_device);
			if(rds_on) free_PulseInputDevice(&rds_device);
			return 1;
		}
	}

	printf("Connecting to output device... (%s)\n", audio_output_device);

	opentime_pulse_error = init_PulseOutputDevice(&output_device, sample_rate, 1, "fm95", "Main Audio Output", audio_output_device, &output_buffer_atr);
	if (opentime_pulse_error) {
		fprintf(stderr, "Error: cannot open output device: %s\n", pa_strerror(opentime_pulse_error));
		free_PulseInputDevice(&input_device);
		if(mpx_on) free_PulseInputDevice(&mpx_device);
		if(rds_on) free_PulseInputDevice(&rds_device);
		if(sca_on) free_PulseInputDevice(&sca_device);
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
			for (int i = 0; i < BUFFER_SIZE; i++) output[i] = get_oscillator_sin_sample(&osc)*master_volume;
			if((pulse_error = write_PulseOutputDevice(&output_device, output, sizeof(output)))) { // get output from the function and assign it into pulse_error, this comment to avoid confusion
				fprintf(stderr, "Error writing to output device: %s\n", pa_strerror(pulse_error));
				to_run = 0;
				break;
			}
		}
		printf("Cleaning up...\n");
		free_PulseInputDevice(&input_device);
		if(mpx_on) free_PulseInputDevice(&mpx_device);
		if(rds_on) free_PulseInputDevice(&rds_device);
		if(sca_on) free_PulseInputDevice(&sca_device);
		free_PulseOutputDevice(&output_device);
		return 0;
	}

	Oscillator osc;
	init_oscillator(&osc, polar_stereo ? 31250.0 : 4750, sample_rate);

	FMModulator sca_mod;
	init_fm_modulator(&sca_mod, sca_frequency, sca_deviation, sample_rate);

	iirfilt_rrrf lpf_l, lpf_r;
	lpf_l = iirfilt_rrrf_create_prototype(LIQUID_IIRDES_CHEBY2, LIQUID_IIRDES_LOWPASS, LIQUID_IIRDES_SOS, LPF_ORDER, (15000.0f/sample_rate), 0.0f, 1.0f, 40.0f);
	lpf_r = iirfilt_rrrf_create_prototype(LIQUID_IIRDES_CHEBY2, LIQUID_IIRDES_LOWPASS, LIQUID_IIRDES_SOS, LPF_ORDER, (15000.0f/sample_rate), 0.0f, 1.0f, 40.0f);

	iirfilt_rrrf mpx_lpf = iirfilt_rrrf_create_prototype(LIQUID_IIRDES_BUTTER, LIQUID_IIRDES_LOWPASS, LIQUID_IIRDES_SOS, 2, (polar_stereo ? (46250.0f/sample_rate) : (53000.0f/sample_rate)), 0.0f, 1.0f, 1.0f);

	ResistorCapacitor preemp_l, preemp_r;
	init_preemphasis(&preemp_l, preemphasis_tau, sample_rate);
	init_preemphasis(&preemp_r, preemphasis_tau, sample_rate);

	MPXPowerMeasurement power;
	init_modulation_power_measure(&power, sample_rate);
	MPXPowerMeasurement mpx_only_power;
	init_modulation_power_measure(&mpx_only_power, sample_rate);

	float bs412_audio_gain = 1.0f;

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
		if((pulse_error = read_PulseInputDevice(&input_device, audio_stereo_input, sizeof(audio_stereo_input)))) { // get output from the function and assign it into pulse_error, this comment to avoid confusion
			fprintf(stderr, "Error reading from input device: %s\n", pa_strerror(pulse_error));
			to_run = 0;
			break;
		}
		uninterleave(audio_stereo_input, left, right, BUFFER_SIZE*2);
		if(mpx_on) {
			if((pulse_error = read_PulseInputDevice(&mpx_device, mpx_in, sizeof(mpx_in)))) {
				fprintf(stderr, "Error reading from MPX device: %s\n", pa_strerror(pulse_error));
				to_run = 0;
				break;
			}
		}
		if(rds_on) {
			if((pulse_error = read_PulseInputDevice(&rds_device, rds1_rds2_in, sizeof(rds1_rds2_in)))) {
				fprintf(stderr, "Error reading from RDS95 device: %s\n", pa_strerror(pulse_error));
				to_run = 0;
				break;
			}
			uninterleave(rds1_rds2_in, rds1_in, rds2_in, BUFFER_SIZE*2);
		}
		if(sca_on) {
			if((pulse_error = read_PulseInputDevice(&sca_device, sca_in, sizeof(sca_in)))) {
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

			iirfilt_rrrf_execute(lpf_l, ready_l, &ready_l);
			iirfilt_rrrf_execute(lpf_r, ready_r, &ready_r);

			ready_l = process_agc_stereo(&agc, ready_l, ready_r, &ready_r);
			ready_l = hard_clip(ready_l*audio_volume, clipper_threshold);
			ready_r = hard_clip(ready_r*audio_volume, clipper_threshold);

			float mono = (ready_l + ready_r) / 2.0f;
			audio = mono*MONO_VOLUME;
			if(stereo) {
				float stereo = (ready_l - ready_r) / 2.0f;
				float stereo_carrier = get_oscillator_sin_multiplier_ni(&osc, polar_stereo ? 1 : 8); // 31.25 or 38 KHz

				if(polar_stereo) audio += ((stereo+0.2)*stereo_carrier)*STEREO_VOLUME;
				else {
					float pilot = get_oscillator_sin_multiplier_ni(&osc, 4); // 19 KHz
					mpx += pilot*PILOT_VOLUME;
					audio += (stereo*stereo_carrier)*STEREO_VOLUME;
				}
			}
			if(rds_on && polar_stereo == 0) {
				float rds_carrier = get_oscillator_cos_multiplier_ni(&osc, 12); // 57 KHz
				mpx += (current_rds_in*rds_carrier)*RDS_VOLUME;
				if(!sca_on) {
					float rds2_carrier_66 = get_oscillator_cos_multiplier_ni(&osc, 14); // 66.5 KHz
					mpx += (current_rds2_in*rds2_carrier_66)*RDS2_VOLUME;
				}
			}
			if(mpx_on) mpx += hard_clip(current_mpx_in, MPX_CLIPPER_THRESHOLD)*MPX_VOLUME;
			if(sca_on) mpx += modulate_fm(&sca_mod, hard_clip(current_sca_in, sca_clipper_threshold))*SCA_VOLUME;

			float mpx_only = measure_mpx(&mpx_only_power, mpx * mpx_deviation);
			float mpower = measure_mpx(&power, (audio+mpx) * mpx_deviation); // Standard requires that the output is measured specifically
			if (mpower > mpx_power) {
				float excess_power = mpower - mpx_power;
				excess_power = deviation_to_dbr(dbr_to_deviation(excess_power) - dbr_to_deviation(mpx_only)); // make sure mpx is not included in the power to attenuate, because we'd be attuating the mpx signal for audio

				float target_gain = dbr_to_deviation(-excess_power)/mpx_deviation;
				bs412_audio_gain = 0.9f * bs412_audio_gain + 0.1f * target_gain;
				audio *= bs412_audio_gain;
			}

			audio = hard_clip(audio, 1.0f-mpx); // Prevent clipping, via clipping the audio signal with relation to the mpx signal

			iirfilt_rrrf_execute(mpx_lpf, audio, &audio);

			output[i] = (audio+mpx)*master_volume;
			if(rds_on || stereo) advance_oscillator(&osc);
		}

		if(write_PulseOutputDevice(&output_device, output, sizeof(output))) {
			fprintf(stderr, "Error writing to output device: %s\n", pa_strerror(pulse_error));
			to_run = 0;
			break;
		}
	}
	printf("Cleaning up...\n");
	iirfilt_rrrf_destroy(lpf_l);
	iirfilt_rrrf_destroy(lpf_r);
	iirfilt_rrrf_destroy(mpx_lpf);

	free_PulseInputDevice(&input_device);
	if(mpx_on) free_PulseInputDevice(&mpx_device);
	if(rds_on) free_PulseInputDevice(&rds_device);
	if(sca_on) free_PulseInputDevice(&sca_device);
	free_PulseOutputDevice(&output_device);
	return 0;
}
