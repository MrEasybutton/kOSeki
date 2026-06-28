#include "string.h"
#include "types.h"
#include "kheap.h"
#include "kmath.h"
#include <stdarg.h>
#include <stdint.h>

#define EOF (-1)

uint32 digit_count(int num, int base) {
    uint32 count = 0;
    if (num == 0) return 1;
    while (num > 0) {
        num /= base;
        count++;
    }
    return count;
}

void append_str(char* buf, const char* str) {
    strcat(buf, str);
}

void append_int(char* buf, int value) {
    char temp[32];
    itoa(temp, 'd', value);
    strcat(buf, temp);
}

void append_hex(char* buf, unsigned int value) {
    char temp[32];
    itoa(temp, 'x', value);
    strcat(buf, temp);
}

void append_char(char* buf, char c) {
    int len = strlen(buf);
    buf[len] = c;
    buf[len+1] = '\0';
}

void *memset(void *dst, int c, size_t n) {
    unsigned char *bp = (unsigned char *)dst;
    while (n > 0 && ((uint32)bp & 3)) {
        *bp++ = (unsigned char)c;
        n--;
    }
    uint32 val = (unsigned char)c;
    val |= val << 8;
    val |= val << 16;
    val |= val << 24;
    uint32 *wp = (uint32 *)bp;
    uint32 words = n >> 2;
    uint32 bytes = n & 3;
    while (words--) {
        *wp++ = val;
    }
    bp = (unsigned char *)wp;
    while (bytes--) {
        *bp++ = (unsigned char)c;
    }
    return dst;
}

int memcmp(const void *s1, const void *s2, uint32 n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    for (uint32 i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

void *memcpy(void *dst, const void *src, size_t n) { // prev a 4bit copy
    uint8_t *d = dst;
    const uint8_t *s = src;

    if (((uintptr_t)d & 7) == ((uintptr_t)s & 7)) {
        while (n && ((uintptr_t)d & 7)) {
            *d++ = *s++;
            n--;
        }

        uint64_t *wd = (uint64_t*)d;
        const uint64_t *ws = (const uint64_t*)s;

        size_t qwords = n >> 3;

        while (qwords--) {
            *wd++ = *ws++;
        }

        d = (uint8_t*)wd;
        s = (const uint8_t*)ws;

        n &= 7;
    }

    while (n--) *d++ = *s++;

    return dst;
}

void *memmove(void *dst, const void *src, uint32 n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

int strlen(const char *s) {
    int i = 0;
    while (s[i] != '\0') i++;
    return i;
}

char* strstr(const char* hs, const char* nd) {
    if (!hs || !nd) return NULL;
    if (*nd == '\0') return (char*)hs;
    while (*hs) {
        const char* h = hs;
        const char* n = nd;
        while (*h && *n && (*h == *n)) {
            h++;
            n++;
        }
        if (*n == '\0') return (char*)hs;
        hs++;
    }
    return NULL;
}

char* strcasestr(const char* hs, const char* nd) {
    if (!hs || !nd) return NULL;
    if (*nd == '\0') return (char*)hs;
    while (*hs) {
        const char* h = hs;
        const char* n = nd;
        while (*h && *n) {
            char h_c = (*h >= 'A' && *h <= 'Z') ? (*h + 32) : *h;
            char n_c = (*n >= 'A' && *n <= 'Z') ? (*n + 32) : *n;
            if (h_c != n_c) break;
            h++; n++;
        }
        if (*n == '\0') return (char*)hs;
        hs++;
    }
    return NULL;
}

int strncmp(const char *s1, const char *s2, uint32 n) {
    for (uint32 i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return (unsigned char)s1[i] - (unsigned char)s2[i];
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

int strcmp(const char *s1, const char *s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        i++;
    }
    return s1[i] - s2[i];
}

int strcpy(char *dst, const char *src) {
    int i = 0;
    while (src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    return i;
}

char *strncpy(char *dst, const char *src, uint32 n) {
    uint32 i = 0;
    while (i < n && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    while (i < n) {
        dst[i] = '\0';
        i++;
    }
    return dst;
}

char* strdup(const char* s) {
    if (!s) return NULL;
    int len = strlen(s);
    char* d = (char*)kmalloc(len + 1);
    if (!d) return NULL;
    memcpy(d, s, len + 1);
    return d;
}

void strcat(char *dest, const char *src) {
    int i = 0, j = 0;
    while (dest[i] != '\0') i++;
    while (src[j] != '\0') {
        dest[i] = src[j];
        i++;
        j++;
    }
    dest[i] = '\0';
}

void strncat(char *dest, const char *src, int n) {
    int dest_len = strlen(dest);
    int i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[dest_len + i] = src[i];
    }
    dest[dest_len + i] = '\0';
}

#ifndef LONG_MAX
#define LONG_MAX 0x7FFFFFFFL
#endif
#ifndef LONG_MIN
#define LONG_MIN (-LONG_MAX - 1L)
#endif
#ifndef ULONG_MAX
#define ULONG_MAX 0xFFFFFFFFUL
#endif

int is_ws(char c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}

static int chtod(char c, int base)
{
    int val;
    if (c >= '0' && c <= '9') val = c - '0';
    else if (c >= 'a' && c <= 'z') val = c - 'a' + 10;
    else if (c >= 'A' && c <= 'Z') val = c - 'A' + 10;
    else return -1;

    return (val < base) ? val : -1;
}

unsigned long strtoul(const char* str, char** endptr, int base)
{
    const char* p = str;

    while (is_ws(*p)) p++;

    int negative = 0;
    if (*p == '+') {
        p++;
    } else if (*p == '-') {
        negative = 1;
        p++;
    }

    if (base == 0) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            base = 16;
            p += 2;
        } else if (p[0] == '0') {
            base = 8;
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
            p += 2;
    }

    unsigned long result = 0;
    int any_digits = 0;
    int overflow = 0;

    while (1) {
        int d = chtod(*p, base);
        if (d < 0) break;

        if (!overflow) {
            if (result > (ULONG_MAX - (unsigned long)d) / (unsigned long)base)
                overflow = 1;
            else
                result = result * (unsigned long)base + (unsigned long)d;
        }
        any_digits = 1;
        p++;
    }

    if (endptr)
        *endptr = (char*)(any_digits ? p : str);

    if (overflow)
        return ULONG_MAX;

    return negative ? (unsigned long)(-(long)result) : result;
}

long strtol(const char* str, char** endptr, int base)
{
    const char* p = str;

    while (is_ws(*p)) p++;

    int negative = 0;
    if (*p == '+') {
        p++;
    } else if (*p == '-') {
        negative = 1;
        p++;
    }

    if (base == 0) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            base = 16;
            p += 2;
        } else if (p[0] == '0') {
            base = 8;
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
            p += 2;
    }

    long result = 0;
    int any_digits = 0;
    int overflow = 0;

    unsigned long abs_limit = (unsigned long)LONG_MAX + (negative ? 1UL : 0);
    unsigned long acc = 0;

    while (1) {
        int d = chtod(*p, base);
        if (d < 0) break;

        if (!overflow) {
            if (acc > (abs_limit - (unsigned long)d) / (unsigned long)base)
                overflow = 1;
            else
                acc = acc * (unsigned long)base + (unsigned long)d;
        }
        any_digits = 1;
        p++;
    }

    if (endptr)
        *endptr = (char*)(any_digits ? p : str);

    if (overflow)
        return negative ? LONG_MIN : LONG_MAX;

    result = negative ? -(long)acc : (long)acc;
    return result;
}

int atoi(const char* str) { return (int)strtol(str, (char**)0, 10); }
long atol(const char* str) { return strtol(str, (char**)0, 10); }

double atof(const char* str) {
    double res = 0.0;
    double f = 1.0;
    int dec = 0;
    
    while (is_ws(*str)) str++;
    
    if (*str == '-') {
        f = -1.0;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    while (*str) {
        if (*str == '.') {
            if (dec) break;
            dec = 1;
        } else if (*str >= '0' && *str <= '9') {
            if (dec) f /= 10.0;
            res = res * 10.0 + (*str - '0');
        } else {
            break;
        }
        str++;
    }
    return res * f;
}

int isalpha(char c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

char upper(char c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}

char lower(char c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

void itoa(char *buf, int base, int d) {
    char *p = buf;
    char *p1, *p2;
    unsigned long ud = d;
    int divisor = 10;
    if (base == 'd' && d < 0) {
        *p++ = '-';
        buf++;
        ud = -d;
    } else if (base == 'x') divisor = 16;
    do {
        unsigned long remainder = ud % divisor;
        *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    } while (ud /= divisor);
    *p = 0;
    p1 = buf;
    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
}

void *malloc(size_t nbytes) { return (void *)kmalloc(nbytes); }

void free(void* ptr) { kfree(ptr); }

int strtoi(char *ch) {
    int i = 0, sign = 1, res = 0;
    if (ch[0] == '-') {
        sign = -1;
        i++;
    }
    for (; ch[i] != '\0'; ++i) {
        if (ch[i] < '0' || ch[i] > '9') return EOF;
        res = res * 10 + ch[i] - '0';
    }
    return sign * res;
}

int toDec(int hex, int base) {
    int dec = 0, i = 0, rem;
    while (hex != 0) {
        rem = hex % 10;
        hex /= 10;
        dec += rem * power(base, i);
        ++i;
    }
    return dec;
}

static int is_delim(char c, const char* delimiters) {
    while (*delimiters != '\0') {
        if (c == *delimiters) return 1;
        delimiters++;
    }
    return 0;
}

char* strtok(char* str, const char* delimiters) {
    static char *p = NULL;

    if (str != NULL) p = str;
    else if (p == NULL || *p == '\0') return NULL;

    while (*p != '\0' && is_delim(*p, delimiters)) p++;

    if (*p == '\0') return NULL;
    char* token_start = p;
    while (*p != '\0' && !is_delim(*p, delimiters)) p++;

    if (*p != '\0') {
        *p = '\0';
        p++;
    }
    return token_start;
}

char* strchr(const char* s, int c) {
    while (*s != '\0' && *s != (char)c) s++;
    return (*s == (char)c) ? (char*)s : NULL;
}

char* strrchr(const char* s, int c) {
    const char* last = 0;
    do {
        if (*s == (char)c) last = s;
    } while (*s++);
    return (char*)last;
}

static void ftoa(char *buf, double val, int prec) {
    char *p = buf;

    if (val < 0.0) {
        *p++ = '-';
        val = -val;
    }

    if (prec < 0) prec = 6;
    if (prec > 17) prec = 17;

    double rounder = 0.5;
    for (int i = 0; i < prec; i++) rounder /= 10.0;
    val += rounder;

    unsigned long ipart = (unsigned long)val;
    double fpart = val - (double)ipart;

    char *istart = p;
    if (ipart == 0) {
        *p++ = '0';
    } else {
        unsigned long tmp = ipart;
        while (tmp > 0) {
            *p++ = '0' + (tmp % 10);
            tmp /= 10;
        }
        char *a = istart, *b = p - 1;
        while (a < b) { char t = *a; *a = *b; *b = t; a++; b--; }
    }

    if (prec > 0) {
        *p++ = '.';
        for (int i = 0; i < prec; i++) {
            fpart *= 10.0;
            int digit = (int)fpart;
            *p++ = '0' + digit;
            fpart -= digit;
        }
    }

    *p = '\0';
}

int vsnprintf(char *buf, size_t n, const char *fmt, va_list args) {
    if (n == 0) return 0;
    size_t pos = 0;
    while (*fmt && pos < n - 1) {
        if (*fmt != '%') {
            buf[pos++] = *fmt++;
            continue;
        }
        fmt++;

        int prec = -1;
        if (*fmt == '.') {
            fmt++;
            prec = 0;
            while (*fmt >= '0' && *fmt <= '9') {
                prec = prec * 10 + (*fmt - '0');
                fmt++;
            }
        }

        switch (*fmt) {
            case 'f': {
                double v = va_arg(args, double);
                char tmp[64];
                ftoa(tmp, v, (prec < 0) ? 6 : prec);
                for (int i = 0; tmp[i] && pos < n - 1; i++) buf[pos++] = tmp[i];
                break;
            }
            case 'g': {
                double v = va_arg(args, double);
                char tmp[64];
                ftoa(tmp, v, (prec < 0) ? 6 : prec);
                
                if (strchr(tmp, '.')) {
                    int last = strlen(tmp) - 1;
                    while (last >= 0 && tmp[last] == '0') {
                        tmp[last--] = '\0';
                    }
                    if (last >= 0 && tmp[last] == '.') {
                        tmp[last] = '\0';
                    }
                }
                
                for (int i = 0; tmp[i] && pos < n - 1; i++) buf[pos++] = tmp[i];
                break;
            }
            case 'd':
            case 'i': {
                int v = va_arg(args, int);
                char tmp[32];
                itoa(tmp, 'd', v);
                for (int i = 0; tmp[i] && pos < n - 1; i++) buf[pos++] = tmp[i];
                break;
            }
            case 'u': {
                unsigned int v = va_arg(args, unsigned int);
                char tmp[32];
                itoa(tmp, 'd', v);
                for (int i = 0; tmp[i] && pos < n - 1; i++) buf[pos++] = tmp[i];
                break;
            }
            case 'x': {
                unsigned int v = va_arg(args, unsigned int);
                char tmp[32];
                itoa(tmp, 'x', v);
                for (int i = 0; tmp[i] && pos < n - 1; i++) buf[pos++] = tmp[i];
                break;
            }
            case 'c': {
                char c = (char)va_arg(args, int);
                buf[pos++] = c;
                break;
            }
            case 's': {
                char *s = va_arg(args, char *);
                if (!s) s = "(null)";
                for (int i = 0; s[i] && pos < n - 1; i++) buf[pos++] = s[i];
                break;
            }
            case '%': buf[pos++] = '%'; break;
            default: buf[pos++] = '%'; if (pos < n - 1) buf[pos++] = *fmt; break;
        }
        fmt++;
    }
    buf[pos] = '\0';
    return pos;
}

int snprintf(char *buf, size_t n, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int res = vsnprintf(buf, n, fmt, args);
    va_end(args);
    return res;
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int res = vsnprintf(buf, (size_t)-1, fmt, args);
    va_end(args);
    return res;
}