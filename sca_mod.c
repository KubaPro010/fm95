#include <stdio.h>
#include <pulse/simple.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>

// Features
// #define PREEMPHASIS
#define LPF

#define SAMPLE_RATE 192000

#define INPUT_DEVICE "real_real_tx_audio_input.monitor"
#define OUTPUT_DEVICE "alsa_output.platform-soc_sound.stereo-fallback"
#define BUFFER_SIZE 512
#define CLIPPER_THRESHOLD 0.425 // Adjust this as needed, this also limits deviation, so if you set this to 0.5 then the deviation will be limited to half

#define VOLUME 0.03f // SCA Volume
#define FREQUENCY 67000 // SCA Frequency
#define DEVIATION 6000 // SCA Deviation

#ifdef PREEMPHASIS
#define PREEMPHASIS_TAU 0.00005  // 50 microseconds, use 0.000075 if in america
#endif

#ifdef LPF
#define LPF_CUTOFF 8000
#endif

volatile sig_atomic_t to_run = 1;

float clip(float sample) {
    if (sample > CLIPPER_THRESHOLD) {
        return CLIPPER_THRESHOLD;  // Clip to the upper threshold
    } else if (sample < -CLIPPER_THRESHOLD) {
        return -CLIPPER_THRESHOLD;  // Clip to the lower threshold
    } else {
        return sample;  // No clipping
    }
}

#define FIR_PHASES 32
#define FIR_TAPS 32

#define PI 3.14159265358979323846
#define M_2PI (3.14159265358979323846 * 2.0)

// Track phase continuously to maintain frequency accuracy
typedef struct {
    float phase;
    float frequency;
    float sample_rate;
} Oscillator;

void init_oscillator(Oscillator *osc, float frequency, float sample_rate) {
    osc->phase = 0.0f;
    osc->frequency = frequency;
    osc->sample_rate = sample_rate;
}

float get_next_sample(Oscillator *osc) {
    float phase_increment = (M_2PI * osc->frequency) / osc->sample_rate; // If you want to have dynamic frequency changing you have to compute this every sample
    float sample = sinf(osc->phase);
    osc->phase += phase_increment;
    if (osc->phase >= M_2PI) {
        osc->phase -= M_2PI;
    }
    return sample;
}

#ifdef PREEMPHASIS
typedef struct {
    float alpha;
    float prev_sample;
} PreEmphasis;

void init_pre_emphasis(PreEmphasis *pe, float sample_rate) {
    pe->prev_sample = 0.0f;
    pe->alpha = exp(-1 / (PREEMPHASIS_TAU * sample_rate));
}

float apply_pre_emphasis(PreEmphasis *pe, float sample) {
    float audio = sample-pe->alpha*pe->prev_sample;
    pe->prev_sample = audio;
    return audio*2;
}
#endif

#ifdef LPF
typedef struct {
    float low_pass_fir[FIR_PHASES][FIR_TAPS];
    float sample_buffer[FIR_TAPS];
    int buffer_index;
} LowPassFilter;

void init_low_pass_filter(LowPassFilter *lp, float sample_rate) {
    for (int i = 0; i < FIR_TAPS; i++) {
        for (int j = 0; j < FIR_PHASES; j++) {
            int mi = i * FIR_PHASES + j + 1;
            float sincpos = mi - (((FIR_TAPS * FIR_PHASES) + 1.0f) / 2.0f);
            float firlowpass = (sincpos == 0.0f) ? 1.0f : sinf(M_2PI * LPF_CUTOFF * sincpos / sample_rate) / (PI * sincpos);
            float window = 0.54f - 0.46f * cosf(M_2PI * mi / (FIR_TAPS * FIR_PHASES)); // Hamming window
            lp->low_pass_fir[j][i] = firlowpass * window;
        }
    }
    memset(lp->sample_buffer, 0, sizeof(lp->sample_buffer));
    lp->buffer_index = 0;
}

float apply_low_pass_filter(LowPassFilter *lp, float sample) {
    // Update the sample buffer
    lp->sample_buffer[lp->buffer_index] = sample;
    lp->buffer_index = (lp->buffer_index + 1) % FIR_TAPS;

    // Apply the filter
    float result = 0.0f;
    int index = lp->buffer_index;
    for (int i = 0; i < FIR_TAPS; i++) {
        result += lp->low_pass_fir[0][i] * lp->sample_buffer[index];
        index = (index + 1) % FIR_TAPS;
    }
    return result*6;
}
#endif

static void stop(int signum) {
    (void)signum;
    printf("\nReceived stop signal. Cleaning up...\n");
    to_run = 0;
}

int main() {
    printf("SCAMod : SCA Modulator (based on the Stereo encoder STCode) made by radio95 (with help of ChatGPT and Claude, thanks!)\n");

    // Define formats and buffer atributes
    pa_sample_spec audio_format = {
        .format = PA_SAMPLE_FLOAT32NE, //Float32 NE, or Float32 Native Endian, the float in c uses the endianess of your pc, or native endian, and float is float32, and double is float64
        .channels = 1,
        .rate = SAMPLE_RATE // Same sample rate makes it easy, leave the resampling to pipewire, it should know better
    };

    pa_buffer_attr input_buffer_atr = {
        .maxlength = 4096, // You can lower this to 512, but this is fine, it's sub-second delay, you're probably not gonna notice unless you're looking for it
	    .fragsize = 2048
    };
    pa_buffer_attr output_buffer_atr = {
        .maxlength = 4096,
        .tlength = 2048,
	    .prebuf = 0
    };

    printf("Connecting to input device... (%s)\n", INPUT_DEVICE);

    pa_simple *input_device = pa_simple_new(
        NULL,
        "SCAMod",
        PA_STREAM_RECORD,
        INPUT_DEVICE,
        "Audio Input",
        &audio_format,
        NULL,
        &input_buffer_atr,
        NULL
    );
    if (!input_device) {
        fprintf(stderr, "Error: cannot open input device.\n");
        return 1;
    }

    printf("Connecting to output device... (%s)\n", OUTPUT_DEVICE);

    pa_simple *output_device = pa_simple_new(
        NULL,
        "SCAMod",
        PA_STREAM_PLAYBACK,
        OUTPUT_DEVICE,
        "Signal",
        &audio_format,
        NULL,
        &output_buffer_atr,
        NULL
    );
    if (!output_device) {
        fprintf(stderr, "Error: cannot open output device.\n");
        pa_simple_free(input_device);
        return 1;
    }

    Oscillator osc;
    init_oscillator(&osc, FREQUENCY, SAMPLE_RATE);
#ifdef PREEMPHASIS
    PreEmphasis preemp;
    init_pre_emphasis(&preemp, SAMPLE_RATE);
#endif
#ifdef LPF
    LowPassFilter lpf;
    init_low_pass_filter(&lpf, SAMPLE_RATE);
#endif

    signal(SIGINT, stop);
    signal(SIGTERM, stop);
    
    float input[BUFFER_SIZE]; // Input from device
    float signal[BUFFER_SIZE]; // this goes to the output
    while (to_run) {
        if (pa_simple_read(input_device, input, sizeof(input), NULL) < 0) {
            fprintf(stderr, "Error reading from input device.\n");
            break;
        }

        for (int i = 0; i < BUFFER_SIZE; i++) {
            float in = input[i];

#ifdef PREEMPHASIS
#ifdef LPF
            float lowpassed = apply_low_pass_filter(&lpf, in);
            float preemphasized = apply_pre_emphasis(&preemp, lowpassed);
            float current_input = clip(preemphasized);
#else
            float preemphasized = apply_pre_emphasis(&preemp, in);
            float current_input = clip(preemphasized);
#endif
#else
#ifdef LPF
            float lowpassed = apply_low_pass_filter(&lpf, in);
            float current_input = clip(lowpassed);
#else
            float current_input = clip(in);
#endif
#endif

            osc.frequency = (FREQUENCY+(current_input*DEVIATION));
            signal[i] = get_next_sample(&osc)*VOLUME;
        }

        if (pa_simple_write(output_device, signal, sizeof(signal), NULL) < 0) {
            fprintf(stderr, "Error writing to output device.\n");
            break;
        }
    }
    printf("Cleaning up...\n");
    pa_simple_free(input_device);
    pa_simple_free(output_device);
    return 0;
}
