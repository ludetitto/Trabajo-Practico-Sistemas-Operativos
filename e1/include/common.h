#pragma once
#include <stdint.h>
#define RING_B 64
#define MAX_STR 32
typedef struct {
    uint64_t id;
    char f1[MAX_STR], f2[MAX_STR];
    double f3;
} record_t;
