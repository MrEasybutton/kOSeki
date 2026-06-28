#include "apps/WAHtercolour.h"
#include "procsys.h"
#include "gui.h"
#include "pon.h"
#include "graphics.h"
#include "string.h"
#include "kheap.h"
#include "vesa.h"
#include "kmath.h"
#include "bmp.h"
#include "serial.h"
#include "fat32.h"
#include "bae.h"
#include <stdint.h>

#define WIN_BORDER 2
#define TITLEBAR_H 20

#define WAH_CANVAS_X 15
#define WAH_CANVAS_Y 32
#define WAH_CANVAS_WIDTH 512
#define WAH_CANVAS_HEIGHT 360

#define WAH_DIALOG_WIDTH 300
#define WAH_DIALOG_HEIGHT 160

#define WAH_PANEL_X 545
#define WAH_PANEL_WIDTH 252

#define WAH_WHEEL_CX 671
#define WAH_WHEEL_CY 92
#define WAH_WHEEL_OUTER_RADIUS 62
#define WAH_WHEEL_INNER_RADIUS 45

#define WAH_TRIANGLE_APEX_Y -44
#define WAH_TRIANGLE_BASE_Y 26
#define WAH_TRIANGLE_HALF_WIDTH 36

#define WAH_SLIDER_X 572
#define WAH_SLIDER_Y 300
#define WAH_SLIDER_WIDTH 200
#define WAH_SLIDER_HEIGHT 12

#define WAH_PALETTE_BOX_X 558
#define WAH_PALETTE_BOX_Y 361
#define WAH_PALETTE_BOX_WIDTH 212
#define WAH_PALETTE_BOX_HEIGHT 60

#define WAH_MAX_BRUSH_SIZE 24
#define WAH_MIN_BRUSH_SIZE 1
#define WAH_MAX_INTENSITY 100
#define WAH_MIN_INTENSITY 0

typedef enum {
    BRUSH_ROUND = 0,
    BRUSH_SQUARE = 1,
    BRUSH_SPLATTER = 2
} WAHBrushType;

#define WAH_BRUSH_CARD_PAD 12
#define WAH_BRUSH_BTN_Y 196
#define WAH_BRUSH_BTN_H 34
#define WAH_BRUSH_BTN_GAP 6

// most of the app palettes are a mix of arbitrary numbers that look nice, google colour picker and chatgpt'd stuff lmao

#define COLOR_WAH_BACKGROUND RGB(41, 30, 56)
#define COLOR_WAH_PANEL RGB(73, 61, 89)
#define COLOR_WAH_NEU_LIGHT RGB(100, 86, 118)
#define COLOR_WAH_NEU_DARK RGB(46, 37, 58)
#define COLOR_WAH_NEU_IN_LIGHT RGB(86, 72, 104)
#define COLOR_WAH_NEU_IN_DARK RGB(38, 29, 48)
#define COLOR_WAH_BORDER RGB(198, 127, 80)
#define COLOR_WAH_BORDER_LIGHT RGB(232, 172, 130)
#define COLOR_WAH_BORDER_DARK RGB(138, 84, 44)
#define COLOR_WAH_ACCENT RGB(198, 127, 80)
#define COLOR_WAH_ACCENT_FILL RGB(138, 84, 44)
#define COLOR_WAH_TEXT RGB(209, 186, 219)
#define COLOR_WAH_TEXT_DIM RGB(148, 126, 163)
#define COLOR_WAH_SLIDER_BG RGB(41, 30, 56)
#define COLOR_WAH_SLIDER_THUMB RGB(198, 127, 80)
#define COLOR_WAH_DIALOG_ACCENT1 RGB(53, 12, 77)
#define COLOR_WAH_DIALOG_ACCENT2 RGB(95, 69, 110)
#define COLOR_WAH_CANVAS_BG RGB(245, 242, 248)
#define COLOR_WAH_CANVAS_BORDER RGB(33, 24, 44)

typedef enum {
    DIALOG_MODE_NONE,
    DIALOG_MODE_SAVE,
    DIALOG_MODE_LOAD
} WAHDialogMode;

typedef struct {
    uint32* canvas_pixels;
    uint32 selected_color;
    int hue;
    int intensity;
    int brush_size;
    BOOL drawing;
    BOOL slider_dragging;
    BOOL triangle_dragging;
    BOOL indicator_dragging;
    BOOL wheel_dragging;
    int last_draw_x;
    int last_draw_y;
    int indicator_x;
    WAHBrushType brush_type;

    char current_filename[MAX_FILENAME_LEN];
    PON_Comp* root_comp;
    PON_Comp* dialog_comp;
    PON_Comp* dialog_comp_to_free;
    BOOL dirty;

    WAHDialogMode dialog_mode;
    char dialog_input_buffer[MAX_FILENAME_LEN];
    int dialog_input_pos;
} WAHtercolourAppData;

static WAHtercolourAppData* app_data_global;
static uint32 apply_intensity(uint32 color, int intensity);

static void on_dialog_cancel(PON_Comp* comp, int rx, int ry);
static void on_save_confirm(PON_Comp* comp, int rx, int ry);
static void on_load_confirm(PON_Comp* comp, int rx, int ry);
static void on_save_click(PON_Comp* comp, int rx, int ry);
static void wahtercolour_dialog_draw_cb(PON_Comp* comp, int ax, int ay);
static void dialog_text_input_key(PON_Comp* comp, unsigned int key);

static void update_selected_color(WAHtercolourAppData* app_data);

static uint32 hue_to_rgb(int hue) {
    hue %= 360;
    if (hue < 0) hue += 360;

    int region = hue / 60;
    int remainder = (hue % 60) * 255 / 60;
    int r = 0, g = 0, b = 0;

    switch (region) {
        case 0: r = 255; g = remainder; b = 0; break;
        case 1: r = 255 - remainder; g = 255; b = 0; break;
        case 2: r = 0; g = 255; b = remainder; break;
        case 3: r = 0; g = 255 - remainder; b = 255; break;
        case 4: r = remainder; g = 0; b = 255; break;
        case 5: default: r = 255; g = 0; b = 255 - remainder; break;
    }

    return RGB(r, g, b);
}

static void fill_circle(int center_x, int center_y, int radius, uint32 color) {
    if (radius <= 0) return;

    int radius_sq = radius * radius;
    for (int dy = -radius; dy <= radius; dy++) {
        int row = center_y + dy;
        int dx_limit = (int)sqrt((float)(radius_sq - dy * dy));
        for (int dx = -dx_limit; dx <= dx_limit; dx++) {
            pixel(center_x + dx, row, color);
        }
    }
}

static inline void neu_border(int x, int y, int w, int h, uint32 tl_color, uint32 br_color) {
    rect(x + 3, y + 3, w - 4, 1, tl_color);
    rect(x + 3, y + 4, w - 4, 1, tl_color);

    rect(x + 3, y + 3, 1, h - 4, tl_color);
    rect(x + 4, y + 3, 1, h - 4, tl_color);

    rect(x + 3, y + h - 2, w - 4, 1, br_color);
    rect(x + 3, y + h - 1, w - 4, 1, br_color);

    rect(x + w - 2, y + 3, 1, h - 4, br_color);
    rect(x + w - 1, y + 3, 1, h - 4, br_color);
}

static void neu_raised(int x, int y, int w, int h) {
    rect(x + 2, y + h, w, 2, COLOR_WAH_NEU_DARK);
    rect(x + w, y + 2, 2, h, COLOR_WAH_NEU_DARK);

    rect(x, y, w, 2, COLOR_WAH_NEU_LIGHT);
    rect(x, y, 2, h, COLOR_WAH_NEU_LIGHT);

    rect(x + 2, y + 2, w - 2, h - 2, COLOR_WAH_PANEL);

    neu_border(
        x, y, w, h,
        COLOR_WAH_BORDER_LIGHT,
        COLOR_WAH_BORDER_DARK);
}

static void neu_inset(int x, int y, int w, int h, uint32 fill) {
    rect(x, y, w, 2, COLOR_WAH_NEU_IN_DARK);
    rect(x, y, 2, h, COLOR_WAH_NEU_IN_DARK);

    rect(x + 2, y + h, w, 2, COLOR_WAH_NEU_IN_LIGHT);
    rect(x + w, y + 2, 2, h, COLOR_WAH_NEU_IN_LIGHT);

    rect(x + 2, y + 2, w - 2, h - 2, fill);

    neu_border(
        x, y, w, h,
        COLOR_WAH_BORDER_DARK,
        COLOR_WAH_BORDER_LIGHT);
}

static void clear_canvas(WAHtercolourAppData* app_data) {
    if (!app_data || !app_data->canvas_pixels) return;
    for (int i = 0; i < WAH_CANVAS_WIDTH * WAH_CANVAS_HEIGHT; i++) {
        app_data->canvas_pixels[i] = COLOR_WAH_CANVAS_BG;
    }
    app_data->dirty = TRUE;
}

static void scale_bitmap_to_canvas(Bitmap* src, uint32* canvas_pixels) {
    if (!src || !src->data || !canvas_pixels) return;

    int src_w = src->width;
    int src_h = src->height;
    int dst_w = WAH_CANVAS_WIDTH;
    int dst_h = WAH_CANVAS_HEIGHT;

    for (int dy = 0; dy < dst_h; dy++) {
        int sy = (dy * src_h) / dst_h;
        if (sy >= src_h) sy = src_h - 1;
        if (sy < 0) sy = 0;

        for (int dx = 0; dx < dst_w; dx++) {
            int sx = (dx * src_w) / dst_w;
            if (sx >= src_w) sx = src_w - 1;
            if (sx < 0) sx = 0;
            
            uint32 color = src->data[sy * src_w + sx];
            canvas_pixels[dy * dst_w + dx] = color;
        }
    }
}

static void draw_canvas_pixels(WAHtercolourAppData* app_data, int origin_x, int origin_y) {
    if (!app_data || !app_data->canvas_pixels) return;

    int canvas_x = origin_x + WAH_CANVAS_X;
    int canvas_y = origin_y + WAH_CANVAS_Y;
    for (int y = 0; y < WAH_CANVAS_HEIGHT; y++) {
        for (int x = 0; x < WAH_CANVAS_WIDTH; x++) {
            int idx = y * WAH_CANVAS_WIDTH + x;
            pixel(canvas_x + x, canvas_y + y, app_data->canvas_pixels[idx]);
        }
    }
}

static uint32 apply_intensity(uint32 color, int intensity) {
    if (intensity < 0) intensity = 0;
    if (intensity > 100) intensity = 100;

    uint8 red = (color >> 16) & 0xFF;
    uint8 green = (color >> 8) & 0xFF;
    uint8 blue = color & 0xFF;

    red = (red * intensity) / 100;
    green = (green * intensity) / 100;
    blue = (blue * intensity) / 100;

    return RGBA(red, green, blue, 255);
}

static void draw_intensity_triangle(int origin_x, int origin_y, WAHtercolourAppData* app_data) {
    int wheel_x = origin_x + WAH_WHEEL_CX;
    int wheel_y = origin_y + WAH_WHEEL_CY;

    int preview_radius = 45;
    fill_circle(wheel_x, wheel_y, preview_radius, app_data->selected_color);
    circle(wheel_x, wheel_y, preview_radius + 1, COLOR_WAH_BORDER);

    int x1 = wheel_x - WAH_TRIANGLE_HALF_WIDTH;
    int y1 = wheel_y + WAH_TRIANGLE_BASE_Y;
    int x2 = wheel_x;
    int y2 = wheel_y + WAH_TRIANGLE_APEX_Y;
    int x3 = wheel_x + WAH_TRIANGLE_HALF_WIDTH;
    int y3 = wheel_y + WAH_TRIANGLE_BASE_Y;

    uint32 c1 = RGB(255, 255, 255);
    uint32 c2 = RGB(0, 0, 0);
    uint32 c3 = hue_to_rgb(app_data->hue);

    int min_y = y2;
    int max_y = y1;
    for (int y = min_y; y <= max_y; y++) {
        for (int x = x1; x <= x3; x++) {
            int den = (y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3);
            if (den == 0) continue;
            int u_num = (y2 - y3) * (x - x3) + (x3 - x2) * (y - y3);
            int v_num = (y3 - y1) * (x - x3) + (x1 - x3) * (y - y3);
            float u = (float)u_num / den;
            float v = (float)v_num / den;
            float w = 1.0f - u - v;
            if (u >= 0 && v >= 0 && w >= 0) {
                uint8 r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
                uint8 r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;
                uint8 r3 = (c3 >> 16) & 0xFF, g3 = (c3 >> 8) & 0xFF, b3 = c3 & 0xFF;
                uint8 r = (uint8)(u * r1 + v * r2 + w * r3);
                uint8 g = (uint8)(u * g1 + v * g2 + w * g3);
                uint8 b = (uint8)(u * b1 + v * b2 + w * b3);
                uint32 color = RGBA(r, g, b, 255);
                pixel(x, y, color);
            }
        }
    }
    
    for (int i = 0; i <= WAH_TRIANGLE_HALF_WIDTH; i++) {
        int x_left = x1 + i;
        int y_left = y1 - ((y1 - y2) * i) / WAH_TRIANGLE_HALF_WIDTH;
        pixel(x_left, y_left, COLOR_WAH_BORDER);
        int x_right = x3 - i;
        int y_right = y3 - ((y3 - y2) * i) / WAH_TRIANGLE_HALF_WIDTH;
        pixel(x_right, y_right, COLOR_WAH_BORDER);
    }
    for (int x = x1; x <= x3; x++) {
        pixel(x, y1, COLOR_WAH_BORDER);
    }

    int top_y = wheel_y + WAH_TRIANGLE_APEX_Y;
    int bottom_y = wheel_y + WAH_TRIANGLE_BASE_Y;
    int indicator_y = top_y + ((app_data->intensity - WAH_MIN_INTENSITY) * (bottom_y - top_y)) /
                      (WAH_MAX_INTENSITY - WAH_MIN_INTENSITY);
    int indicator_x = wheel_x + app_data->indicator_x;
    int indicator_half = 8;
    for (int dy = -indicator_half; dy <= indicator_half; dy++) {
        for (int dx = -indicator_half; dx <= indicator_half; dx++) {
            int px = indicator_x + dx;
            int py = indicator_y + dy;
            int dist_sq = dx * dx + dy * dy;
            if (dist_sq <= indicator_half * indicator_half) {
                int inner = indicator_half - 3;
                if (!(dist_sq <= inner * inner))
                    pixel(px, py, COLOR_WAH_SLIDER_THUMB);
            }
        }
    }
}

static void draw_color_wheel(int origin_x, int origin_y) {
    int wheel_x = origin_x + WAH_WHEEL_CX;
    int wheel_y = origin_y + WAH_WHEEL_CY;

    for (int angle = 0; angle < 360; angle += 1) {
        uint32 color = hue_to_rgb(angle);
        for (int radius = WAH_WHEEL_INNER_RADIUS; radius <= WAH_WHEEL_OUTER_RADIUS; radius++) {
            int x = wheel_x + (int)(cos_deg(angle) * (float)radius);
            int y = wheel_y + (int)(sin_deg(angle) * (float)radius);
            pixel(x, y, color);
        }
    }

    circle(wheel_x, wheel_y, WAH_WHEEL_OUTER_RADIUS + 1, COLOR_WAH_BORDER);
    circle(wheel_x, wheel_y, WAH_WHEEL_INNER_RADIUS - 1, COLOR_WAH_BORDER);
}

static void draw_wheel_thumb(int origin_x, int origin_y, WAHtercolourAppData* app_data) {
    int wheel_x = origin_x + WAH_WHEEL_CX;
    int wheel_y = origin_y + WAH_WHEEL_CY;

    int thumb_radius = (WAH_WHEEL_INNER_RADIUS + WAH_WHEEL_OUTER_RADIUS + 1) / 2;
    int tx = wheel_x + (int)(cos_deg((float)app_data->hue) * (float)thumb_radius);
    int ty = wheel_y + (int)(sin_deg((float)app_data->hue) * (float)thumb_radius);

    int inner = 4;
    int outer = 6;
    for (int dy = -outer; dy <= outer; dy++) {
        for (int dx = -outer; dx <= outer; dx++) {
            int d = dx*dx + dy*dy;
            if (d <= outer*outer && d >= inner*inner)
                pixel(tx + dx, ty + dy, COLOR_WAH_NEU_LIGHT);
        }
    }
    for (int dy = -outer-1; dy <= outer+1; dy++) {
        for (int dx = -outer-1; dx <= outer+1; dx++) {
            int d = dx*dx + dy*dy;
            int rim = outer+1;
            if (d <= rim*rim && d >= outer*outer)
                pixel(tx + dx, ty + dy, COLOR_WAH_BORDER);
        }
    }
}

static BOOL is_inside_canvas(int x, int y) {
    return x >= WAH_CANVAS_X && x < WAH_CANVAS_X + WAH_CANVAS_WIDTH &&
           y >= WAH_CANVAS_Y && y < WAH_CANVAS_Y + WAH_CANVAS_HEIGHT;
}

static BOOL is_inside_wheel(int x, int y) {
    int dx = x - WAH_WHEEL_CX;
    int dy = y - WAH_WHEEL_CY;
    int distance_sq = dx * dx + dy * dy;
    int outer_sq = WAH_WHEEL_OUTER_RADIUS * WAH_WHEEL_OUTER_RADIUS;
    int inner_sq = WAH_WHEEL_INNER_RADIUS * WAH_WHEEL_INNER_RADIUS;
    return distance_sq <= outer_sq && distance_sq >= inner_sq;
}

static BOOL is_inside_intensity_triangle(int x, int y) {
    int tx = x - WAH_WHEEL_CX;
    int ty = y - WAH_WHEEL_CY;

    int ax = 0, ay = WAH_TRIANGLE_APEX_Y;
    int bx = -WAH_TRIANGLE_HALF_WIDTH, by = WAH_TRIANGLE_BASE_Y;
    int cx = WAH_TRIANGLE_HALF_WIDTH, cy = WAH_TRIANGLE_BASE_Y;

    int d1 = (bx - ax) * (ty - ay) - (by - ay) * (tx - ax);
    int d2 = (cx - bx) * (ty - by) - (cy - by) * (tx - bx);
    int d3 = (ax - cx) * (ty - cy) - (ay - cy) * (tx - cx);

    BOOL has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    BOOL has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(has_neg && has_pos);
}

static BOOL is_inside_intensity_indicator(int x, int y) {
    if (!app_data_global) return FALSE;
    int top_y = WAH_WHEEL_CY + WAH_TRIANGLE_APEX_Y;
    int bottom_y = WAH_WHEEL_CY + WAH_TRIANGLE_BASE_Y;
    int indicator_y = top_y + ((app_data_global->intensity - WAH_MIN_INTENSITY) * (bottom_y - top_y)) /
                      (WAH_MAX_INTENSITY - WAH_MIN_INTENSITY);
    int indicator_x = WAH_WHEEL_CX + app_data_global->indicator_x;
    int dx = x - indicator_x;
    int dy = y - indicator_y;
    return (dx * dx + dy * dy <= 100);
}

static int intensity_from_triangle_y(int y) {
    int top_y = WAH_WHEEL_CY + WAH_TRIANGLE_APEX_Y;
    int bottom_y = WAH_WHEEL_CY + WAH_TRIANGLE_BASE_Y;
    if (y <= top_y) return WAH_MIN_INTENSITY;
    if (y >= bottom_y) return WAH_MAX_INTENSITY;

    int relative = y - top_y;
    int total = bottom_y - top_y;
    return WAH_MIN_INTENSITY + (relative * (WAH_MAX_INTENSITY - WAH_MIN_INTENSITY)) / total;
}

static int triangle_max_half_width_at_y(int y) {
    int top_y = WAH_WHEEL_CY + WAH_TRIANGLE_APEX_Y;
    int bottom_y = WAH_WHEEL_CY + WAH_TRIANGLE_BASE_Y;
    int ty = y - top_y;
    int total = bottom_y - top_y;
    if (ty <= 0) return 0;
    if (ty >= total) return WAH_TRIANGLE_HALF_WIDTH;
    return (ty * WAH_TRIANGLE_HALF_WIDTH) / total;
}

static void update_selected_color(WAHtercolourAppData* app_data) {
    int x1 = WAH_WHEEL_CX - WAH_TRIANGLE_HALF_WIDTH, y1 = WAH_WHEEL_CY + WAH_TRIANGLE_BASE_Y;
    int x2 = WAH_WHEEL_CX, y2 = WAH_WHEEL_CY + WAH_TRIANGLE_APEX_Y;
    int x3 = WAH_WHEEL_CX + WAH_TRIANGLE_HALF_WIDTH, y3 = WAH_WHEEL_CY + WAH_TRIANGLE_BASE_Y;

    int top_y = WAH_WHEEL_CY + WAH_TRIANGLE_APEX_Y;
    int bottom_y = WAH_WHEEL_CY + WAH_TRIANGLE_BASE_Y;
    int ind_y = top_y + ((app_data->intensity - WAH_MIN_INTENSITY) * (bottom_y - top_y)) /
                (WAH_MAX_INTENSITY - WAH_MIN_INTENSITY);
    int ind_x = WAH_WHEEL_CX + app_data->indicator_x;

    int den = (y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3);
    if (den == 0) return;
    float u = (float)((y2 - y3) * (ind_x - x3) + (x3 - x2) * (ind_y - y3)) / den;
    float v = (float)((y3 - y1) * (ind_x - x3) + (x1 - x3) * (ind_y - y3)) / den;
    float w = 1.0f - u - v;
    if (u < 0.0f) u = 0.0f;
    if (v < 0.0f) v = 0.0f;
    if (w < 0.0f) w = 0.0f;

    uint32 hue_color = hue_to_rgb(app_data->hue);
    uint8 hr = (hue_color >> 16) & 0xFF, hg = (hue_color >> 8) & 0xFF, hb = hue_color & 0xFF;

    uint8 r = (uint8)(u * 255 + v * 0 + w * hr);
    uint8 g = (uint8)(u * 255 + v * 0 + w * hg);
    uint8 b = (uint8)(u * 255 + v * 0 + w * hb);

    app_data->selected_color = RGBA(r, g, b, 255);
}

static void update_from_tri_pos(WAHtercolourAppData* app_data, int x, int y) {
    app_data->intensity = intensity_from_triangle_y(y);
    int half_w = triangle_max_half_width_at_y(y);
    int offset_x = x - WAH_WHEEL_CX;
    if (offset_x < -half_w) offset_x = -half_w;
    if (offset_x > half_w) offset_x = half_w;
    app_data->indicator_x = offset_x;
    update_selected_color(app_data);
}

static void set_color_from_wheel(WAHtercolourAppData* app_data, int x, int y) {
    int dx = x - WAH_WHEEL_CX;
    int dy = y - WAH_WHEEL_CY;
    float angle = atan2((float)dy, (float)dx);
    int hue = (int)((angle * 180.0f / 3.14159265f) + 360.0f) % 360;
    app_data->hue = hue;
    update_selected_color(app_data);
}

static inline int clamp_int(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static int slider_get_val_from_pos(int x, int min_value, int max_value) {
    int relative = x - WAH_SLIDER_X;
    relative = clamp_int(relative, 0, WAH_SLIDER_WIDTH - 1);
    return min_value + (relative * (max_value - min_value)) / (WAH_SLIDER_WIDTH - 1);
}

static int slider_thumb_x(int value, int min_value, int max_value) {
    int relative = (value - min_value) * (WAH_SLIDER_WIDTH - 1) / (max_value - min_value);
    return WAH_SLIDER_X + relative;
}

static void load_canvas(WAHtercolourAppData* app_data) {
    if (!app_data || !app_data->canvas_pixels || app_data->dialog_input_pos == 0) return;
    char normalized[MAX_FILENAME_LEN + 4];
    strcpy(normalized, app_data->dialog_input_buffer);
    for (int i = 0; normalized[i]; i++) if (normalized[i] >= 'a' && normalized[i] <= 'z') normalized[i] -= 32;
    const char* extension = strrchr(normalized, '.');
    BOOL user_provided_extension = (extension != NULL);
    if (!extension) { strcat(normalized, ".WAH"); extension = ".WAH"; }
    char path[MAX_FILENAME_LEN + 20] = "/";
    strcat(path, normalized);
    Bitmap* loaded = NULL;
    if (!user_provided_extension) {
        loaded = load_wah_image(path);
        if (!loaded) {
            char bmp_path[MAX_FILENAME_LEN + 20] = "/";
            strncat(bmp_path, normalized, strlen(normalized) - 4);
            strcat(bmp_path, ".BMP");
            loaded = load_bmp(bmp_path);
        }
    } else if (strcmp(extension, ".WAH") == 0 || strcmp(extension, ".RAW") == 0) loaded = load_wah_image(path);
    else if (strcmp(extension, ".BMP") == 0) loaded = load_bmp(path);
    if (loaded) {
        scale_bitmap_to_canvas(loaded, app_data->canvas_pixels);
        strcpy(app_data->current_filename, normalized);
        app_data->dirty = FALSE;
        free_bmp(loaded);
    }
}

static void save_canvas(WAHtercolourAppData* app_data) {
    if (!app_data || !app_data->canvas_pixels || app_data->dialog_input_pos == 0) return;
    char normalized[MAX_FILENAME_LEN + 4];
    strcpy(normalized, app_data->dialog_input_buffer);
    for (int i = 0; normalized[i]; i++) if (normalized[i] >= 'a' && normalized[i] <= 'z') normalized[i] -= 32;
    const char* extension = strrchr(normalized, '.');
    if (!extension) { strcat(normalized, ".WAH"); extension = ".WAH"; }
    else if (strcmp(extension, ".RAW") == 0) { strcpy((char*)extension, ".WAH"); extension = ".WAH"; }
    char path[MAX_FILENAME_LEN + 20] = "/";
    strcat(path, normalized);
    FAT_dirent* existing = fat_find_file(path);
    if (!existing) fat_create_file(path); else kfree(existing);
    Bitmap bmp; bmp.width = WAH_CANVAS_WIDTH; bmp.height = WAH_CANVAS_HEIGHT; bmp.data = app_data->canvas_pixels;
    int saved = (strcmp(extension, ".BMP") == 0) ? save_bmp(path, &bmp) : save_wah_image(path, &bmp);
    if (saved == 0) {
        strcpy(app_data->current_filename, normalized);
        app_data->dirty = FALSE;
    }
}

static void render_dialog(WAHtercolourAppData* app_data, char state) {
    if (!app_data || app_data->dialog_comp) return;

    char* title_text = (state == 'S') ? "SAVE IMAGE" : "OPEN IMAGE";
    PON_Comp* d = DIALOG(WAH_DIALOG_WIDTH, WAH_DIALOG_HEIGHT, title_text);
    d->draw = wahtercolour_dialog_draw_cb;
    d->x = (800 - WAH_DIALOG_WIDTH) / 2;
    d->y = (435 - WAH_DIALOG_HEIGHT) / 2;
    d->appdata = app_data;
    app_data->dialog_mode = (state == 'S') ? DIALOG_MODE_SAVE : DIALOG_MODE_LOAD;

    PON_Comp* header = PANEL(0, 0, WAH_DIALOG_WIDTH, 36, COLOR_WAH_DIALOG_ACCENT2);
    PON_child(d, header);

    PON_Comp* title = TEXT(20, 10, title_text, COLOR_WAH_TEXT);
    PON_child(d, title);

    PON_Comp* back = PANEL(0, 36, WAH_DIALOG_WIDTH, WAH_DIALOG_HEIGHT - 36, COLOR_WAH_DIALOG_ACCENT1);
    PON_child(d, back);

    PON_Comp* lbl = TEXT(20, 48, "filename:", COLOR_WAH_TEXT_DIM);
    PON_child(d, lbl);

    PON_Comp* in = TEXTFIELD(20, 78, WAH_DIALOG_WIDTH - 40, 26, MAX_FILENAME_LEN);
    in->on_key_press = dialog_text_input_key;
    if (app_data->current_filename[0]) {
        PON_TextField_d* id = (PON_TextField_d*)in->data;
        strcpy(id->buffer, app_data->current_filename);
    }
    in->focused = TRUE;
    PON_child(d, in);

    PON_Comp* btn_c = BUTTON(20, WAH_DIALOG_HEIGHT - 38, 80, 26, "cancel", on_dialog_cancel);
    btn_c->appdata = app_data;
    PON_child(d, btn_c);

    char* confirm_text = (state == 'S') ? "SAVE" : "OPEN";
    PON_Comp* btn_s = BUTTON(WAH_DIALOG_WIDTH - 100, WAH_DIALOG_HEIGHT - 38, 80, 26, confirm_text, (state == 'S') ? on_save_confirm : on_load_confirm);
    btn_s->appdata = app_data;
    PON_child(d, btn_s);

    app_data->dialog_comp = d;
}

static void open_save_dialog(WAHtercolourAppData* app_data) {
    render_dialog(app_data, 'S');
}

static void request_dialog_close(WAHtercolourAppData* app_data) {
    if (!app_data || !app_data->dialog_comp) return;
    if (app_data->dialog_comp_to_free) return;
    app_data->dialog_comp_to_free = app_data->dialog_comp;
    app_data->dialog_comp = NULL;
    app_data->dialog_mode = DIALOG_MODE_NONE;
}

static void process_pending_dialog_close(WAHtercolourAppData* app_data) {
    if (!app_data || !app_data->dialog_comp_to_free) return;
    PON_remove_child(app_data->root_comp, app_data->dialog_comp_to_free);
    PON_free(app_data->dialog_comp_to_free);
    app_data->dialog_comp_to_free = NULL;
}

static void open_load_dialog(WAHtercolourAppData* app_data) {
    render_dialog(app_data, 'O');
}

static void wahtercolour_dialog_draw_cb(PON_Comp* comp, int ax, int ay) {
    PON_Dialog_d* data = (PON_Dialog_d*)comp->data;

    rect(ax + 3, ay + 3, comp->width, comp->height, RGB(8, 8, 10));
    rect(ax, ay, comp->width, comp->height, g_pon_theme.bg);
    rect(ax, ay, comp->width, 1, g_pon_theme.border);
    rect(ax, ay + comp->height - 1, comp->width, 1, g_pon_theme.border);
    rect(ax, ay, 1, comp->height, g_pon_theme.border);
    rect(ax + comp->width - 1, ay, 1, comp->height, g_pon_theme.border);

    rect(ax + 1, ay + 1, comp->width - 2, 35, COLOR_WAH_DIALOG_ACCENT2);
    rect(ax + 1, ay + 1, 4, 35, COLOR_WAH_DIALOG_ACCENT1);

    if (data && data->title) {
        text(data->title, ax + 15, ay + 12, g_pon_theme.text, FONT_KALNIA, FALSE);
    }
}

static void qsave_canvas(WAHtercolourAppData* app_data) {
    if (!app_data || !app_data->current_filename[0]) return;
    strcpy(app_data->dialog_input_buffer, app_data->current_filename);
    app_data->dialog_input_pos = strlen(app_data->current_filename);
    save_canvas(app_data);
}

static void draw_brush_icon(int cx, int cy, WAHBrushType type, uint32 color) {
    int r = 5;
    if (type == BRUSH_ROUND) {
        for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) if (dx*dx + dy*dy <= r*r) pixel(cx + dx, cy + dy, color);
    } else if (type == BRUSH_SQUARE) {
        rect(cx - r, cy - r, r*2+1, r*2+1, color);
    } else {
        int pts[][2] = {{0,-5},{4,-3},{-3,-2},{6,0},{-5,1},{2,3},{-1,5},{5,4},{-4,-5},{3,-1}};
        for (int i = 0; i < 10; i++) {
            int px = cx + pts[i][0], py = cy + pts[i][1];
            pixel(px, py, color); pixel(px+1, py, color); pixel(px, py+1, color);
        }
    }
}

static void paint_pixel_at(WAHtercolourAppData* app_data, int px, int py) {
    if (px >= 0 && py >= 0 && px < WAH_CANVAS_WIDTH && py < WAH_CANVAS_HEIGHT)
        app_data->canvas_pixels[py * WAH_CANVAS_WIDTH + px] = app_data->selected_color;
}

static void blend_pixel_at(WAHtercolourAppData* app_data, int px, int py, uint32 src_color, int alpha) {
    if (px < 0 || py < 0 || px >= WAH_CANVAS_WIDTH || py >= WAH_CANVAS_HEIGHT || alpha <= 0) return;
    if (alpha >= 255) { app_data->canvas_pixels[py * WAH_CANVAS_WIDTH + px] = src_color; return; }
    uint32 dst = app_data->canvas_pixels[py * WAH_CANVAS_WIDTH + px];
    int sr = (src_color >> 16) & 0xFF, sg = (src_color >> 8) & 0xFF, sb = src_color & 0xFF;
    int dr = (dst >> 16) & 0xFF, dg = (dst >> 8) & 0xFF, db = dst & 0xFF;
    int inv = 255 - alpha;
    int or_ = (sr * alpha + dr * inv) >> 8, og = (sg * alpha + dg * inv) >> 8, ob = (sb * alpha + db * inv) >> 8;
    app_data->canvas_pixels[py * WAH_CANVAS_WIDTH + px] = RGBA(or_, og, ob, 255);
}

static void draw_brush_at(WAHtercolourAppData* app_data, int x, int y) {
    if (!app_data || !app_data->canvas_pixels) return;
    int local_x = x - WAH_CANVAS_X, local_y = y - WAH_CANVAS_Y;
    if (local_x < -app_data->brush_size || local_y < -app_data->brush_size ||
        local_x >= WAH_CANVAS_WIDTH + app_data->brush_size || local_y >= WAH_CANVAS_HEIGHT + app_data->brush_size) return;
    app_data->dirty = TRUE;
    int r = app_data->brush_size;
    if (app_data->brush_type == BRUSH_ROUND) {
        int r_sq = r * r;
        for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) if (dx*dx + dy*dy <= r_sq) paint_pixel_at(app_data, local_x + dx, local_y + dy);
    } else if (app_data->brush_type == BRUSH_SQUARE) {
        for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) paint_pixel_at(app_data, local_x + dx, local_y + dy);
    } else {
        int outer = r + r / 2 + 2, outer_sq = outer * outer, inner_sq = r * r;
        int br = (app_data->selected_color >> 16) & 0xFF, bg = (app_data->selected_color >> 8) & 0xFF, bb = app_data->selected_color & 0xFF;
        for (int dy = -outer; dy <= outer; dy++) {
            for (int dx = -outer; dx <= outer; dx++) {
                int dist_sq = dx*dx + dy*dy; if (dist_sq > outer_sq) continue;
                int cpx = local_x + dx, cpy = local_y + dy;
                unsigned int h = (unsigned int)(cpx * 2311u ^ cpy * 1741u ^ (unsigned int)(cpx * cpy) * 149u);
                h ^= h >> 13; h *= 0x9e3779b9u; h ^= h >> 11;
                unsigned int density = (dist_sq <= inner_sq) ? 174u : 56u; if ((h & 0xFFu) >= density) continue;
                int drop_r = ((h >> 8) & 1u) ? 2 : 1, base_alpha = (dist_sq <= inner_sq) ? 96 : 48;
                int alpha = base_alpha + (int)((h >> 16) & 0x3Fu), tint = (int)((h >> 9) & 0xFu) - 8;
                uint32 drop_color = RGBA(clamp_int(br + tint, 0, 255), clamp_int(bg + ((tint * 3) >> 2), 0, 255), clamp_int(bb - tint, 0, 255), 255);
                for (int fy = -drop_r; fy <= drop_r; fy++) for (int fx = -drop_r; fx <= drop_r; fx++) if (fx*fx + fy*fy <= drop_r*drop_r) blend_pixel_at(app_data, cpx + fx, cpy + fy, drop_color, alpha);
            }
        }
    }
}

static void stroke_to(WAHtercolourAppData* app_data, int x, int y) {
    const int dx = x - app_data->last_draw_x, dy = y - app_data->last_draw_y;
    int steps = (fabs(dx) > fabs(dy)) ? fabs(dx) : fabs(dy);
    if (steps == 0) { draw_brush_at(app_data, x, y); return; }
    for (int i = 0; i <= steps; i++) draw_brush_at(app_data, app_data->last_draw_x + dx * i / steps, app_data->last_draw_y + dy * i / steps);
}

// a lot of bs callbacks
static void canvas_draw_cb(PON_Comp* comp, int ax, int ay);
static void canvas_mouse_down_cb(PON_Comp* comp, int rx, int ry);
static void canvas_mouse_up_cb(PON_Comp* comp, int rx, int ry);
static void picker_draw_cb(PON_Comp* comp, int ax, int ay);
static void picker_mouse_down_cb(PON_Comp* comp, int rx, int ry);
static void picker_mouse_up_cb(PON_Comp* comp, int rx, int ry);
static void slider_draw_cb(PON_Comp* comp, int ax, int ay);
static void slider_mouse_down_cb(PON_Comp* comp, int rx, int ry);
static void slider_mouse_up_cb(PON_Comp* comp, int rx, int ry);
static void preview_draw_cb(PON_Comp* comp, int ax, int ay);
static void on_brush_click(PON_Comp* comp, int rx, int ry);
static void brush_btn_draw_cb(PON_Comp* comp, int ax, int ay);
static void on_clear_click(PON_Comp* comp, int rx, int ry);
static void on_save_click(PON_Comp* comp, int rx, int ry);
static void on_load_click(PON_Comp* comp, int rx, int ry);
static void on_save_confirm(PON_Comp* comp, int rx, int ry);
static void on_load_confirm(PON_Comp* comp, int rx, int ry);
static void on_dialog_cancel(PON_Comp* comp, int rx, int ry);

static void canvas_draw_cb(PON_Comp* comp, int ax, int ay) {
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)comp->appdata;
    neu_inset(ax - 4, ay - 4, WAH_CANVAS_WIDTH + 8, WAH_CANVAS_HEIGHT + 8, COLOR_WAH_CANVAS_BG);
    draw_canvas_pixels(app_data, ax - WAH_CANVAS_X, ay - WAH_CANVAS_Y);
}

static void canvas_mouse_down_cb(PON_Comp* comp, int rx, int ry) {
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)comp->appdata;
    app_data->drawing = TRUE;
    app_data->last_draw_x = rx + WAH_CANVAS_X;
    app_data->last_draw_y = ry + WAH_CANVAS_Y;
    draw_brush_at(app_data, app_data->last_draw_x, app_data->last_draw_y);
}

static void canvas_mouse_up_cb(PON_Comp* comp, int rx, int ry) {
    (void)rx; (void)ry;
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)comp->appdata;
    app_data->drawing = FALSE;
}

static void picker_draw_cb(PON_Comp* comp, int ax, int ay) {
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)comp->appdata;
    int origin_x = ax - (WAH_PANEL_X + 12);
    int origin_y = ay - 16;
    
    draw_color_wheel(origin_x, origin_y);
    draw_wheel_thumb(origin_x, origin_y, app_data);
    draw_intensity_triangle(origin_x, origin_y, app_data);
}

static void picker_mouse_down_cb(PON_Comp* comp, int rx, int ry) {
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)comp->appdata;
    int lx = rx + WAH_PANEL_X + 12;
    int ly = ry + 16;
    
    if (is_inside_wheel(lx, ly)) {
        app_data->wheel_dragging = TRUE;
        set_color_from_wheel(app_data, lx, ly);
    } else if (is_inside_intensity_indicator(lx, ly)) {
        app_data->indicator_dragging = TRUE;
        update_from_tri_pos(app_data, lx, ly);
    } else if (is_inside_intensity_triangle(lx, ly)) {
        app_data->triangle_dragging = TRUE;
        update_from_tri_pos(app_data, lx, ly);
    }
}

static void picker_mouse_up_cb(PON_Comp* comp, int rx, int ry) {
    (void)rx; (void)ry;
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)comp->appdata;
    app_data->wheel_dragging = FALSE;
    app_data->indicator_dragging = FALSE;
    app_data->triangle_dragging = FALSE;
}

static void slider_draw_cb(PON_Comp* comp, int ax, int ay) {
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)comp->appdata;
    int origin_x = ax - WAH_SLIDER_X;
    int origin_y = ay - WAH_SLIDER_Y;
    
    neu_inset(ax - 2, ay - 4, WAH_SLIDER_WIDTH + 4, WAH_SLIDER_HEIGHT + 8, COLOR_WAH_SLIDER_BG);

    int thumb_x_local = slider_thumb_x(app_data->brush_size, WAH_MIN_BRUSH_SIZE, WAH_MAX_BRUSH_SIZE);
    int fill_w = thumb_x_local - WAH_SLIDER_X;
    if (fill_w > 0)
        rect(ax, ay, fill_w, WAH_SLIDER_HEIGHT, COLOR_WAH_ACCENT_FILL);

    int tx = ax + fill_w - 7;
    int ty = ay - 6;
    neu_raised(tx, ty, 14, WAH_SLIDER_HEIGHT + 12);
    rect(tx + 5, ty + 4, 6, WAH_SLIDER_HEIGHT + 4, COLOR_WAH_ACCENT);
}

static void slider_mouse_down_cb(PON_Comp* comp, int rx, int ry) {
    (void)ry;
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)comp->appdata;
    app_data->slider_dragging = TRUE;
    app_data->brush_size = slider_get_val_from_pos(rx + WAH_SLIDER_X, WAH_MIN_BRUSH_SIZE, WAH_MAX_BRUSH_SIZE);
}

static void slider_mouse_up_cb(PON_Comp* comp, int rx, int ry) {
    (void)rx; (void)ry;
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)comp->appdata;
    app_data->slider_dragging = FALSE;
}

static void preview_draw_cb(PON_Comp* comp, int ax, int ay) {
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)comp->appdata;
    neu_inset(ax, ay, comp->width, comp->height, app_data->selected_color);
}

static void on_brush_click(PON_Comp* comp, int rx, int ry) {
    (void)rx; (void)ry;
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)comp->appdata;
    app_data->brush_type = (WAHBrushType)(uintptr_t)comp->data;
}

static void brush_btn_draw_cb(PON_Comp* comp, int ax, int ay) {
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)comp->appdata;
    WAHBrushType type = (WAHBrushType)(uintptr_t)comp->data;
    BOOL selected = (app_data->brush_type == type);
    
    if (selected)
        neu_inset(ax, ay, comp->width, comp->height, COLOR_WAH_SLIDER_BG);
    else
        neu_raised(ax, ay, comp->width, comp->height);

    uint32 icon_col = selected ? COLOR_WAH_ACCENT : COLOR_WAH_TEXT_DIM;
    draw_brush_icon(ax + comp->width / 2, ay + comp->height / 2, type, icon_col);
}

static void on_clear_click(PON_Comp* comp, int rx, int ry) {
    (void)rx; (void)ry;
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)comp->appdata;
    clear_canvas(app_data);
}

static void on_save_confirm(PON_Comp* comp, int rx, int ry) {
    (void)rx; (void)ry;
    PON_Comp* dialog = comp->parent;
    PON_Comp* input = dialog->children[4];
    PON_TextField_d* in_data = (PON_TextField_d*)input->data;
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)comp->appdata;
    
    strcpy(app_data->dialog_input_buffer, in_data->buffer);
    app_data->dialog_input_pos = strlen(in_data->buffer);
    save_canvas(app_data);
    
    request_dialog_close(app_data);
}

static void on_load_confirm(PON_Comp* comp, int rx, int ry) {
    (void)rx; (void)ry;
    PON_Comp* dialog = comp->parent;
    PON_Comp* input = dialog->children[4];
    PON_TextField_d* in_data = (PON_TextField_d*)input->data;
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)comp->appdata;

    strcpy(app_data->dialog_input_buffer, in_data->buffer);
    app_data->dialog_input_pos = strlen(in_data->buffer);
    load_canvas(app_data);

    request_dialog_close(app_data);
}

static void on_dialog_cancel(PON_Comp* comp, int rx, int ry) {
    (void)rx; (void)ry;
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)comp->appdata;
    request_dialog_close(app_data);
}

static void on_save_click(PON_Comp* comp, int rx, int ry) {
    (void)rx; (void)ry;
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)comp->appdata;
    open_save_dialog(app_data);
}

static void dialog_text_input_key(PON_Comp* comp, unsigned int key) {
    PON_TextField_d* data = (PON_TextField_d*)comp->data;
    if (!data || !data->buffer) return;

    if (key == 13 || key == 10) {
        PON_Comp* dialog = comp->parent;
        if (!dialog || dialog->type != COMP_DIALOG) return;

        WAHtercolourAppData* app_data = (WAHtercolourAppData*)dialog->appdata;
        if (!app_data) return;

        PON_Dialog_d* ddata = (PON_Dialog_d*)dialog->data;
        if (!ddata || !ddata->title) return;

        strcpy(app_data->dialog_input_buffer, data->buffer);
        app_data->dialog_input_pos = strlen(data->buffer);

        if (app_data->dialog_mode == DIALOG_MODE_SAVE) {
            save_canvas(app_data);
        } else if (app_data->dialog_mode == DIALOG_MODE_LOAD) {
            load_canvas(app_data);
        }

        request_dialog_close(app_data);
        return;
    }

    int len = strlen(data->buffer);
    if (key == '\b') {
        if (len > 0) data->buffer[len - 1] = '\0';
    } else if (len < data->max_len - 1 && key >= 32 && key <= 126) {
        data->buffer[len] = (char)key;
        data->buffer[len + 1] = '\0';
    }
}

static void on_load_click(PON_Comp* comp, int rx, int ry) {
    (void)rx; (void)ry;
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)comp->appdata;
    open_load_dialog(app_data);
}

static void wahtercolour_render(Window* win) {
    if (!win) return;

    Process* p = get_process(win->pid);
    if (!p || !p->data) return;

    WAHtercolourAppData* app_data = (WAHtercolourAppData*)p->data;
    int origin_x = win->x + WIN_BORDER;
    int origin_y = win->y + TITLEBAR_H + WIN_BORDER;

    PON_render(app_data->root_comp, origin_x, origin_y);

    if (app_data->current_filename[0] != '\0') {
        char status[MAX_FILENAME_LEN + 16] = "> ";
        strcat(status, app_data->current_filename);
        if (app_data->dirty) strcat(status, "*");
        text(status, origin_x + WAH_CANVAS_X, origin_y + WAH_CANVAS_Y + WAH_CANVAS_HEIGHT + 12, COLOR_WAH_TEXT_DIM, FONT_KALNIA, TRUE);
    }

    if (app_data->dialog_comp) {
        PON_render(app_data->dialog_comp, origin_x, origin_y);
    }
}

static void wahtercolour_on_mouse_down(Window* win, int x, int y) {
    if (!win) return;
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)p->data;

    if (app_data->dialog_comp) {
        handle_mouse(app_data->dialog_comp, 0, 0, x, y, PON_MOUSE_DOWN);
        process_pending_dialog_close(app_data);
        return;
    }

    handle_mouse(app_data->root_comp, 0, 0, x, y, PON_MOUSE_DOWN);
}

static void wahtercolour_on_mouse_move(Window* win, int x, int y) {
    if (!win) return;
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)p->data;

    if (app_data->dialog_comp) {
        handle_mouse(app_data->dialog_comp, 0, 0, x, y, PON_MOUSE_MOVE);
        process_pending_dialog_close(app_data);
        return;
    }

    handle_mouse(app_data->root_comp, 0, 0, x, y, PON_MOUSE_MOVE);

    if (app_data->wheel_dragging) {
        set_color_from_wheel(app_data, x, y);
    }
    if (app_data->indicator_dragging || app_data->triangle_dragging) {
        update_from_tri_pos(app_data, x, y);
    }
    if (app_data->slider_dragging) {
        app_data->brush_size = slider_get_val_from_pos(x, WAH_MIN_BRUSH_SIZE, WAH_MAX_BRUSH_SIZE);
    }

    if (app_data->drawing) {
        if (is_inside_canvas(x, y)) {
            stroke_to(app_data, x, y);
            app_data->last_draw_x = x;
            app_data->last_draw_y = y;
        }
    }
}

static void wahtercolour_on_mouse_up(Window* win, int x, int y) {
    if (!win) return;
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)p->data;

    if (app_data->dialog_comp) {
        handle_mouse(app_data->dialog_comp, 0, 0, x, y, PON_MOUSE_UP);
        process_pending_dialog_close(app_data);
        return;
    }

    handle_mouse(app_data->root_comp, 0, 0, x, y, PON_MOUSE_UP);

    app_data->drawing = FALSE;
    app_data->slider_dragging = FALSE;
    app_data->triangle_dragging = FALSE;
    app_data->indicator_dragging = FALSE;
    app_data->wheel_dragging = FALSE;
}

static void wahtercolour_on_key_press(Window* win, unsigned int c) {
    if (!win) return;
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    WAHtercolourAppData* app_data = (WAHtercolourAppData*)p->data;

    if (app_data->dialog_comp) {
        handle_key(app_data->dialog_comp, c);
        process_pending_dialog_close(app_data);
        return;
    }

    if (c == 19) { // ctrl+s
        if (app_data->current_filename[0]) {
            qsave_canvas(app_data);
        } else {
            open_save_dialog(app_data);
        }
        return;
    }

    if (c == 15) { // ctrl+o
        open_load_dialog(app_data);
        return;
    }
}

static void wahtercolour_cleanup(Window* win) {
    if (!win) return;

    Process* p = get_process(win->pid);
    if (p && p->data) {
        WAHtercolourAppData* app_data = (WAHtercolourAppData*)p->data;
        if (app_data) {
            if (app_data->canvas_pixels) {
                kfree(app_data->canvas_pixels);
            }
            if (app_data->root_comp) {
                PON_free(app_data->root_comp);
            }
            kfree(app_data);
            p->data = NULL;
        }
    }
}

void launch_wahtercolour(const char* filename) {
    Process* p = create_process("WAHtercolour");
    if (!p) return;

    WAHtercolourAppData* app_data = (WAHtercolourAppData*)kmalloc(sizeof(WAHtercolourAppData));
    if (!app_data) {
        cleanup_process(p->pid);
        return;
    }

    app_data->canvas_pixels = (uint32*)kmalloc(WAH_CANVAS_WIDTH * WAH_CANVAS_HEIGHT * sizeof(uint32));
    if (!app_data->canvas_pixels) {
        kfree(app_data);
        cleanup_process(p->pid);
        return;
    }

    for (int i = 0; i < WAH_CANVAS_WIDTH * WAH_CANVAS_HEIGHT; i++) {
        app_data->canvas_pixels[i] = COLOR_WAH_CANVAS_BG;
    }

    app_data->hue = 280;
    app_data->intensity = 42;
    app_data->brush_size = 6;
    app_data->selected_color = apply_intensity(hue_to_rgb(app_data->hue), app_data->intensity);
    app_data->drawing = FALSE;
    app_data->slider_dragging = FALSE;
    app_data->triangle_dragging = FALSE;
    app_data->indicator_dragging = FALSE;
    app_data->wheel_dragging = FALSE;
    app_data->last_draw_x = 0;
    app_data->last_draw_y = 0;
    app_data->indicator_x = 0;
    app_data->brush_type = BRUSH_ROUND;
    app_data->dialog_comp = NULL;
    app_data->dialog_comp_to_free = NULL;
    app_data->dialog_mode = DIALOG_MODE_NONE;
    app_data->dirty = FALSE;
    memset(app_data->current_filename, 0, MAX_FILENAME_LEN);

    if (filename) {
        Bitmap* loaded = NULL;
        const char* ext = strrchr(filename, '.');
        if (ext) {
            if (strcmp(ext, ".WAH") == 0 || strcmp(ext, ".wah") == 0 || strcmp(ext, ".RAW") == 0 || strcmp(ext, ".raw") == 0) {
                loaded = load_wah_image((char*)filename);
            } else if (strcmp(ext, ".BMP") == 0 || strcmp(ext, ".bmp") == 0) {
                loaded = load_bmp((char*)filename);
            }
        }
        if (loaded) {
            scale_bitmap_to_canvas(loaded, app_data->canvas_pixels);
            const char* last_slash = strrchr(filename, '/');
            strcpy(app_data->current_filename, last_slash ? last_slash + 1 : filename);
            free_bmp(loaded);
        }
    }

    PON_Comp* root = PANEL(0, 0, 795, 430, COLOR_WAH_BACKGROUND);
    root->appdata = app_data;
    app_data->root_comp = root;

    PON_Comp* sidebar = PANEL(WAH_PANEL_X, 0, WAH_PANEL_WIDTH, 430, COLOR_WAH_PANEL);
    PON_child(root, sidebar);

    PON_Comp* canvas = PON_summon(COMP_GENERIC, WAH_CANVAS_X, WAH_CANVAS_Y, WAH_CANVAS_WIDTH, WAH_CANVAS_HEIGHT);
    canvas->draw = canvas_draw_cb;
    canvas->on_mouse_down = canvas_mouse_down_cb;
    canvas->on_mouse_up = canvas_mouse_up_cb;
    canvas->appdata = app_data;
    PON_child(root, canvas);

    PON_Comp* btn_save = BUTTON(WAH_CANVAS_X + WAH_CANVAS_WIDTH - 124, WAH_CANVAS_Y + WAH_CANVAS_HEIGHT + 6, 60, 28, "SAVE", on_save_click);
    btn_save->appdata = app_data;
    PON_child(root, btn_save);

    PON_Comp* btn_load = BUTTON(WAH_CANVAS_X + WAH_CANVAS_WIDTH - 56, WAH_CANVAS_Y + WAH_CANVAS_HEIGHT + 6, 60, 28, "LOAD", on_load_click);
    btn_load->appdata = app_data;
    PON_child(root, btn_load);

    PON_Comp* file_text = TEXT(WAH_CANVAS_X, WAH_CANVAS_Y + WAH_CANVAS_HEIGHT + 12, "", COLOR_WAH_TEXT_DIM);
    file_text->appdata = app_data;
    file_text->draw = NULL;
    PON_child(root, file_text);

    PON_Comp* wcard = PANEL(WAH_PANEL_X + 12, 16, WAH_PANEL_WIDTH - 24, WAH_WHEEL_CY + WAH_TRIANGLE_BASE_Y + 32, COLOR_WAH_PANEL);
    PON_child(root, wcard);

    PON_Comp* picker = PON_summon(COMP_GENERIC, 0, 0, wcard->width, wcard->height);
    picker->draw = picker_draw_cb;
    picker->on_mouse_down = picker_mouse_down_cb;
    picker->on_mouse_up = picker_mouse_up_cb;
    picker->appdata = app_data;
    PON_child(wcard, picker);

    PON_Comp* btn_clear = BUTTON(wcard->width - 64, 6, 60, 28, "CLEAR", on_clear_click);
    btn_clear->appdata = app_data;
    PON_child(wcard, btn_clear);

    PON_Comp* bcard = PANEL(WAH_PANEL_X + WAH_BRUSH_CARD_PAD, wcard->y + wcard->height + 10, WAH_PANEL_WIDTH - 2 * WAH_BRUSH_CARD_PAD, WAH_BRUSH_BTN_H + 38, COLOR_WAH_PANEL);
    PON_child(root, bcard);
    PON_child(bcard, TEXT(8, 8, "brush", COLOR_WAH_TEXT_DIM));

    int btn_inner_w = bcard->width - 2 * WAH_BRUSH_BTN_GAP;
    int btn_w = (btn_inner_w - 2 * WAH_BRUSH_BTN_GAP) / 3;
    for (int i = 0; i < 3; i++) {
        PON_Comp* bb = PON_summon(COMP_GENERIC, WAH_BRUSH_BTN_GAP + i * (btn_w + WAH_BRUSH_BTN_GAP), bcard->height - WAH_BRUSH_BTN_H - 6, btn_w, WAH_BRUSH_BTN_H);
        bb->draw = brush_btn_draw_cb;
        bb->on_click = on_brush_click;
        bb->data = (void*)(uintptr_t)i;
        bb->appdata = app_data;
        PON_child(bcard, bb);
    }

    PON_Comp* scard = PANEL(WAH_PANEL_X + 12, bcard->y + bcard->height + 10, WAH_PANEL_WIDTH - 24, 64, COLOR_WAH_PANEL);
    PON_child(root, scard);
    PON_child(scard, TEXT(8, 8, "size", COLOR_WAH_TEXT_DIM));
    
    PON_Comp* slider = PON_summon(COMP_GENERIC, WAH_SLIDER_X - (WAH_PANEL_X + 12), WAH_SLIDER_Y - scard->y, WAH_SLIDER_WIDTH, WAH_SLIDER_HEIGHT);
    slider->draw = slider_draw_cb;
    slider->on_mouse_down = slider_mouse_down_cb;
    slider->on_mouse_up = slider_mouse_up_cb;
    slider->appdata = app_data;
    PON_child(scard, slider);

    PON_Comp* pcard = PANEL(WAH_PANEL_X + 12, scard->y + scard->height + 14, WAH_PANEL_WIDTH - 24, 80, COLOR_WAH_PANEL);
    PON_child(root, pcard);
    PON_child(pcard, TEXT(10, 6, "PREVIEW", COLOR_WAH_TEXT));
    
    PON_Comp* preview = PON_summon(COMP_GENERIC, 8, 26, pcard->width - 16, pcard->height - 34);
    preview->draw = preview_draw_cb;
    preview->appdata = app_data;
    PON_child(pcard, preview);

    p->data = app_data;
    app_data_global = app_data;

    uint8 r = 140, g = 90, b = 200;
    sample_icn("SYSTEM/wah-ic.bmp", &r, &g, &b);

    Window* w = window_r(p->pid, "WAHtercolour", -1, -1, 800, 455, r, g, b);
    if (!w) {
        kfree(app_data->canvas_pixels);
        PON_free(app_data->root_comp);
        kfree(app_data);
        cleanup_process(p->pid);
        return;
    }

    w->content_renderer = wahtercolour_render;
    w->on_close = wahtercolour_cleanup;
    w->on_mouse_down = wahtercolour_on_mouse_down;
    w->on_mouse_up = wahtercolour_on_mouse_up;
    w->on_mouse_move = wahtercolour_on_mouse_move;
    w->on_key_press = wahtercolour_on_key_press;
}