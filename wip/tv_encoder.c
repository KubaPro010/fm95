// This will encode a black and white TV signal using a luminance value, how does it work?
/*
    It encodes the luminance into negative values, so totally white pixel should output -1, a black one should be 0
    
    Every new line it sends a 0.5, every frame it is a 1.0
*/

#include "../lib/fm_modulator.h"

unsigned int rgb_to_luminance(unsigned int r, unsigned int g, unsigned int b) {
    return (unsigned int)(0.299 * r + 0.587 * g + 0.114 * b);
}

typedef struct {
    int line;
    int pixel;
    int lines;
    int pixels;
} TVEncoder;

void init_tv_modulator(TVEncoder* tv, int lines, int pixels) {
    tv->pixels = pixels;
    tv->lines = lines;
    tv->line = 0;
    tv->pixel = 0;
}

float tv_encode(TVEncoder* tv, float luminance) {
    float normalized_luminance = luminance / 255.0f;  // Normalize luminance to [0, 1]

    if (tv->line < tv->lines) {
        if (tv->pixel < tv->pixels) {
            // Process pixel within the current line
            tv->pixel++;
            return -normalized_luminance;
        } else {
            // End of line: reset pixel counter and move to the next line
            tv->pixel = 0;
            tv->line++;
            return 0.5f;
        }
    } else {
        // End of frame: reset frame counters
        tv->line = 0;
        tv->pixel = 0;
        return 1.0f;
    }
}