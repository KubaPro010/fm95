#pragma once

#include <pulse/simple.h>
#include <pulse/error.h>
#include <string.h>
#include <stdlib.h>

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
int read_PulseInputDevice(PulseInputDevice *dev, float *buffer, size_t size);
void free_PulseInputDevice(PulseInputDevice *dev);

typedef PulseInputDevice PulseOutputDevice;
int init_PulseOutputDevice(PulseOutputDevice *dev, int sample_rate, int channels, char *app_name, char *stream_name, char *device, pa_buffer_attr *buffer_attr);
int init_PulseOutputDevicef(PulseOutputDevice *dev, int sample_rate, int channels, char *app_name, char *stream_name, char *device, pa_buffer_attr *buffer_attr, pa_sample_format_t format);
int write_PulseOutputDevice(PulseOutputDevice *dev, float *buffer, size_t size);
int write_PulseOutputDevicef(PulseOutputDevice *dev, void *buffer, size_t size);
void free_PulseOutputDevice(PulseOutputDevice *dev);