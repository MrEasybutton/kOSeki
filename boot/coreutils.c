#include <stdbool.h>

// semantics (reminder add in methods from utilities)

int strlen(const char* str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

// meth

float sqrt(float number) {
    int i;
    float x2, y;
    const float threehalfs = 1.5F;

    x2 = number * 0.5F;
    y = number;
    i = *(int*)&y;
    i = 0x5f3759df - (i >> 1);
    y = *(float*)&i;
    y = y * (threehalfs - (x2 * y * y));

    return number * y;
}

float sin(float x) {
    const float PI = 3.14159;
    const float TWO_PI = 2.0 * PI;
    
    while (x < 0.0)
        x += TWO_PI;
    
    while (x >= TWO_PI)
        x -= TWO_PI;
    
    if (x > PI)
        x -= TWO_PI;
    
    float x2 = x * x;
    float x3 = x2 * x;
    float x5 = x3 * x2;
    float x7 = x5 * x2;
    
    return x - (x3 / 6.0) + (x5 / 120.0) - (x7 / 5040.0);
}

float cos(float x) {
    const float PI = 3.14159;
    return sin(x + PI / 2.0);
}

// pseudorandom stuff

static unsigned int stunseed = 81800;

int srand() {
    stunseed = (1103515245 * stunseed + 81800) & 0x7fffffff;
    return stunseed;
}

int srand_clamp(int max, int seed) {
    seed = (1103515245 * seed + 81800) & 0x7fffffff;
    return seed % max;
}

int rand_range_spawn(int low, int high) {
    if (low > high) {
        int temp = low;
        low = high;
        high = temp;
    }
    
    int range = high - low + 1;
    return (srand() % range) + low;
}

