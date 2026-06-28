#ifndef STDLIB_H
#define STDLIB_H

#include "types.h"

static inline int abs(int x) { return x < 0 ? -x : x; }

void* malloc(size_t size);
void free(void* ptr);

int atoi(const char* str);
double atof(const char* str);

#endif
