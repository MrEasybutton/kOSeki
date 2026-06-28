#include "mbedtls/entropy_poll.h"
#include "cmos.h"
#include "kheap.h"
#include "string.h"
#include "serial.h"

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen) {
    (void)data;

    // entropy

    time_t t;
    get_time(&t);

    uint32_t seed = (uint32_t)t.second
                  ^ ((uint32_t)t.minute  << 8)
                  ^ ((uint32_t)t.hour    << 16)
                  ^ ((uint32_t)t.day     << 24)
                  ^ ((uint32_t)t.month   * 1000003u)
                  ^ ((uint32_t)t.year    * 2999999u);

    // sum RDTSC jitter
    uint64_t tsc;
    __asm__ volatile ("rdtsc" : "=A"(tsc));
    seed ^= (uint32_t)(tsc & 0xFFFFFFFF);
    seed ^= (uint32_t)(tsc >> 32) * 1664525u;

    static uint32_t cc = 0;
    seed ^= ++cc * 2246822519u;

    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;

    for (size_t i = 0; i < len; i++) {
        seed = seed * 1664525u + 1013904223u; // knuth (no wacky hijinks here)
        output[i] = (unsigned char)(seed >> 24); //hi
    }

    *olen = len;
    return 0;
}

uint32_t mbedtls_time(uint32_t* t) {
    time_t tm;
    get_time(&tm);

    uint32_t timestamp = (tm.year - 1970) * 31536000 + (tm.month * 2592000) + (tm.day * 86400) + (tm.hour * 3600) + (tm.minute * 60) + tm.second;
    if (t) *t = timestamp;
    return timestamp;
}

void* mbedtls_calloc(size_t nmemb, size_t size) {
    return kcalloc(nmemb, size);
}

void mbedtls_free(void* ptr) {
    kfree(ptr);
}
