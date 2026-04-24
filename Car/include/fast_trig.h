#ifndef FAST_TRIG_H
#define FAST_TRIG_H

#include <stdint.h>

#define FT_PI       3.14159265f
#define FT_TWO_PI   6.28318530f
#define FT_HALF_PI  1.57079632f

#pragma CODE_SECTION(_ft_sin_q1, ".TI.ramfunc")
#pragma CODE_SECTION(atan2f, ".TI.ramfunc")
#pragma CODE_SECTION(sqrtf, ".TI.ramfunc")

static inline float fmaxf(float a, float b)
{
    return a > b ? a : b;
}

static inline float sqrtf(float x)
{
    if (x <= 0.0f) return 0.0f;

    float h = 0.5f * x;
    uint32_t i;
    __builtin_memcpy(&i, &x, sizeof(i));
    i = 0x5f3759df - (i >> 1);
    float y;
    __builtin_memcpy(&y, &i, sizeof(y));
    y = y * (1.5f - h * y * y);
    return x * y;
}

static inline float _ft_sin_q1(float x)
{
    float x2 = x * x;
    return x * (0.99997937f + x2 * (-0.16649714f + x2 * 0.00799440f));
}

static inline float sinf(float x)
{
    if (x <= FT_HALF_PI)                return  _ft_sin_q1(x);
    if (x <= FT_PI)                     return  _ft_sin_q1(FT_PI - x);
    if (x <= FT_PI + FT_HALF_PI)       return -_ft_sin_q1(x - FT_PI);
    return                                     -_ft_sin_q1(FT_TWO_PI - x);
}

static inline float cosf(float x)
{
    float s = x + FT_HALF_PI;
    if (s >= FT_TWO_PI) s -= FT_TWO_PI;
    return sinf(s);
}

static inline float atan2f(float y, float x)
{
    if (x == 0.0f) {
        if (y > 0.0f) return  FT_HALF_PI;
        if (y < 0.0f) return -FT_HALF_PI;
        return 0.0f;
    }

    float abs_y = y < 0.0f ? -y : y;
    float abs_x = x < 0.0f ? -x : x;
    float t, angle;
    int swapped = 0;

    if (abs_y > abs_x) {
        t = abs_x / abs_y;
        swapped = 1;
    } else {
        t = abs_y / abs_x;
    }

    float t2 = t * t;
    angle = t * (0.99997726f + t2 * (-0.33262347f + t2 * 0.19354346f));

    if (swapped) angle = FT_HALF_PI - angle;
    if (x < 0.0f) angle = FT_PI - angle;
    if (y < 0.0f) angle = -angle;

    return angle;
}

#endif /* FAST_TRIG_H */
