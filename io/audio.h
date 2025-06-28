#pragma once

#include <pulse/simple.h>
#include <pulse/error.h>
#include <string.h>
#include <stdlib.h>

#ifdef DEBUG
#define PULSE_DEBUG
#endif
#ifdef PULSE_DEBUG
#include "../lib/debug.h"
#endif

typedef struct
{
	pa_simple* dev;
	pa_sample_spec sample_spec;
	pa_buffer_attr buffer_attr;
	char* app_name;
	char* stream_name;
	char* device;
	sig_atomic_t initialized;
} PulseDevice;

typedef PulseDevice PulseInputDevice;
int init_PulseInputDevice(PulseInputDevice* dev, const int sample_rate, const int channels, const char* app_name, const char *stream_name, const char* device, pa_buffer_attr* buffer_attr, enum pa_sample_format format);
int read_PulseInputDevice(PulseInputDevice *dev, void *buffer, size_t size);
void free_PulseInputDevice(PulseInputDevice *dev);

typedef PulseDevice PulseOutputDevice;
int init_PulseOutputDevice(PulseOutputDevice* dev, const int sample_rate, const int channels, const char* app_name, const char *stream_name, const char* device, pa_buffer_attr* buffer_attr, enum pa_sample_format format);
int write_PulseOutputDevice(PulseOutputDevice *dev, void *buffer, size_t size);
void free_PulseOutputDevice(PulseOutputDevice *dev);