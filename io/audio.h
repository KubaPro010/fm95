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
	int initialized;
} PulseInputDevice;

int init_PulseInputDevice(PulseInputDevice *dev, int sample_rate, int channels, char *app_name, char *stream_name, char *device, pa_buffer_attr *buffer_attr);
int init_PulseInputDevicef(PulseInputDevice *dev, int sample_rate, int channels, char *app_name, char *stream_name, char *device, pa_buffer_attr *buffer_attr, pa_sample_format_t format);
int read_PulseInputDevice(PulseInputDevice *dev, void *buffer, size_t size);
void free_PulseInputDevice(PulseInputDevice *dev);

typedef PulseInputDevice PulseOutputDevice;
int init_PulseOutputDevice(PulseOutputDevice *dev, int sample_rate, int channels, char *app_name, char *stream_name, char *device, pa_buffer_attr *buffer_attr);
int init_PulseOutputDevicef(PulseOutputDevice *dev, int sample_rate, int channels, char *app_name, char *stream_name, char *device, pa_buffer_attr *buffer_attr, pa_sample_format_t format);
int write_PulseOutputDevice(PulseOutputDevice *dev, void *buffer, size_t size);
void free_PulseOutputDevice(PulseOutputDevice *dev);