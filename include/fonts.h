#ifndef FONTS_H
#define FONTS_H

#include "types.h"

typedef enum {
    FONT_FUZZY,
    FONT_KALNIA,
    FONT_DEFAULT = FONT_KALNIA
} font_t;

typedef unsigned int (*get_char_func_ptr)(int, int);

typedef struct {
    int width;
    int height;
    get_char_func_ptr get_char;
} FontInfo;

const FontInfo* get_font_info(font_t font_type);

#endif