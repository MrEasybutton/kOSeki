#ifndef STRING_H
#define STRING_H

#include "types.h"
#include "multiboot.h"
#include <stdarg.h>

uint32 digit_count(int num, int base);

void kfree(void* memory);

void *memset(void *dst, int c, size_t n);
void *memcpy(void *dst, const void *src, uint32 n);
void *memmove(void *dst, const void *src, uint32 n);

void append_str(char* buf, const char* str);
void append_int(char* buf, int value);
void append_hex(char* buf, unsigned int value);
void append_char(char* buf, char c);

int memcmp(const void *s1, const void *s2, uint32 n);

char* strstr(const char* haystack, const char* needle);
char* strcasestr(const char* haystack, const char* needle);

int strncmp(const char *s1, const char *s2, uint32 n); 

int strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, uint32 n);
char* strdup(const char* s);
void strcat(char *dest, const char *src);
void strncat(char *dest, const char *src, int n);

long strtol(const char* str, char** endptr, int base);
int atoi(const char* str);
long atol(const char* str);
double atof(const char* str);

int is_ws(char c);
int isalpha(char c);
char upper(char c);
char lower(char c);

void itoa(char *buf, int base, int d);
void *malloc(size_t nbytes);
void free(void* ptr);
int strtoi(char* ch);
int toDec(int hex, int base);

char* strtok(char* str, const char* delimiters);
char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, size_t n, const char *fmt, ...);
int vsnprintf(char *buf, size_t n, const char *fmt, va_list args);

#endif
