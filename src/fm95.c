#include <getopt.h>
#include <liquid/liquid.h>

#define LPF_ORDER 21

#define buffer_maxlength 12288
#define buffer_tlength_fragsize 12288
#define buffer_prebuf 8

#define DEFAULT_STEREO 1
#define DEFAULT_STEREO_POLAR 0
#define DEFAULT_RDS_STREAMS 2
#define DEFAULT_CLIPPER_THRESHOLD 1.0f
#define DEFAULT_SCA_FREQUENCY 67000.0f
#define DEFAULT_SCA_DEVIATION 7000.0f
#define DEFAULT_SCA_CLIPPER_THRESHOLD 1.0f
#define DEFAULT_PREEMPHASIS_TAU 50e-6 // Europe, the freedomers use 75µs (75e-6)
#define DEFAULT_MPX_POWER 3.0f // dbr, this is for BS412, simplest bs412
#define DEFAULT_MPX_DEVIATION 75000.0f // for BS412
#define DEFAULT_DEVIATION 75000.0f // another way to set the volume

#include "../dsp/oscillator.h"
#include "../filter/iir.h"
#include "../modulation/fm_modulator.h"
#include "../modulation/stereo_encoder.h"
#include "../lib/optimization.h"
#include "../filter/bs412.h"
#include "../filter/gain_control.h"

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
#define RDS_VOLUME 0.0475f // 4.75%
#define RDS_VOLUME_STEP 0.9f // 90%, so RDS2 stream 4 is 90% of stream 3 which is 90% of stream 2, which again is 90% of stream 1...
#define SCA_VOLUME 0.1f // 10%, needs to be high because this is analog. TODO: move sca to its own program (because then you can have as much scas as your computer allows to, here you have just one, and sca does not need any phase sync)

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
		"\t-s,--stereo\tForce Stereo [default: %d]\n"
		"\t-i,--input\tOverride input device [default: %s]\n"
		"\t-o,--output\tOverride output device [default: %s]\n"
		"\t-M,--mpx\tOverride MPX input device [default: %s]\n"
		"\t-r,--rds\tOverride RDS95 input device [default: %s]\n"
		"\t-R,--rds_strs\tSpecifies the number of the RDS streams provided by RDS95 [default: %d]\n"
		"\t-S,--sca\tOverride the SCA input device [default: %s]\n"
		"\t-f,--sca_freq\tOverride the SCA frequency [default: %.1f]\n"
		"\t-F,--sca_dev\tOverride the SCA deviation [default: %.2f]\n"
		"\t-C,--sca_clip\tOverride the SCA clipper threshold [default: %.2f]\n"
		"\t-c,--clipper\tOverride the clipper threshold [default: %.2f]\n"
		"\t-O,--polar\tForce Polar Stereo (does not take effect with -s0%s)\n"
		"\t-e,--preemp\tOverride preemphasis [default: %.2f µs]\n"
		"\t-V,--calibrate\tEnable Calibration mode [default: off, option 2 enables a 60 hz square wave instead of the 400 hz sine wave]\n"
		"\t-p,--power\tSet the MPX power [default: %.1f]\n"
		"\t-P,--mpx_dev\tSet the MPX deviation [default: %.1f]\n"
		"\t-A,--master_vol\tSet master volume [default: %.3f]\n"
		"\t-v,--audio_vol\tSet audio volume [default: %.3f]\n"
		"\t-D,--deviation\tSet audio volume, but with the deviation (100%% being 75000) [default: %.1f]\n"
		,name
		,DEFAULT_STEREO
		,INPUT_DEVICE
		,OUTPUT_DEVICE
		,MPX_DEVICE
		,RDS_DEVICE
		,DEFAULT_RDS_STREAMS
		,SCA_DEVICE
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
	printf("fm95 (an FM Processor by radio95) version 1.8\n");

	PulseInputDevice input_device, mpx_device, rds_device, sca_device;
	PulseOutputDevice output_device;

	float clipper_threshold = DEFAULT_CLIPPER_THRESHOLD;
	uint8_t stereo = DEFAULT_STEREO;
	uint8_t polar_stereo = DEFAULT_STEREO_POLAR;
	uint8_t rds_streams = DEFAULT_RDS_STREAMS;

	float sca_frequency = DEFAULT_SCA_FREQUENCY;
	float sca_deviation = DEFAULT_SCA_DEVIATION;
	float sca_clipper_threshold = DEFAULT_SCA_CLIPPER_THRESHOLD;

	char audio_input_device[48] = INPUT_DEVICE;
	char audio_output_device[48] = OUTPUT_DEVICE;
	char audio_mpx_device[48] = MPX_DEVICE;
	char audio_rds_device[48] = RDS_DEVICE;
	char audio_sca_device[48] = SCA_DEVICE;
	float preemphasis_tau = DEFAULT_PREEMPHASIS_TAU;

	uint8_t calibration_mode = 0;
	float max_mpx_power = DEFAULT_MPX_POWER;
	float mpx_deviation = DEFAULT_MPX_DEVIATION;
	float master_volume = DEFAULT_MASTER_VOLUME;
	float audio_volume = DEFAULT_AUDIO_VOLUME;

	uint32_t sample_rate = DEFAULT_SAMPLE_RATE;

	int opt;
	const char	*short_opt = "s::i:o:M:r:R:S:f:F:C:c:O::e:V::p:P:A:v:D:h";
	struct option	long_opt[] =
	{
		{"stereo",      optional_argument, NULL, 's'},
		{"input",       required_argument, NULL, 'i'},
		{"output",      required_argument, NULL, 'o'},
		{"mpx",         required_argument, NULL, 'M'},
		{"rds",         required_argument, NULL, 'r'},
		{"rds_strs",        required_argument, NULL, 'R'},
		{"sca",         required_argument, NULL, 'S'},
		{"sca_freq",    required_argument, NULL, 'f'},
		{"sca_dev",     required_argument, NULL, 'F'},
		{"sca_clip",    required_argument, NULL, 'C'},
		{"clipper",     required_argument, NULL, 'c'},
		{"polar",       optional_argument,       NULL, 'O'},
		{"preemp",      required_argument,       NULL, 'e'},
		{"calibrate",     optional_argument,       NULL, 'V'},
		{"power",     required_argument,       NULL, 'p'},
		{"mpx_dev",     required_argument,       NULL, 'P'},
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
				memcpy(audio_input_device, optarg, 47);
				break;
			case 'o': // Output Device
				memcpy(audio_output_device, optarg, 47);
				break;;
			case 'M': //MPX in
				memcpy(audio_mpx_device, optarg, 47);
				break;
			case 'r': // RDS in
				memcpy(audio_rds_device, optarg, 47);
				break;
			case 'R': // RDS Streams
				rds_streams = atoi(optarg);
				if(rds_streams > 4) {
					printf("Can't do more RDS streams than 4 (why even?)\n");
					exit(1);
				}
				break;
			case 'S': //SCA in
				memcpy(audio_sca_device, optarg, 47);
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
			case 'c': //Clipper
				clipper_threshold = strtof(optarg, NULL);
				break;
			case 'O': //Polar
				if(optarg) polar_stereo = atoi(optarg);
				else polar_stereo = 1;
				break;
			case 'e': // Preemp
				preemphasis_tau = strtof(optarg, NULL)*1.0e-6f;
				break;
			case 'V': // Calibration
				if(optarg) calibration_mode = atoi(optarg);
				else calibration_mode = 1;
				break;
			case 'p': // Power
				max_mpx_power = strtof(optarg, NULL);
				break;
			case 'P': // MPX deviation
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
	int rds_on = (strlen(audio_rds_device) != 0 && rds_streams != 0);
	int sca_on = (strlen(audio_sca_device) != 0);

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
	opentime_pulse_error = init_PulseInputDevice(&input_device, sample_rate, 2, "fm95", "Main Audio Input", audio_input_device, &input_buffer_atr, PA_SAMPLE_FLOAT32NE);
	if (opentime_pulse_error) {
		fprintf(stderr, "Error: cannot open input device: %s\n", pa_strerror(opentime_pulse_error));
		return 1;
	}

	if(mpx_on) {
		printf("Connecting to MPX device... (%s)\n", audio_mpx_device);

		opentime_pulse_error = init_PulseInputDevice(&mpx_device, sample_rate, 1, "fm95", "MPX Input", audio_mpx_device, &input_buffer_atr, PA_SAMPLE_FLOAT32NE);
		if (opentime_pulse_error) {
			fprintf(stderr, "Error: cannot open MPX device: %s\n", pa_strerror(opentime_pulse_error));
			free_PulseInputDevice(&input_device);
			return 1;
		}
	}
	if(rds_on) {
		printf("Connecting to RDS95 device... (%s)\n", audio_rds_device);

		opentime_pulse_error = init_PulseInputDevice(&rds_device, sample_rate, rds_streams, "fm95", "RDS95 Input", audio_rds_device, &input_buffer_atr, PA_SAMPLE_FLOAT32NE);
		if (opentime_pulse_error) {
			fprintf(stderr, "Error: cannot open RDS device: %s\n", pa_strerror(opentime_pulse_error));
			free_PulseInputDevice(&input_device);
			if(mpx_on) free_PulseInputDevice(&mpx_device);
			return 1;
		}
	}

	if(sca_on) {
		printf("Connecting to SCA device... (%s)\n", audio_sca_device);

		opentime_pulse_error = init_PulseInputDevice(&sca_device, sample_rate, 1, "fm95", "SCA Input", audio_sca_device, &input_buffer_atr, PA_SAMPLE_FLOAT32NE);
		if (opentime_pulse_error) {
			fprintf(stderr, "Error: cannot open SCA device: %s\n", pa_strerror(opentime_pulse_error));
			free_PulseInputDevice(&input_device);
			if(mpx_on) free_PulseInputDevice(&mpx_device);
			if(rds_on) free_PulseInputDevice(&rds_device);
			return 1;
		}
	}

	printf("Connecting to output device... (%s)\n", audio_output_device);

	opentime_pulse_error = init_PulseOutputDevice(&output_device, sample_rate, 1, "fm95", "Main Audio Output", audio_output_device, &output_buffer_atr, PA_SAMPLE_FLOAT32NE);
	if (opentime_pulse_error) {
		fprintf(stderr, "Error: cannot open output device: %s\n", pa_strerror(opentime_pulse_error));
		free_PulseInputDevice(&input_device);
		if(mpx_on) free_PulseInputDevice(&mpx_device);
		if(rds_on) free_PulseInputDevice(&rds_device);
		if(sca_on) free_PulseInputDevice(&sca_device);
		return 1;
	}

	if(calibration_mode != 0) {
		Oscillator osc;
		init_oscillator(&osc, (calibration_mode == 2) ? 60 : 400, sample_rate);

		signal(SIGINT, stop);
		signal(SIGTERM, stop);
		int pulse_error;
		float output[BUFFER_SIZE];

		while(to_run) {
			for (int i = 0; i < BUFFER_SIZE; i++) {
				float sample = get_oscillator_sin_sample(&osc);
				if(calibration_mode == 2) sample = (sample > 0.0f) ? 1.0f : -1.0f; // Sine wave to square wave filter
				output[i] = sample*master_volume;
			}
			if((pulse_error = write_PulseOutputDevice(&output_device, output, sizeof(output)))) { // get output from the function and assign it into pulse_error, this comment to avoid confusion
				if(pulse_error == -1) fprintf(stderr, "Main PulseOutputDevice reported as uninitialized.");
				else fprintf(stderr, "Error writing to output device: %s\n", pa_strerror(pulse_error));
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
	init_oscillator(&osc, polar_stereo ? 3906.25 : 4750, sample_rate); // 3906.25 * 8 = 31250.0, this is to reduce branching later on

	FMModulator sca_mod;
	init_fm_modulator(&sca_mod, sca_frequency, sca_deviation, sample_rate);

	iirfilt_rrrf lpf_l, lpf_r;
	lpf_l = iirfilt_rrrf_create_prototype(LIQUID_IIRDES_CHEBY2, LIQUID_IIRDES_LOWPASS, LIQUID_IIRDES_SOS, LPF_ORDER, (15000.0f/sample_rate), 0.0f, 1.0f, 40.0f);
	lpf_r = iirfilt_rrrf_create_prototype(LIQUID_IIRDES_CHEBY2, LIQUID_IIRDES_LOWPASS, LIQUID_IIRDES_SOS, LPF_ORDER, (15000.0f/sample_rate), 0.0f, 1.0f, 40.0f);

	ResistorCapacitor preemp_l, preemp_r;
	init_preemphasis(&preemp_l, preemphasis_tau, sample_rate, 15250.0f);
	init_preemphasis(&preemp_r, preemphasis_tau, sample_rate, 15250.0f);

	MPXPowerMeasurement power;
	init_modulation_power_measure(&power, sample_rate);

	StereoEncoder stencode;
	init_stereo_encoder(&stencode, 4.0f, &osc, polar_stereo, MONO_VOLUME, PILOT_VOLUME, STEREO_VOLUME);

	float bs412_audio_gain = 1.0f;

	AGC agc;
	//            fs           target   min   max   attack  release
	initAGC(&agc, sample_rate, 0.65f, 0.0f, 1.75f, 0.03f, 0.225f);

	signal(SIGINT, stop);
	signal(SIGTERM, stop);

	int pulse_error;

	float audio_stereo_input[BUFFER_SIZE*2]; // Stereo

	float *rds_in = malloc(sizeof(float) * BUFFER_SIZE * rds_streams);
	memset(rds_in, 0, sizeof(float) * BUFFER_SIZE * rds_streams);

	float mpx_in[BUFFER_SIZE] = {0};
	float sca_in[BUFFER_SIZE] = {0};
	float output[BUFFER_SIZE];

	while (to_run) {
		if((pulse_error = read_PulseInputDevice(&input_device, audio_stereo_input, sizeof(audio_stereo_input)))) { // get output from the function and assign it into pulse_error, this comment to avoid confusion
			if(pulse_error == -1) fprintf(stderr, "Main PulseInputDevice reported as uninitialized.");
			else fprintf(stderr, "Error reading from input device: %s\n", pa_strerror(pulse_error));
			to_run = 0;
			break;
		}
		if(mpx_on) {
			if((pulse_error = read_PulseInputDevice(&mpx_device, mpx_in, sizeof(mpx_in)))) {
				if(pulse_error == -1) fprintf(stderr, "MPX PulseInputDevice reported as uninitialized.");
				else fprintf(stderr, "Error reading from MPX device: %s\n", pa_strerror(pulse_error));
				to_run = 0;
				break;
			}
		}
		if(rds_on) {
			if((pulse_error = read_PulseInputDevice(&rds_device, rds_in, sizeof(float) * BUFFER_SIZE * rds_streams))) {
				if(pulse_error == -1) fprintf(stderr, "RDS95 PulseInputDevice reported as uninitialized.");
				else fprintf(stderr, "Error reading from RDS95 device: %s\n", pa_strerror(pulse_error));
				to_run = 0;
				break;
			}
		}
		if(sca_on) {
			if((pulse_error = read_PulseInputDevice(&sca_device, sca_in, sizeof(sca_in)))) {
				if(pulse_error == -1) fprintf(stderr, "SCA PulseInputDevice reported as uninitialized.");
				else fprintf(stderr, "Error reading from SCA device: %s\n", pa_strerror(pulse_error));
				to_run = 0;
				break;
			}
		}

		for (uint16_t i = 0; i < BUFFER_SIZE; i++) {
			float mpx = 0.0f;

			float ready_l = apply_preemphasis(&preemp_l, audio_stereo_input[2*i+0]);
			float ready_r = apply_preemphasis(&preemp_r, audio_stereo_input[2*i+1]);
			iirfilt_rrrf_execute(lpf_l, ready_l, &ready_l);
			iirfilt_rrrf_execute(lpf_r, ready_r, &ready_r);

			float agc_gain = process_agc(&agc, ((ready_l + ready_r) * 0.5f));
			ready_l *= agc_gain;
			ready_r *= agc_gain;

			ready_l = hard_clip(ready_l*audio_volume, clipper_threshold);
			ready_r = hard_clip(ready_r*audio_volume, clipper_threshold);

			mpx = stereo_encode(&stencode, stereo, ready_l, ready_r);

			if(rds_on && !polar_stereo) {
				for(uint8_t stream = 0; stream < rds_streams; stream++) {
					uint8_t osc_stream = 12+stream;
					if(osc_stream == 13) osc_stream++; // 61.75 KHz is not used, idk why but would be cool if it was
					mpx += (rds_in[rds_streams*i+stream]*get_oscillator_cos_multiplier_ni(&osc, osc_stream)) * (RDS_VOLUME * powf(RDS_VOLUME_STEP, stream));
				}
			}
			if(mpx_on) mpx += mpx_in[i];
			if(sca_on) mpx += modulate_fm(&sca_mod, hard_clip(sca_in[i], sca_clipper_threshold))*SCA_VOLUME;

			float mpx_power = measure_mpx(&power, mpx * mpx_deviation);
			if (mpx_power > max_mpx_power) {
				float excess_power = mpx_power - max_mpx_power;
				
				if (excess_power > 0.0f && excess_power < 10.0f) {
					float target_gain = dbr_to_deviation(-excess_power) / mpx_deviation;
					
					target_gain = fmaxf(target_gain, 0.1f);
					target_gain = fminf(target_gain, 1.0f);
					
					bs412_audio_gain = 0.85f * bs412_audio_gain + 0.15f * target_gain;
				}
			} else {
				bs412_audio_gain = fminf(1.0f, bs412_audio_gain + 0.001f);
			}

			mpx *= bs412_audio_gain;
			
			output[i] = hard_clip(mpx, 1.0f)*master_volume; // Ensure peak deviation of 75 khz, assuming we're calibrated correctly
			if(rds_on || stereo) advance_oscillator(&osc);
		}

		if((pulse_error = write_PulseOutputDevice(&output_device, output, sizeof(output)))) {
			if(pulse_error == -1) fprintf(stderr, "Main PulseOutputDevice reported as uninitialized.");
			else fprintf(stderr, "Error writing to output device: %s\n", pa_strerror(pulse_error));
			to_run = 0;
			break;
		}
	}
	printf("Cleaning up...\n");
	iirfilt_rrrf_destroy(lpf_l);
	iirfilt_rrrf_destroy(lpf_r);

	free_PulseInputDevice(&input_device);
	if(mpx_on) free_PulseInputDevice(&mpx_device);
	if(rds_on) free_PulseInputDevice(&rds_device);
	if(sca_on) free_PulseInputDevice(&sca_device);
	free_PulseOutputDevice(&output_device);
	free(rds_in);
	return 0;
}
