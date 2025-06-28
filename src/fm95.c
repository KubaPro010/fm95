#include <getopt.h>
#include <liquid/liquid.h>
#include <stdbool.h>

#define LPF_ORDER 17

#define buffer_maxlength 12288
#define buffer_tlength_fragsize 12288
#define buffer_prebuf 8

#define DEFAULT_STEREO 1
#define DEFAULT_STEREO_POLAR 0
#define DEFAULT_RDS_STREAMS 2
#define DEFAULT_CLIPPER_THRESHOLD 1.0f
#define DEFAULT_PREEMPHASIS_TAU 50e-6 // Europe, the freedomers use 75µs (75e-6)
#define DEFAULT_MPX_POWER 3.0f // dbr, this is for BS412, simplest bs412
#define DEFAULT_MPX_DEVIATION 75000.0f // for BS412
#define DEFAULT_DEVIATION 75000.0f // another way to set the volume

#include "../dsp/oscillator.h"
#include "../filter/iir.h"
#include "../modulation/fm_modulator.h"
#include "../modulation/stereo_encoder.h"
#include "../filter/bs412.h"
#include "../filter/gain_control.h"

#define DEFAULT_SAMPLE_RATE 192000

#define INPUT_DEVICE "FM_Audio.monitor"
#define OUTPUT_DEVICE "alsa_output.platform-soc_sound.stereo-fallback"
#define RDS_DEVICE "RDS.monitor"
#define MPX_DEVICE "FM_MPX.monitor"

#define BUFFER_SIZE 2048

#include "../io/audio.h"

#define DEFAULT_MASTER_VOLUME 1.0f // Volume of everything combined, for calibration
#define DEFAULT_AUDIO_VOLUME 1.0f // Audio volume, before clipper

#define MONO_VOLUME 0.45f // 45%
#define PILOT_VOLUME 0.09f // 9%
#define STEREO_VOLUME 0.3f // 30%
#define RDS_VOLUME 0.0475f // 4.75%
#define RDS_VOLUME_STEP 0.9f // 90%, so RDS2 stream 4 is 90% of stream 3 which is 90% of stream 2, which again is 90% of stream 1...

static volatile sig_atomic_t to_run = 1;

inline float hard_clip(float sample, float threshold) { return fmaxf(-threshold, fminf(threshold, sample)); }

typedef struct
{
	bool stereo;
	bool polar_stereo;

	uint8_t rds_streams;

	float clipper_threshold;
	float preemphasis;
	uint8_t calibration;
	float mpx_power;
	float mpx_deviation;
	float master_volume;
	float audio_volume;

	uint32_t sample_rate;
} FM95_Config;

typedef struct
{
	PulseInputDevice input_device, mpx_device, rds_device;
	PulseOutputDevice output_device;
} FM95_Runtime;

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

int run_fm95(FM95_Config config, FM95_Runtime* runtime) {
	int mpx_on = (runtime->mpx_device.initialized == 1);
	int rds_on = (runtime->rds_device.initialized == 1);

	if(config.calibration != 0) {
		Oscillator osc;
		init_oscillator(&osc, (config.calibration == 2) ? 60 : 400, config.sample_rate);

		int pulse_error;
		float output[BUFFER_SIZE];

		while(to_run) {
			for (int i = 0; i < BUFFER_SIZE; i++) {
				float sample = get_oscillator_sin_sample(&osc);
				if(config.calibration == 2) sample = (sample > 0.0f) ? 1.0f : -1.0f; // Sine wave to square wave filter
				output[i] = sample*config.master_volume;
			}
			if((pulse_error = write_PulseOutputDevice(&runtime->output_device, output, sizeof(output)))) { // get output from the function and assign it into pulse_error, this comment to avoid confusion
				if(pulse_error == -1) fprintf(stderr, "Main PulseOutputDevice reported as uninitialized.");
				else fprintf(stderr, "Error writing to output device: %s\n", pa_strerror(pulse_error));
				to_run = 0;
				break;
			}
		}
		return 0;
	}

	Oscillator osc;
	init_oscillator(&osc, config.polar_stereo ? 7812.5 : 4750, config.sample_rate);

	iirfilt_rrrf lpf_l, lpf_r;
	lpf_l = iirfilt_rrrf_create_prototype(LIQUID_IIRDES_CHEBY2, LIQUID_IIRDES_LOWPASS, LIQUID_IIRDES_SOS, LPF_ORDER, (15000.0f/config.sample_rate), 0.0f, 1.0f, 60.0f);
	lpf_r = iirfilt_rrrf_create_prototype(LIQUID_IIRDES_CHEBY2, LIQUID_IIRDES_LOWPASS, LIQUID_IIRDES_SOS, LPF_ORDER, (15000.0f/config.sample_rate), 0.0f, 1.0f, 60.0f);

	ResistorCapacitor preemp_l, preemp_r;
	init_preemphasis(&preemp_l, config.preemphasis, config.sample_rate, 15250.0f);
	init_preemphasis(&preemp_r, config.preemphasis, config.sample_rate, 15250.0f);

	MPXPowerMeasurement power;
	init_modulation_power_measure(&power, config.sample_rate);

	StereoEncoder stencode;
	init_stereo_encoder(&stencode, 4.0f, &osc, config.polar_stereo, MONO_VOLUME, PILOT_VOLUME, STEREO_VOLUME);

	float bs412_audio_gain = 1.0f;

	AGC agc;
	//            fs           target   min   max   attack  release
	initAGC(&agc, config.sample_rate, 0.65f, 0.0f, 1.75f, 0.03f, 0.225f);

	int pulse_error;

	float audio_stereo_input[BUFFER_SIZE*2]; // Stereo

	float *rds_in = malloc(sizeof(float) * BUFFER_SIZE * config.rds_streams);
	memset(rds_in, 0, sizeof(float) * BUFFER_SIZE * config.rds_streams);

	float mpx_in[BUFFER_SIZE] = {0};
	float output[BUFFER_SIZE];

	while (to_run) {
		if((pulse_error = read_PulseInputDevice(&runtime->input_device, audio_stereo_input, sizeof(audio_stereo_input)))) { // get output from the function and assign it into pulse_error, this comment to avoid confusion
			fprintf(stderr, "Error reading from input device: %s\n", pa_strerror(pulse_error));
			to_run = 0;
			break;
		}
		if(mpx_on) {
			if((pulse_error = read_PulseInputDevice(&runtime->mpx_device, mpx_in, sizeof(mpx_in)))) {
				fprintf(stderr, "Error reading from MPX device: %s\n", pa_strerror(pulse_error));
				fprintf(stderr, "Disabling MPX.\n");
				mpx_on = 0;
			}
		}
		if(rds_on) {
			if((pulse_error = read_PulseInputDevice(&runtime->rds_device, rds_in, sizeof(float) * BUFFER_SIZE * config.rds_streams))) {
				fprintf(stderr, "Error reading from RDS95 device: %s\n", pa_strerror(pulse_error));
				fprintf(stderr, "Disabling RDS.\n");
				rds_on = 0;
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

			ready_l = hard_clip(ready_l*config.audio_volume, config.clipper_threshold);
			ready_r = hard_clip(ready_r*config.audio_volume, config.clipper_threshold);

			mpx = stereo_encode(&stencode, config.stereo, ready_l, ready_r);

			if(rds_on && !config.polar_stereo) {
				for(uint8_t stream = 0; stream < config.rds_streams; stream++) {
					uint8_t osc_stream = 12+stream; // If the osc is a 4750 sine wave, then doing this would mean that stream 0 is 12, so 57 khz
					if(osc_stream == 13) osc_stream++; // 61.75 KHz is not used, idk why but would be cool if it was
					mpx += (rds_in[config.rds_streams*i+stream]*get_oscillator_cos_multiplier_ni(&osc, osc_stream)) * (RDS_VOLUME * powf(RDS_VOLUME_STEP, stream));
				}
			}

			float mpx_power = measure_mpx(&power, mpx * config.mpx_deviation);
			if (mpx_power > config.mpx_power) {
				float excess_power = mpx_power - config.mpx_power;
				
				if (excess_power > 0.0f && excess_power < 10.0f) {
					float target_gain = dbr_to_deviation(-excess_power) / config.mpx_deviation;
					
					target_gain = fmaxf(target_gain, 0.1f);
					target_gain = fminf(target_gain, 1.0f);
					
					bs412_audio_gain = 0.8f * bs412_audio_gain + 0.2f * target_gain;
				}
			} else bs412_audio_gain = fminf(1.5f, bs412_audio_gain + 0.001f);

			mpx *= bs412_audio_gain;
			
			output[i] = hard_clip(mpx, 1.0f)*config.master_volume+mpx_in[i]; // Ensure peak deviation of 75 khz, assuming we're calibrated correctly
			advance_oscillator(&osc);
		}

		if((pulse_error = write_PulseOutputDevice(&runtime->output_device, output, sizeof(output)))) {
			fprintf(stderr, "Error writing to output device: %s\n", pa_strerror(pulse_error));
			to_run = 0;
			break;
		}
	}
	iirfilt_rrrf_destroy(lpf_l);
	iirfilt_rrrf_destroy(lpf_r);

	free(rds_in);
	return 0;
}

int main(int argc, char **argv) {
	printf("fm95 (an FM Processor by radio95) version 2.0\n");

	FM95_Config config = {
		.stereo = DEFAULT_STEREO,
		.polar_stereo = DEFAULT_STEREO_POLAR,

		.rds_streams = DEFAULT_RDS_STREAMS,

		.clipper_threshold = DEFAULT_CLIPPER_THRESHOLD,
		.preemphasis = DEFAULT_PREEMPHASIS_TAU,
		.calibration = 0,
		.mpx_power = DEFAULT_MPX_POWER,
		.mpx_deviation = DEFAULT_MPX_DEVIATION,
		.master_volume = DEFAULT_MASTER_VOLUME,
		.audio_volume = DEFAULT_AUDIO_VOLUME,

		.sample_rate = DEFAULT_SAMPLE_RATE
	};

	char input_device_name[64] = INPUT_DEVICE;
	char output_device_name[64] = OUTPUT_DEVICE;
	char mpx_device_name[64] = MPX_DEVICE;
	char rds_device_name[64] = RDS_DEVICE;

	int opt;
	const char	*short_opt = "s::i:o:M:r:R:c:O::e:V::p:P:A:v:D:h";
	struct option	long_opt[] =
	{
		{"stereo",      optional_argument, NULL, 's'},
		{"input",       required_argument, NULL, 'i'},
		{"output",      required_argument, NULL, 'o'},
		{"mpx",         required_argument, NULL, 'M'},
		{"rds",         required_argument, NULL, 'r'},
		{"rds_strs",        required_argument, NULL, 'R'},
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
				if(optarg) config.stereo = atoi(optarg);
				else config.stereo = 1;
				break;
			case 'i': // Input Device
				memcpy(input_device_name, optarg, 63);
				break;
			case 'o': // Output Device
				memcpy(output_device_name, optarg, 63);
				break;;
			case 'M': //MPX in
				memcpy(mpx_device_name, optarg, 63);
				break;
			case 'r': // RDS in
				memcpy(rds_device_name, optarg, 63);
				break;
			case 'R': // RDS Streams
				config.rds_streams = atoi(optarg);
				if(config.rds_streams > 4) {
					printf("RDS Streams more than 4? Nuh uh\n");
					return 1;
				}
				break;
			case 'c': //Clipper
				config.clipper_threshold = strtof(optarg, NULL);
				break;
			case 'O': //Polar
				if(optarg) config.polar_stereo = atoi(optarg);
				else config.polar_stereo = 1;
				break;
			case 'e': // Preemp
				config.preemphasis = strtof(optarg, NULL)*1.0e-6f;
				break;
			case 'V': // Calibration
				if(optarg) config.calibration = atoi(optarg);
				else config.calibration = 1;
				break;
			case 'p': // Power
				config.mpx_power = strtof(optarg, NULL);
				break;
			case 'P': // MPX deviation
				config.mpx_deviation = strtof(optarg, NULL);
				break;
			case 'A': // Master vol
				config.master_volume = strtof(optarg, NULL);
				break;
			case 'v': // Audio Volume
				config.audio_volume = strtof(optarg, NULL);
				break;
			case 'D': // Deviation
				config.master_volume *= (strtof(optarg, NULL)/75000.0f);
				break;
			case 'h':
				show_help(argv[0]);
				return 1;
		}
	}

	FM95_Runtime runtime;
	memset(&runtime, 0, sizeof(runtime));

	int mpx_on = (strlen(mpx_device_name) != 0);
	int rds_on = (strlen(rds_device_name) != 0 && config.rds_streams != 0);	

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

	printf("Connecting to input device... (%s)\n", input_device_name);
	opentime_pulse_error = init_PulseInputDevice(&runtime.input_device, config.sample_rate, 2, "fm95", "Main Audio Input", input_device_name, &input_buffer_atr, PA_SAMPLE_FLOAT32NE);
	if (opentime_pulse_error) {
		fprintf(stderr, "Error: cannot open input device: %s\n", pa_strerror(opentime_pulse_error));
		return 1;
	}

	if(mpx_on) {
		printf("Connecting to MPX device... (%s)\n", mpx_device_name);

		opentime_pulse_error = init_PulseInputDevice(&runtime.mpx_device, config.sample_rate, 1, "fm95", "MPX Input", mpx_device_name, &input_buffer_atr, PA_SAMPLE_FLOAT32NE);
		if (opentime_pulse_error) {
			fprintf(stderr, "Error: cannot open MPX device: %s\n", pa_strerror(opentime_pulse_error));
			free_PulseInputDevice(&runtime.input_device);
			return 1;
		}
	}
	if(rds_on) {
		printf("Connecting to RDS95 device... (%s)\n", rds_device_name);

		opentime_pulse_error = init_PulseInputDevice(&runtime.rds_device, config.sample_rate, config.rds_streams, "fm95", "RDS95 Input", rds_device_name, &input_buffer_atr, PA_SAMPLE_FLOAT32NE);
		if (opentime_pulse_error) {
			fprintf(stderr, "Error: cannot open RDS device: %s\n", pa_strerror(opentime_pulse_error));
			free_PulseInputDevice(&runtime.input_device);
			if(mpx_on) free_PulseInputDevice(&runtime.mpx_device);
			return 1;
		}
	}

	printf("Connecting to output device... (%s)\n", output_device_name);

	opentime_pulse_error = init_PulseOutputDevice(&runtime.output_device, config.sample_rate, 1, "fm95", "Main Audio Output", output_device_name, &output_buffer_atr, PA_SAMPLE_FLOAT32NE);
	if (opentime_pulse_error) {
		fprintf(stderr, "Error: cannot open output device: %s\n", pa_strerror(opentime_pulse_error));
		free_PulseInputDevice(&runtime.input_device);
		if(mpx_on) free_PulseInputDevice(&runtime.mpx_device);
		if(rds_on) free_PulseInputDevice(&runtime.rds_device);
		return 1;
	}

	signal(SIGINT, stop);
	signal(SIGTERM, stop);

	int ret = run_fm95(config, &runtime);
	printf("Cleaning up...\n");
	free_PulseInputDevice(&runtime.input_device);
	if(mpx_on) free_PulseInputDevice(&runtime.mpx_device);
	if(rds_on) free_PulseInputDevice(&runtime.rds_device);
	free_PulseOutputDevice(&runtime.output_device);
	return ret;
}