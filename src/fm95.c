#include <getopt.h>
#include <liquid/liquid.h>
#include "../inih/ini.h"
#include <stdbool.h>

#define DEFAULT_INI_PATH "/etc/fm95.conf"

#define buffer_maxlength 12288
#define buffer_tlength_fragsize 12288
#define buffer_prebuf 8

#define DEFAULT_STEREO 1
#define DEFAULT_RDS_STREAMS 2
#define DEFAULT_CLIPPER_THRESHOLD 1.0f
#define DEFAULT_PREEMPHASIS_TAU 50e-6 // Europe, the freedomers use 75µs (75e-6)
#define DEFAULT_MPX_POWER 3.0f // dbr, this is for BS412, simplest bs412
#define DEFAULT_MPX_DEVIATION 75000.0f // for BS412
#define DEFAULT_DEVIATION 75000.0f // another way to set the volume

#include "../dsp/oscillator.h"
#include "../filter/iir.h"
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

#define DEFAULT_MONO_VOLUME 0.45f // 45%
#define DEFAULT_PILOT_VOLUME 0.09f // 9%
#define DEFAULT_STEREO_VOLUME 0.3f // 30%
#define DEFAULT_RDS_VOLUME 0.0475f // 4.75%
#define DEFAULT_RDS_VOLUME_STEP 0.9f // 90%, so RDS2 stream 4 is 90% of stream 3 which is 90% of stream 2, which again is 90% of stream 1...

static volatile sig_atomic_t to_run = 1;

inline float hard_clip(float sample, float threshold) { return fmaxf(-threshold, fminf(threshold, sample)); }

typedef struct
{
	float mono;
	float pilot;
	float stereo;
	float rds;
	float rds_step;
} FM95_Volumes;
typedef struct
{
	FM95_Volumes volumes;
	uint8_t stereo;

	uint8_t rds_streams;

	float clipper_threshold;
	float preemphasis;
	float tilt;
	uint8_t calibration;
	float mpx_power;
	float mpx_deviation;
	float audio_deviation;
	float master_volume;
	float audio_volume;
	float audio_preamp;

	uint32_t sample_rate;

	// ini dont edit
	char ini_config_path[64];

	uint8_t lpf_order;
	float preemp_unity_freq;
	float agc_target;
	float agc_attack;
	float agc_release;
	float agc_max;
	float agc_min;
	float bs412_attack;
	float bs412_release;
	float bs412_max;
	float lpf_cutoff;
} FM95_Config;

typedef struct
{
	PulseInputDevice input_device, mpx_device, rds_device;
	PulseOutputDevice output_device;
} FM95_Runtime;

typedef struct {
    char input[64];
    char output[64];
    char mpx[64];
    char rds[64];
} FM95_DeviceNames;
typedef struct {
    FM95_Config* config;
    FM95_DeviceNames* devices;
} FM95_SetupContext;

static void stop(int signum) {
	(void)signum;
	printf("\nReceived stop signal.\n");
	to_run = 0;
}

void show_help(char *name) {
	printf(
		"Usage: \t%s\n"
		"\t-c,--config\tOverride the default config path (%s)\n",
		name,
		DEFAULT_INI_PATH
	);
}

void cleanup_runtime(FM95_Runtime *rt, bool mpx_on, bool rds_on) {
    free_PulseInputDevice(&rt->input_device);
    if (mpx_on) free_PulseInputDevice(&rt->mpx_device);
    if (rds_on) free_PulseInputDevice(&rt->rds_device);
    free_PulseOutputDevice(&rt->output_device);
}

int run_fm95(const FM95_Config config, FM95_Runtime* runtime) {
	bool mpx_on = (runtime->mpx_device.initialized == 1);
	bool rds_on = (runtime->rds_device.initialized == 1);

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
	init_oscillator(&osc, (config.stereo == 2) ? 7812.5 : 4750, config.sample_rate);

	iirfilt_rrrf lpf_l, lpf_r;
	if(config.lpf_cutoff != 0) {
		lpf_l = iirfilt_rrrf_create_prototype(LIQUID_IIRDES_CHEBY2, LIQUID_IIRDES_LOWPASS, LIQUID_IIRDES_SOS, config.lpf_order, (config.lpf_cutoff/config.sample_rate), 0.0f, 1.0f, 60.0f);
		lpf_r = iirfilt_rrrf_create_prototype(LIQUID_IIRDES_CHEBY2, LIQUID_IIRDES_LOWPASS, LIQUID_IIRDES_SOS, config.lpf_order, (config.lpf_cutoff/config.sample_rate), 0.0f, 1.0f, 60.0f);
	}

	ResistorCapacitor preemp_l, preemp_r;
	if(config.preemphasis != 0) {
		init_preemphasis(&preemp_l, config.preemphasis, config.sample_rate, config.preemp_unity_freq);
		init_preemphasis(&preemp_r, config.preemphasis, config.sample_rate, config.preemp_unity_freq);
	}

	BS412Compressor bs412;
	init_bs412(&bs412, config.mpx_deviation, config.mpx_power, config.bs412_attack, config.bs412_release, config.bs412_max, config.sample_rate);

	TiltCorrectionFilter tilter;
	if(config.tilt != 0) tilt_init(&tilter, config.tilt);

	StereoEncoder stencode;
	init_stereo_encoder(&stencode, 4.0f, &osc, (config.stereo == 2), config.volumes.mono, config.volumes.pilot, config.volumes.stereo);

	AGC agc;
	if(config.agc_max != 0.0) initAGC(&agc, config.sample_rate, config.agc_target, config.agc_min, config.agc_max, config.agc_attack, config.agc_release);

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
				fprintf(stderr, "Error reading from MPX device: %s\nDisabling MPX.\n", pa_strerror(pulse_error));
				mpx_on = 0;
			}
		}
		if(rds_on) {
			if((pulse_error = read_PulseInputDevice(&runtime->rds_device, rds_in, sizeof(float) * BUFFER_SIZE * config.rds_streams))) {
				fprintf(stderr, "Error reading from RDS95 device: %s\nDisabling RDS.\n", pa_strerror(pulse_error));
				rds_on = 0;
			}
		}

		for (uint16_t i = 0; i < BUFFER_SIZE; i++) {
			float mpx = 0.0f;

			float l = audio_stereo_input[2*i+0]*config.audio_preamp;
			float r = audio_stereo_input[2*i+1]*config.audio_preamp;
			
			if(config.agc_max != 0.0) {
				float agc_gain = process_agc(&agc, 0.5f * (fabsf(l) + fabsf(r)));
				l *= agc_gain;
				r *= agc_gain;
			}

			if(config.lpf_cutoff != 0) {
				iirfilt_rrrf_execute(lpf_l, l, &l);
				iirfilt_rrrf_execute(lpf_r, r, &r);
			}
			
			if(config.preemphasis != 0) {
				l = apply_preemphasis(&preemp_l, l);
				r = apply_preemphasis(&preemp_r, r);
			}

			l = hard_clip(l*config.audio_volume, config.clipper_threshold);
			r = hard_clip(r*config.audio_volume, config.clipper_threshold);

			mpx = stereo_encode(&stencode, config.stereo, l, r);

			if(rds_on && !(config.stereo == 2)) { // disable rds on polar stereo
				float rds_level = config.volumes.rds;
				for(uint8_t stream = 0; stream < config.rds_streams; stream++) {
					uint8_t osc_stream = 12 + stream;
					if(osc_stream == 13) osc_stream++;

					mpx += (rds_in[config.rds_streams * i + stream] * get_oscillator_cos_multiplier_ni(&osc, osc_stream)) * rds_level;

					rds_level *= config.volumes.rds_step; // Prepare level for the next stream
				}
			}

			mpx = bs412_compress(&bs412, mpx+mpx_in[i]);
			if(config.tilt != 0) mpx = tilt(&tilter, mpx);

			output[i] = hard_clip(mpx*config.master_volume, 1.0); // Ensure peak deviation of 75 khz (or the set deviation), assuming we're calibrated correctly
			advance_oscillator(&osc);
		}

		if((pulse_error = write_PulseOutputDevice(&runtime->output_device, output, sizeof(output)))) {
			fprintf(stderr, "Error writing to output device: %s\n", pa_strerror(pulse_error));
			to_run = 0;
			break;
		}
	}
	if(config.lpf_cutoff != 0) {
		iirfilt_rrrf_destroy(lpf_l);
		iirfilt_rrrf_destroy(lpf_r);
	}

	free(rds_in);
	return 0;
}


int parse_arguments(int argc, char **argv, FM95_Config* config) {
	int opt;
	const char	*short_opt = "c:h";
	struct option	long_opt[] =
	{
		{"config",		required_argument,	NULL,	'c'},
		{"help",        no_argument,       NULL, 'h'},
		{0,             0,                 0,    0}
	};

	while((opt = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1) {
		switch(opt) {
			case 'c':
				memcpy(config->ini_config_path, optarg, 63);
				break;
			case 'h':
				show_help(argv[0]);
				return 1;
		}
	}

	return 0;
}

static int config_handler(void* user, const char* section, const char* name, const char* value) {
    FM95_SetupContext* ctx = (FM95_SetupContext*)user;
    FM95_Config* pconfig = ctx->config;
    FM95_DeviceNames* dv = ctx->devices;

    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

    if (MATCH("fm95", "stereo")) {
        pconfig->stereo = atoi(value);
	} else if (MATCH("devices", "input")) {
        strncpy(dv->input, value, 63);
        dv->input[63] = '\0';
    } else if (MATCH("devices", "output")) {
        strncpy(dv->output, value, 63);
        dv->output[63] = '\0';
    } else if (MATCH("devices", "mpx")) {
        strncpy(dv->mpx, value, 63);
        dv->mpx[63] = '\0';
    } else if (MATCH("devices", "rds")) {
        strncpy(dv->rds, value, 63);
        dv->rds[63] = '\0';
    } else if (MATCH("fm95", "rds_streams")) {
        pconfig->rds_streams = atoi(value);
        if(pconfig->rds_streams > 4) {
            printf("RDS Streams more than 4? Nuh uh\n");
            return 0;
        }
    } else if (MATCH("fm95", "clipper_threshold")) {
        pconfig->clipper_threshold = strtof(value, NULL);
    } else if (MATCH("fm95", "preemphasis")) {
        pconfig->preemphasis = strtof(value, NULL) * 1.0e-6f;
    } else if (MATCH("fm95", "calibration")) {
        pconfig->calibration = atoi(value);
    } else if (MATCH("fm95", "mpx_power")) {
        pconfig->mpx_power = strtof(value, NULL);
    } else if (MATCH("fm95", "mpx_deviation")) {
        pconfig->mpx_deviation = strtof(value, NULL);
    } else if (MATCH("fm95", "master_volume")) {
        pconfig->master_volume = strtof(value, NULL);
    } else if (MATCH("fm95", "audio_volume")) {
        pconfig->audio_volume = strtof(value, NULL);
    } else if (MATCH("fm95", "audio_preamp")) {
        pconfig->audio_preamp = strtof(value, NULL);
    } else if (MATCH("fm95", "deviation")) {
        pconfig->audio_deviation = strtof(value, NULL);
	} else if(MATCH("advanced", "lpf_order")) {
		pconfig->lpf_order = atoi(value);
	} else if(MATCH("advanced", "preemp_unity")) {
		pconfig->preemp_unity_freq = strtof(value, NULL);
	} else if(MATCH("advanced", "sample_rate")) {
		pconfig->sample_rate = atoi(value);
	} else if(MATCH("advanced", "agc_target")) {
		pconfig->agc_target = strtof(value, NULL);
	} else if(MATCH("advanced", "agc_attack")) {
		pconfig->agc_attack = strtof(value, NULL);
	} else if(MATCH("advanced", "agc_release")) {
		pconfig->agc_release = strtof(value, NULL);
	} else if(MATCH("advanced", "agc_min")) {
		pconfig->agc_min = strtof(value, NULL);
	} else if(MATCH("advanced", "agc_max")) {
		pconfig->agc_max = strtof(value, NULL);
	} else if(MATCH("advanced", "bs412_attack")) {
		pconfig->bs412_attack = strtof(value, NULL);
	} else if(MATCH("advanced", "bs412_release")) {
		pconfig->bs412_release = strtof(value, NULL);
	} else if(MATCH("volumes", "mono")) {
		pconfig->volumes.mono = strtof(value, NULL);
	} else if(MATCH("volumes", "pilot")) {
		pconfig->volumes.pilot = strtof(value, NULL);
	} else if(MATCH("volumes", "stereo")) {
		pconfig->volumes.stereo = strtof(value, NULL);
	} else if(MATCH("volumes", "rds")) {
		pconfig->volumes.rds = strtof(value, NULL);
	} else if(MATCH("volumes", "rds_step")) {
		pconfig->volumes.rds_step = strtof(value, NULL);
	} else if(MATCH("fm95", "tilt")) {
		pconfig->tilt = strtof(value, NULL);
	} else if(MATCH("advanced", "bs412_max")) {
		pconfig->bs412_max = strtof(value, NULL);
	} else if(MATCH("advanced", "lpf_cutoff")) {
		pconfig->lpf_cutoff = strtof(value, NULL);
		if(pconfig->lpf_cutoff > (pconfig->sample_rate * 0.5)) {
			pconfig->lpf_cutoff = (pconfig->sample_rate * 0.5);
			fprintf(stderr, "LPF cutoff over niquist, limiting.\n");
		}
	} else {
        return 0; // Unknown section/name
    }

    return 1;
}

int parse_config(FM95_Config* config, FM95_DeviceNames* dv) {
	FM95_SetupContext ctx = {
		.config = config,
		.devices = dv
	};
	return ini_parse(config->ini_config_path, &config_handler, &ctx);
}

int setup_audio(FM95_Runtime* runtime, const FM95_DeviceNames dv_names, const FM95_Config config, bool mpx_on, bool rds_on) {
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

	printf("Connecting to input device... (%s)\n", dv_names.input);
	opentime_pulse_error = init_PulseInputDevice(&runtime->input_device, config.sample_rate, 2, "fm95", "Main Audio Input", dv_names.input, &input_buffer_atr, PA_SAMPLE_FLOAT32NE);
	if (opentime_pulse_error) {
		fprintf(stderr, "Error: cannot open input device: %s\n", pa_strerror(opentime_pulse_error));
		return 1;
	}

	if(mpx_on) {
		printf("Connecting to MPX device... (%s)\n", dv_names.mpx);

		opentime_pulse_error = init_PulseInputDevice(&runtime->mpx_device, config.sample_rate, 1, "fm95", "MPX Input", dv_names.mpx, &input_buffer_atr, PA_SAMPLE_FLOAT32NE);
		if (opentime_pulse_error) {
			fprintf(stderr, "Error: cannot open MPX device: %s\n", pa_strerror(opentime_pulse_error));
			free_PulseInputDevice(&runtime->input_device);
			return 1;
		}
	}
	if(rds_on) {
		printf("Connecting to RDS95 device... (%s)\n", dv_names.rds);

		opentime_pulse_error = init_PulseInputDevice(&runtime->rds_device, config.sample_rate, config.rds_streams, "fm95", "RDS95 Input", dv_names.rds, &input_buffer_atr, PA_SAMPLE_FLOAT32NE);
		if (opentime_pulse_error) {
			fprintf(stderr, "Error: cannot open RDS device: %s\n", pa_strerror(opentime_pulse_error));
			free_PulseInputDevice(&runtime->input_device);
			if(mpx_on) free_PulseInputDevice(&runtime->mpx_device);
			return 1;
		}
	}

	printf("Connecting to output device... (%s)\n", dv_names.output);

	opentime_pulse_error = init_PulseOutputDevice(&runtime->output_device, config.sample_rate, 1, "fm95", "Main Audio Output", dv_names.output, &output_buffer_atr, PA_SAMPLE_FLOAT32NE);
	if (opentime_pulse_error) {
		fprintf(stderr, "Error: cannot open output device: %s\n", pa_strerror(opentime_pulse_error));
		free_PulseInputDevice(&runtime->input_device);
		if(mpx_on) free_PulseInputDevice(&runtime->mpx_device);
		if(rds_on) free_PulseInputDevice(&runtime->rds_device);
		return 1;
	}
	return 0;
}

int main(int argc, char **argv) {
	printf("fm95 (an FM Processor by radio95) version 2.2\n");

	FM95_Config config = {
		.volumes = {
			.mono = DEFAULT_MONO_VOLUME,
			.pilot = DEFAULT_PILOT_VOLUME,
			.stereo = DEFAULT_STEREO_VOLUME,
			.rds = DEFAULT_RDS_VOLUME,
			.rds_step = DEFAULT_RDS_VOLUME_STEP
		},
		.stereo = DEFAULT_STEREO,

		.rds_streams = DEFAULT_RDS_STREAMS,

		.clipper_threshold = DEFAULT_CLIPPER_THRESHOLD,
		.preemphasis = DEFAULT_PREEMPHASIS_TAU,
		.tilt = 0,
		.calibration = 0,
		.mpx_power = DEFAULT_MPX_POWER,
		.mpx_deviation = DEFAULT_MPX_DEVIATION,
		.audio_deviation = DEFAULT_DEVIATION,
		.master_volume = DEFAULT_MASTER_VOLUME,
		.audio_volume = 1.0f,
		.audio_preamp = 1.0f,

		.sample_rate = DEFAULT_SAMPLE_RATE,

		.ini_config_path = DEFAULT_INI_PATH,

		.lpf_order = 17,
		.preemp_unity_freq = 15250.0f,
		.agc_target = 0.625f,
		.agc_attack = 0.03f,
		.agc_release = 0.225f,
		.agc_min = 0.1f,
		.agc_max = 1.75f,
		.bs412_attack = 0.03f,
		.bs412_release = 0.02,
		.bs412_max = 1.0f,
		.lpf_cutoff = 15000,
	};

	FM95_DeviceNames dv_names = {
		.input = INPUT_DEVICE,
		.output = OUTPUT_DEVICE,
		.mpx = MPX_DEVICE,
		.rds = RDS_DEVICE
	};

	int err;
	err = parse_arguments(argc, argv, &config);
	if(err != 0) return err;

	err = parse_config(&config, &dv_names);
	if(err != 0) {
		printf("Could not parse the config file. (error code as return code)\n");
		return err;
	}

	config.master_volume *= config.audio_deviation/75000.0f;

	FM95_Runtime runtime;
	memset(&runtime, 0, sizeof(runtime));

	bool mpx_on = (strlen(dv_names.mpx) != 0);
	bool rds_on = (strlen(dv_names.rds) != 0 && config.rds_streams != 0);

	err = setup_audio(&runtime, dv_names, config, mpx_on, rds_on);
	if(err != 0) return err;

	signal(SIGINT, stop);
	signal(SIGTERM, stop);

	int ret = run_fm95(config, &runtime);
	printf("Cleaning up...\n");
	cleanup_runtime(&runtime, mpx_on, rds_on);
	return ret;
}