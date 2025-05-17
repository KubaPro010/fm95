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
    if (buffer->count >= buffer->capacity) {
        return 0;
    }

    if (size > MAX_AUDIO_DATA_SIZE) {
        fprintf(stderr, "Audio data too large for buffer\n");
        return -1;
    }

    memcpy(buffer->packets[buffer->count].data, data, size);
    buffer->packets[buffer->count].size = size;
    memcpy(&buffer->header, header, sizeof(VBANHeader));
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
    if (buffer->count == 0) {
        return;
    }
    
    for(int i = 0; i < buffer->count; i++) {
        // this function internally checks for initialization so no issues here, if not initialized then the data is discarded
        write_PulseOutputDevicef(&output, buffer->packets[i].data, buffer->packets[i].size);
    }
    
    buffer->count = 0;
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <remote_ip> <port> <streamname> <buffer_size> <pulse_device>\n", argv[0]);
        return 1;
    }

    char *remote_ip = argv[1];
    int listen_port = atoi(argv[2]);
    char *stream_name = argv[3];
    int buffer_size = atoi(argv[4]);
    char *pulse_device = argv[5];

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

        if (sender_addr.sin_addr.s_addr == remote_addr_bin.s_addr || strcmp(remote_ip, "0.0.0.0") == 0) {
            VBANHeaderUnion data;
            memcpy(&data.raw_data, buffer, sizeof(VBANHeader));

            if (memcmp(data.packet_data.vban, "VBAN", 4) != 0) {
                fprintf(stderr, "Invalid VBAN header\n");
                continue;
            }
            
            if (memcmp(data.packet_data.streamname, stream_name, strlen(stream_name)) != 0) {
                continue;
            }

            if (vban_frame == 0) {
                vban_frame = data.packet_data.frame_num;
            } else {
                uint32_t expected_frame = vban_frame + 1;

                if (data.packet_data.frame_num != expected_frame) {
                    int32_t diff = (int32_t)(data.packet_data.frame_num - expected_frame);
                    if (diff > 0) {
                        printf("Dropped %d packet(s)\n", diff);
                    } else if (diff < 0) {
                        printf("Late or duplicate packet\n");
                    }

                    vban_frame = data.packet_data.frame_num;
                } else {
                    vban_frame = expected_frame;
                }
            }

            if(vban_last_sr != data.packet_data.sample_rate_idx) {
                vban_last_sr = data.packet_data.sample_rate_idx;
                printf("New sample rate of %ld\n", VBAN_SRList[vban_last_sr % VBAN_SR_MAXNUMBER]);
                vban_audio_reset = 1;
                audio_buffer->count = 0;
            }
            
            if(vban_last_format != data.packet_data.format_type) {
                vban_last_format = data.packet_data.format_type;
                printf("New data format of %s\n", VBAN_TextBITList[vban_last_format % VBAN_BIT_MAXNUMBER]);
                vban_audio_reset = 1;
                audio_buffer->count = 0;
            }
            
            if(vban_last_channels != data.packet_data.sample_channels) {
                vban_last_channels = data.packet_data.sample_channels;
                printf("New channel count of %d\n", vban_last_channels + 1); // Add 1 because VBAN channels are 0-based
                vban_audio_reset = 1;
                audio_buffer->count = 0;
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