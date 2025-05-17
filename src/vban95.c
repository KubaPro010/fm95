#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>

#define buffer_maxlength 12288
#define buffer_tlength_fragsize 12288
#define buffer_prebuf 8

#define VBAN_SR_MAXNUMBER 21
static long VBAN_SRList[VBAN_SR_MAXNUMBER] = {
    6000, 12000, 24000, 48000, 96000, 192000, 384000,
    8000, 16000, 32000, 64000, 128000, 256000, 512000,
    11025, 22050, 44100, 88200, 176400, 352800, 705600
};

#include "../io/audio.h"
#include <pulse/simple.h>

#define VBAN_BIT_MAXNUMBER 5 // 7 in the standard but pa does these 5
static enum pa_sample_format VBAN_BITList[VBAN_BIT_MAXNUMBER] = {
    PA_SAMPLE_U8,
    PA_SAMPLE_S16NE,
    PA_SAMPLE_S24NE,
    PA_SAMPLE_S32NE,
    PA_SAMPLE_FLOAT32NE,
};
static char VBAN_TextBITList[VBAN_BIT_MAXNUMBER][4] = {
    "U08",
    "S16",
    "S24",
    "S32",
    "F32",
};

#define VBAN_PROTOCOL_AUDIO 0x00
#define VBAN_PROTOCOL_SERIAL 0x20
#define VBAN_PROTOCOL_TXT 0x40
#define VBAN_PROTOCOL_SERVICE 0x60

#define BUF_SIZE 2048
#define MAX_AUDIO_DATA_SIZE (BUF_SIZE - sizeof(VBANHeader))
#define MAX_BUFFER_PACKETS 128

#pragma pack(1)
typedef struct {
    char vban[4];
    uint8_t sample_rate_idx;
    uint8_t samples_per_frame;
    uint8_t sample_channels;
    uint8_t format_type;
    char streamname[16];
    uint32_t frame_num;
} VBANHeader;

typedef union {
    VBANHeader packet_data;
    char raw_data[sizeof(VBANHeader)];
} VBANHeaderUnion;
#pragma pack()

typedef struct {
    char data[MAX_AUDIO_DATA_SIZE];
    size_t size;
} AudioPacket;

typedef struct {
    AudioPacket* packets;
    int capacity;
    int head;
    int tail;
    int count;
    VBANHeader header;
} AudioBuffer;

AudioBuffer* create_audio_buffer(int capacity) {
    AudioBuffer* buffer = (AudioBuffer*)malloc(sizeof(AudioBuffer));
    if (!buffer) {
        perror("Failed to allocate audio buffer");
        return NULL;
    }

    buffer->packets = (AudioPacket*)malloc(capacity * sizeof(AudioPacket));
    if (!buffer->packets) {
        perror("Failed to allocate packet buffer");
        free(buffer);
        return NULL;
    }

    buffer->capacity = capacity;
    buffer->head = 0;
    buffer->tail = 0;
    buffer->count = 0;

    return buffer;
}

void destroy_audio_buffer(AudioBuffer* buffer) {
    if (buffer) {
        free(buffer->packets);
        free(buffer);
    }
}

int add_to_buffer(AudioBuffer* buffer, const char* data, size_t size, const VBANHeader* header) {
    if (size > MAX_AUDIO_DATA_SIZE) {
        fprintf(stderr, "Audio data too large for buffer\n");
        return -1;
    }

    if (buffer->count == buffer->capacity) {
        buffer->tail = (buffer->tail + 1) % buffer->capacity;
        buffer->count--;
    }

    AudioPacket* pkt = &buffer->packets[buffer->head];
    memcpy(pkt->data, data, size);
    pkt->size = size;
    memcpy(&buffer->header, header, sizeof(VBANHeader));

    buffer->head = (buffer->head + 1) % buffer->capacity;
    buffer->count++;

    return 1;
}


volatile uint8_t to_run = 1;

static void stop(int signum) {
    (void)signum;
    printf("\nReceived stop signal.\n");
    to_run = 0;
}

static PulseOutputDevice output = {0};

void process_audio_buffer(AudioBuffer* buffer, PulseOutputDevice* output_device) {
    while (buffer->count > 0) {
        AudioPacket* pkt = &buffer->packets[buffer->tail];
        write_PulseOutputDevicef(output_device, pkt->data, pkt->size);

        buffer->tail = (buffer->tail + 1) % buffer->capacity;
        buffer->count--;
    }
}

void reset_audio_buffer(AudioBuffer* buffer) {
    buffer->head = 0;
    buffer->tail = 0;
    buffer->count = 0;
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        fprintf(stderr, "Usage: %s <remote_ip> <port> <streamname> <buffer_size> <pulse_device> <optional: quiet>\n", argv[0]);
        return 1;
    }

    char *remote_ip = argv[1];
    int listen_port = atoi(argv[2]);
    char *stream_name = argv[3];
    int buffer_size = atoi(argv[4]);
    char *pulse_device = argv[5];
    int quiet = (argc == 7);

    if (buffer_size <= 0 || buffer_size > MAX_BUFFER_PACKETS) {
        fprintf(stderr, "Buffer size must be between 1 and %d\n", MAX_BUFFER_PACKETS);
        return 1;
    }

    printf("Starting VBAN receiver with buffer size: %d packets\n", buffer_size);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(listen_port);

    if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("bind");
        close(sockfd);
        return 1;
    }

    struct in_addr remote_addr_bin;
    if (inet_pton(AF_INET, remote_ip, &remote_addr_bin) != 1) {
        fprintf(stderr, "Invalid remote IP address: %s\n", remote_ip);
        close(sockfd);
        return 1;
    }

    char buffer[BUF_SIZE];
    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);

    uint32_t vban_frame = 0;
    uint8_t vban_last_sr = 0;
    uint8_t vban_last_format = 0;
    uint8_t vban_last_channels = 0;
    uint8_t vban_audio_reset = 0;

    AudioBuffer* audio_buffer = create_audio_buffer(buffer_size);
    if (!audio_buffer) {
        close(sockfd);
        return 1;
    }

    signal(SIGINT, stop);
    signal(SIGTERM, stop);

    while (to_run) {
        ssize_t recv_len = recvfrom(sockfd, buffer, BUF_SIZE, 0,
                                    (struct sockaddr *)&sender_addr, &sender_len);
        if (recv_len < 0) {
            perror("recvfrom");
            break;
        }

        if (sender_addr.sin_addr.s_addr == remote_addr_bin.s_addr || remote_addr_bin.s_addr == 0) {
            VBANHeaderUnion data;
            memcpy(&data.raw_data, buffer, sizeof(VBANHeader));

            if (memcmp(data.packet_data.vban, "VBAN", 4) != 0) continue; // Not VBAN
            if (memcmp(data.packet_data.streamname, stream_name, strlen(stream_name)) != 0) continue; // Not this

            if (vban_frame == 0 && data.packet_data.frame_num != 0) {
                // This means either this is our first packet, sync to the sender then
                vban_frame = data.packet_data.frame_num;
            }

            if(data.packet_data.frame_num != vban_frame) {
                if (data.packet_data.frame_num > vban_frame) {
                    uint32_t dropped_packets = data.packet_data.frame_num - vban_frame;
                    if (quiet == 0) printf("Dropped %u packets\n", dropped_packets);
                    vban_frame -= dropped_packets;
                } else {
                    if (quiet == 0) printf("Packets received out of order (got:%u, expected:%u)\n", data.packet_data.frame_num, vban_frame);
                }
                vban_frame = data.packet_data.frame_num;
            }
            vban_frame++;

            if(vban_last_sr != data.packet_data.sample_rate_idx) {
                vban_last_sr = data.packet_data.sample_rate_idx;
                if(quiet == 0) printf("New sample rate of %ld\n", VBAN_SRList[vban_last_sr % VBAN_SR_MAXNUMBER]);
                vban_audio_reset = 1;
                reset_audio_buffer(audio_buffer);
            }
            
            if(vban_last_format != data.packet_data.format_type) {
                vban_last_format = data.packet_data.format_type;
                if(quiet == 0) printf("New data format of %s\n", VBAN_TextBITList[vban_last_format % VBAN_BIT_MAXNUMBER]); // Here it should be fine to use the modulo, as during the reset we point out the idx may be shit
                vban_audio_reset = 1;
                reset_audio_buffer(audio_buffer);
            }
            
            if(vban_last_channels != data.packet_data.sample_channels) {
                vban_last_channels = data.packet_data.sample_channels;
                if(quiet == 0) printf("New channel count of %d\n", vban_last_channels + 1); // Add 1 because VBAN channels are 0-based
                vban_audio_reset = 1;
                reset_audio_buffer(audio_buffer);
            }

            // Handle audio reset if needed
            if(vban_audio_reset) {
                if (vban_last_sr >= VBAN_SR_MAXNUMBER || vban_last_format >= VBAN_BIT_MAXNUMBER) {
                    fprintf(stderr, "Unsupported sample rate or format\n");
                    continue;
                }

                if (output.initialized) {
                    free_PulseOutputDevice(&output);
                }
                
                pa_buffer_attr buffer_attr = {
                    .maxlength = buffer_maxlength,
                    .tlength = buffer_tlength_fragsize,
                    .prebuf = buffer_prebuf
                };
                
                int result = init_PulseOutputDevicef(
                    &output, 
                    VBAN_SRList[vban_last_sr], 
                    vban_last_channels + 1, // Add 1 because VBAN channels are 0-based
                    "vban95", 
                    stream_name, 
                    pulse_device, 
                    &buffer_attr,
                    VBAN_BITList[vban_last_format]
                );
                
                if (result != 0) {
                    fprintf(stderr, "Failed to initialize PulseAudio output device: %s\n", pa_strerror(result));
                }
                
                vban_audio_reset = 0;
                continue;
            }

            char* audio_data = buffer + sizeof(VBANHeader);
            size_t audio_data_size = recv_len - sizeof(VBANHeader);

            if (add_to_buffer(audio_buffer, audio_data, audio_data_size, &data.packet_data) > 0) {
                if (audio_buffer->count >= audio_buffer->capacity) {
                    process_audio_buffer(audio_buffer, &output);
                }
            }
        }
    }

    // Clean up
    printf("Cleaning up...\n");
    if (output.initialized) {
        free_PulseOutputDevice(&output);
    }
    destroy_audio_buffer(audio_buffer);
    close(sockfd);
    
    return 0;
}