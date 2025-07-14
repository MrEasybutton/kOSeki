#ifndef COREUTILS_H
#define COREUTILS_H
#include <stdbool.h>

int strlen(const char* str);
float sqrt(float number);
float sin(float x);
float cos(float x);
int srand();
int srand_clamp(int max, int seed_input);
int rand_range_spawn(int low, int high);

#endif