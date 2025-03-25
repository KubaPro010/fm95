#pragma once
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_2PI
#define M_2PI (M_PI * 2.0)
#endif

inline float sincf(float x) {
    return (x == 0.0f) ? 1.0f : sinf(M_PI * x) / (M_PI * x);
}