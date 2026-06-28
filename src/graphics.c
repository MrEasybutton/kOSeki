#include "console.h"
#include "graphics.h"
#include "vesa.h"
#include "../include/fonts.h"

extern uint32 *g_back_buffer;
extern uint32 g_width;
extern uint32 g_height;

static int g_clip_x = 0;
static int g_clip_y = 0;
static int g_clip_w = 4096;
static int g_clip_h = 4096;

void set_clip(int x, int y, int w, int h) {
    g_clip_x = x;
    g_clip_y = y;
    g_clip_w = w;
    g_clip_h = h;
}

void reset_clip() {
    g_clip_x = 0;
    g_clip_y = 0;
    g_clip_w = 4096;
    g_clip_h = 4096;
}

#define FF_PUTPIXEL(x, y, color) g_back_buffer[(y) * g_width + (x)] = (color)
#define FF_GETPIXEL(x, y) g_back_buffer[(y) * g_width + (x)]


void rect(int x, int y, int width, int height, uint32 color) {
    
    if (width <= 0 || height <= 0) return;

    uint32 screen_width = vbe_get_width();
    uint32 screen_height = vbe_get_height();

    int start_x = (x < 0) ? 0 : x;
    int start_y = (y < 0) ? 0 : y;
    int end_x = (x + width > (int)screen_width) ? (int)screen_width : x + width;
    int end_y = (y + height > (int)screen_height) ? (int)screen_height : y + height;

    if (start_x < g_clip_x) start_x = g_clip_x;
    if (start_y < g_clip_y) start_y = g_clip_y;
    if (end_x > g_clip_x + g_clip_w) end_x = g_clip_x + g_clip_w;
    if (end_y > g_clip_y + g_clip_h) end_y = g_clip_y + g_clip_h;

    
    if (start_x >= (int)screen_width || 
        start_y >= (int)screen_height ||
        end_x <= 0 || end_y <= 0 || start_x >= end_x || start_y >= end_y) {
        return;
    }

    int line_width = end_x - start_x;

    uint8 alpha = (color >> 24) & 0xFF;
    if (alpha == 0) return;
    if (alpha == 255) {
        for (int draw_y = start_y; draw_y < end_y; draw_y++) {
            uint32* scanline = &g_back_buffer[draw_y * screen_width + start_x];

            if (start_x >= 0 && end_x <= (int)screen_width) {
                for (int i = 0; i < line_width; i++) {
                    scanline[i] = color;
                }
            } else {
                for (int draw_x = start_x; draw_x < end_x; draw_x++) {
                    if (draw_x >= 0 && draw_x < (int)screen_width) {
                        FF_PUTPIXEL(draw_x, draw_y, color);
                    }
                }
            }
        }
    } else {
        for (int draw_y = start_y; draw_y < end_y; draw_y++) {
            for (int draw_x = start_x; draw_x < end_x; draw_x++) {
                pixel(draw_x, draw_y, color);
            }
        }
    }
}


void rect_grad(int x, int y, int width, int height, uint32 start_color, uint32 end_color) {
    if (width <= 0 || height <= 0) return;

    uint32 screen_width = vbe_get_width();
    uint32 screen_height = vbe_get_height();
    
    int start_x = (x < 0) ? 0 : x;
    int start_y = (y < 0) ? 0 : y;
    int end_x = (x + width > (int)screen_width) ? (int)screen_width : x + width;
    int end_y = (y + height > (int)screen_height) ? (int)screen_height : y + height;

    if (start_x < g_clip_x) start_x = g_clip_x;
    if (start_y < g_clip_y) start_y = g_clip_y;
    if (end_x > g_clip_x + g_clip_w) end_x = g_clip_x + g_clip_w;
    if (end_y > g_clip_y + g_clip_h) end_y = g_clip_y + g_clip_h;

    
    if (start_x >= (int)screen_width || 
        start_y >= (int)screen_height ||
        end_x <= 0 || end_y <= 0 || start_x >= end_x || start_y >= end_y) {
        return;
    }
    
    int line_width = end_x - start_x;
    
    if (start_color == end_color) {
        rect(x, y, width, height, start_color);
        return;
    }
    
    uint8 start_r = (start_color >> 16) & 0xFF;
    uint8 start_g = (start_color >> 8) & 0xFF;
    uint8 start_b = start_color & 0xFF;
    uint8 start_a = (start_color >> 24) & 0xFF;

    uint8 end_r = (end_color >> 16) & 0xFF;
    uint8 end_g = (end_color >> 8) & 0xFF;
    uint8 end_b = end_color & 0xFF;
    uint8 end_a = (end_color >> 24) & 0xFF;

    if (height <= 1) {
        rect(x, y, width, height, start_color);
        return;
    }
    
    float r_step = (float)(end_r - start_r) / (height - 1);
    float g_step = (float)(end_g - start_g) / (height - 1);
    float b_step = (float)(end_b - start_b) / (height - 1);
    float a_step = (float)(end_a - start_a) / (height - 1);

    
    for (int draw_y = start_y; draw_y < end_y; draw_y++) {
        
        int rel_y = draw_y - y;
        if (rel_y < 0) rel_y = 0;
        if (rel_y >= height) rel_y = height - 1;

        uint8 r = start_r + (int)(r_step * rel_y);
        uint8 g = start_g + (int)(g_step * rel_y);
        uint8 b = start_b + (int)(b_step * rel_y);
        uint8 a = start_a + (int)(a_step * rel_y);

        uint32 color = (a << 24) | (r << 16) | (g << 8) | b;
        
        uint32* scanline = &g_back_buffer[draw_y * screen_width + start_x];

        
        if (a == 255) {
            
            if (start_x >= 0 && end_x <= (int)screen_width) {
                
                for (int i = 0; i < line_width; i++) {
                    scanline[i] = color;
                }
            } else {
                
                for (int draw_x = start_x; draw_x < end_x; draw_x++) {
                    if (draw_x >= 0 && draw_x < (int)screen_width) {
                        FF_PUTPIXEL(draw_x, draw_y, color);
                    }
                }
            }
        }
        else if (a > 0) {
            
            for (int draw_x = start_x; draw_x < end_x; draw_x++) {
                pixel(draw_x, draw_y, color);
            }
        }
        
    }
}

void circle(int center_x, int center_y, int radius, uint32 color) {
    if (radius <= 0) return;
    
    int screen_width = vbe_get_width();
    int screen_height = vbe_get_height();
    
    if (center_x + radius < 0 || center_x - radius >= screen_width ||
        center_y + radius < 0 || center_y - radius >= screen_height) {
        return;
    }
    
    void plot_pixel(int x, int y) {
        if (x >= 0 && x < screen_width && y >= 0 && y < screen_height) {
            pixel(x, y, color);
        }
    }
    
    int x = radius;
    int y = 0;
    int err = 0;

    while (x >= y) {
        
        plot_pixel(center_x + x, center_y + y);
        plot_pixel(center_x + y, center_y + x);
        plot_pixel(center_x - y, center_y + x);
        plot_pixel(center_x - x, center_y + y);
        plot_pixel(center_x - x, center_y - y);
        plot_pixel(center_x - y, center_y - x);
        plot_pixel(center_x + y, center_y - x);
        plot_pixel(center_x + x, center_y - y);

        if (err <= 0) {
            y += 1;
            err += 2 * y + 1;
        } else {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}

// nearest neighbour for Calli header
void text_ex(const char* str, int x, int y, uint32 colour, font_t font_type, bool bold, int scale) {
    if (!str) return;
    if (scale <= 0) scale = 1;

    const FontInfo* font_info = get_font_info(font_type);
    if (!font_info) return;

    const int sw = (int)g_width;
    const int sh = (int)g_height;
    const int font_w = font_info->width;
    const int font_h = font_info->height;

    if (y >= sh || y + font_h * scale <= 0) return;

    const uint8 alpha = (colour >> 24) & 0xFF;
    if (alpha == 0) return;
    const bool opaque = (alpha == 255);

    const int px_span = scale + (bold ? 1 : 0);
    const int char_step = (font_w + 1) * scale + (bold ? 1 : 0);

    int cx = x;
    for (int ci = 0; str[ci] != '\0' && ci < 1024; ci++, cx += char_step) {
        if (cx >= sw) break;
        if (cx + font_w * scale <= 0) continue;

        int code = (unsigned char)str[ci];
        if (code < 32 || code > 126) code = 32;

        if (scale == 1 && !bold) {
            // row major
            for (int row = 0; row < font_h; row++) {
                int py = y + row;
                if (py < 0 || py >= sh) continue;

                uint32 *row_ptr = g_back_buffer + py * sw;
                int current_cx = x;
                for (int ci = 0; str[ci] != '\0' && ci < 1024; ci++, current_cx += char_step) {
                    if (current_cx >= sw) break;
                    if (current_cx + font_w <= 0) continue;

                    int code = (unsigned char)str[ci];
                    if (code < 32 || code > 126) code = 32;

                    unsigned int bits = font_info->get_char(code, row);
                    if (!bits) continue;

                    for (int col = 0; col < font_w; col++) {
                        if ((bits >> (font_w - 1 - col)) & 1) {
                            int px = current_cx + col;
                            if (px >= 0 && px < sw) {
                                if (opaque) row_ptr[px] = colour;
                                else row_ptr[px] = blend_pixel(colour, row_ptr[px]);
                            }
                        }
                    }
                }
            }
        } else {
            for (int row = 0; row < font_h; row++) {
                int py = y + row * scale;
                if (py + scale <= 0 || py >= sh) continue;

                int y0 = py < 0 ? 0 : py;
                int y1 = py + scale > sh ? sh : py + scale;

                unsigned int bits = font_info->get_char(code, row);
                if (!bits) continue;

                for (int col = 0; col < font_w; col++) {
                    if ((bits >> (font_w - 1 - col)) & 1) {
                        int px = cx + col * scale;
                        if (px + px_span <= 0 || px >= sw) continue;

                        int x0 = px < 0 ? 0 : px;
                        int x1 = px + px_span > sw ? sw : px + px_span;

                        for (int sy = y0; sy < y1; sy++) {
                            uint32 *row_ptr = g_back_buffer + sy * sw;
                            if (opaque) {
                                for (int sx = x0; sx < x1; sx++) row_ptr[sx] = colour;
                            } else {
                                for (int sx = x0; sx < x1; sx++)
                                    row_ptr[sx] = blend_pixel(colour, row_ptr[sx]);
                            }
                        }
                    }
                }
            }
        }
    }
}

void text(const char* str, int x, int y, uint32 colour, font_t font_type, bool bold) {
    text_ex(str, x, y, colour, font_type, bold, 1);
}

// shld probably use this more often
void text_clip(const char* str, int x, int y, uint32 colour, font_t font_type) {
    if (!str) return;

    const FontInfo* font_info = get_font_info(font_type);
    if (!font_info) return;

    const int sw = (int)g_width;
    const int sh = (int)g_height;
    const uint8 alpha = (colour >> 24) & 0xFF;
    if (alpha == 0) return;
    const bool opaque = (alpha == 255);

    const int font_w = font_info->width;
    const int font_h = font_info->height;

    if (y >= sh || y + font_h <= 0) return;

    int cx = x;

    for (int ci = 0; str[ci] != '\0' && ci < 1024; ci++, cx += font_w) {
        if (cx >= sw) break;
        if (cx + font_w <= 0) continue;

        int code = (unsigned char)str[ci];
        if (code < 32 || code > 126) code = 32;

        for (int row = 0; row < font_h; row++) {
            int py = y + row;
            if (py < 0 || py >= sh) continue;

            unsigned int bits = font_info->get_char(code, row);
            if (!bits) continue;

            uint32 *row_ptr = g_back_buffer + py * sw;

            for (int col = 0; col < font_w; col++) {
                if ((bits >> (font_w - 1 - col)) & 1) {
                    int px = cx + col;
                    if (px >= 0 && px < sw) {
                        if (opaque) row_ptr[px] = colour;
                        else row_ptr[px] = blend_pixel(colour, row_ptr[px]);
                    }
                }
            }
        }
    }
}

void line(int x1, int y1, int x2, int y2, uint32 color) {
    int dx = (x2 > x1) ? x2 - x1 : x1 - x2;
    int dy = (y2 > y1) ? y2 - y1 : y1 - y2;

    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;

    int err = dx - dy;

    while (1) {
        pixel(x1, y1, color);

        if (x1 == x2 && y1 == y2) break;

        int e2 = 2 * err;

        if (e2 > -dy) {
            err -= dy; x1 += sx;
        }
        if (e2 < dx) {
            err += dx; y1 += sy;
        }
    }
}