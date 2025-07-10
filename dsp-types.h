#ifndef LPCSDR_DSP_TYPES_H
#define LPCSDR_DSP_TYPES_H

#include <stdint.h>

typedef struct {
    double i;
    double q;
} __attribute__((packed)) cs16_t;

typedef struct {
    float i;
    float q;
} __attribute__((packed)) cf32_t;

#endif /* LPCSDR_DSP_TYPES_H */