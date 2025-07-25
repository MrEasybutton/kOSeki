#ifndef GRAPHICS_H
#define GRAPHICS_H

typedef struct VBEInfoBlockStruct {
    unsigned short mode_attribute;
    unsigned char win_a_attribute;
    unsigned char win_b_attribute;
    unsigned short win_granuality;
    unsigned short win_size;
    unsigned short win_a_segment;
    unsigned short win_b_segment;
    unsigned int win_func_ptr;
    unsigned short bytes_per_scan_line;
    unsigned short x_resolution;
    unsigned short y_resolution;
    unsigned char char_x_size;
    unsigned char char_y_size;
    unsigned char number_of_planes;
    unsigned char bits_per_pixel;
    unsigned char number_of_banks;
    unsigned char memory_model;
    unsigned char bank_size;
    unsigned char number_of_image_pages;
    unsigned char b_reserved;
    unsigned char red_mask_size;
    unsigned char red_field_position;
    unsigned char green_mask_size;
    unsigned char green_field_position;
    unsigned char blue_mask_size;
    unsigned char blue_field_position;
    unsigned char reserved_mask_size;
    unsigned char reserved_field_position;
    unsigned char direct_color_info;
    unsigned int screen_ptr;
} VBEInfoBlock;

#define VBEInfoAddress 0x8000
#define ScreenBufferAddress 0xffff0

extern const int font_font_width;
extern const int font_font_height;

int getFontCharacter(int index, int y);
int abs(int value);
int rgb(int r, int g, int b);
int rgba(int r, int g, int b, int a);
void DrawPixel(int x, int y, int r, int g, int b);
void DrawPixelAlpha(int x, int y, int r, int g, int b, int a);
void Clear(int r, int g, int b);
void DrawRect(int x, int y, int width, int height, int r, int g, int b);
void DrawRectAlpha(int x, int y, int width, int height, int r, int g, int b, int a);
void DrawRectGradient(int x, int y, int width, int height, int r1, int g1, int b1, int r2, int g2, int b2);
void ClearScreenGradient(int startR, int startG, int startB, int endR, int endG, int endB);
void DrawCircle(int x, int y, int radius, int r, int g, int b);
void DrawCircleAlpha(int x, int y, int radius, int r, int g, int b, int a);
void DrawCharacter(int (*f)(int, int), int font_width, int font_height, char character, int x, int y, int r, int g, int b);
void DrawText(int (*f)(int, int), int font_width, int font_height, char* string, int x, int y, int r, int g, int b);
void DrawGem(int x, int y, int width, int height, int r, int g, int b, int a);
void MouseGraphics(int x, int y, int r, int g, int b);
void DrawIconBrand(int x, int y, int width, int height, int r, int g, int b, int icon_index);
void Refresh();
int bg_r1, bg_g1, bg_b1, bg_r2, bg_g2, bg_b2;

int DrawSlider(int x, int y, int width, int height, int min_value, int max_value, int* current_value, int r, int g, int b, int process_inst);

#endif
