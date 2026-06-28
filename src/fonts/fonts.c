#include "../../include/fonts.h"

extern const int fuzzy_width;
extern const int fuzzy_height;
extern unsigned int fuzzy_get_char(int index, int y);

extern const int kalnia_width;
extern const int kalnia_height;
extern unsigned int kalnia_get_char(int index, int y);

static const FontInfo fuzzy_font_info = {
    .width = 10,
    .height = 15,
    .get_char = fuzzy_get_char
};

static const FontInfo kalnia_font_info = {
    .width = 10,
    .height = 15,
    .get_char = kalnia_get_char
};

const FontInfo* get_font_info(font_t font_type) {
    switch (font_type) {
        case FONT_FUZZY:
            return &fuzzy_font_info;
        case FONT_KALNIA:
            return &kalnia_font_info;
        default:
            return &kalnia_font_info;
    }
}