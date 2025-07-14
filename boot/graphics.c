#include "graphics.h"

const int size = 12;
const int ofs = size / 2;

int abs(int value) {
    return (value < 0) ? -value : value;
}

int rgb(int r, int g, int b)
{
    r = r & 0xFF;
    g = g & 0xFF;
    b = b & 0xFF;

    return (r << 16) | (g << 8) | b;
}

int rgba(int r, int g, int b, int a)
{
    r = r & 0xFF;
    g = g & 0xFF;
    b = b & 0xFF;
    a = a & 0xFF;

    return (r << 16) | (g << 8) | b;
}

void DrawPixel(int x, int y, int r, int g, int b)
{
    VBEInfoBlock *VBE = (VBEInfoBlock *)VBEInfoAddress;
    unsigned int *buffer = (unsigned int *)ScreenBufferAddress;

    int index = y * VBE->x_resolution + x;
    *(buffer + index) = rgb(r, g, b);
}

void DrawPixelAlpha(int x, int y, int r, int g, int b, int a)
{
    if (a <= 0) return;
    
    if (a >= 255) {
        DrawPixel(x, y, r, g, b);
        return;
    }
    
    VBEInfoBlock *VBE = (VBEInfoBlock *)VBEInfoAddress;
    unsigned int *buffer = (unsigned int *)ScreenBufferAddress;
    
    int index = y * VBE->x_resolution + x;
    unsigned int existing_color = *(buffer + index);
    
    int existing_r = (existing_color >> 16) & 0xFF;
    int existing_g = (existing_color >> 8) & 0xFF;
    int existing_b = existing_color & 0xFF;
    
    int alpha_factor = a;
    int inverse_alpha = 255 - alpha_factor;
    
    int new_r = ((r * alpha_factor) + (existing_r * inverse_alpha)) / 255;
    int new_g = ((g * alpha_factor) + (existing_g * inverse_alpha)) / 255;
    int new_b = ((b * alpha_factor) + (existing_b * inverse_alpha)) / 255;
    *(buffer + index) = rgb(new_r, new_g, new_b);
}

void Clear(int r, int g, int b)
{
    VBEInfoBlock *VBE = (VBEInfoBlock *)VBEInfoAddress;
    unsigned int color = rgb(r, g, b);
    unsigned int *buffer = (unsigned int *)ScreenBufferAddress;

    for (int y = 0; y < VBE->y_resolution; y++)
    {
        for (int x = 0; x < VBE->x_resolution; x++)
        {
            *(buffer + y * VBE->x_resolution + x) = color;
        }
    }
}

void DrawRect(int x, int y, int width, int height, int r, int g, int b)
{
    VBEInfoBlock *VBE = (VBEInfoBlock *)VBEInfoAddress;
    unsigned int *buffer = (unsigned int *)ScreenBufferAddress;
    unsigned int color = rgb(r, g, b);

    for (int j = y; j < y + height; j++)
    {
        if (j >= 0 && j < VBE->y_resolution)
        {
            for (int i = x; i < x + width; i++)
            {
                if (i >= 0 && i < VBE->x_resolution)
                {
                    *(buffer + j * VBE->x_resolution + i) = color;
                }
            }
        }
    }
}

void DrawRectAlpha(int x, int y, int width, int height, int r, int g, int b, int a)
{
    if (a <= 0) return;
    
    if (a >= 255) {
        DrawRect(x, y, width, height, r, g, b);
        return;
    }
    
    VBEInfoBlock *VBE = (VBEInfoBlock *)VBEInfoAddress;
    
    for (int j = y; j < y + height; j++)
    {
        if (j >= 0 && j < VBE->y_resolution)
        {
            for (int i = x; i < x + width; i++)
            {
                if (i >= 0 && i < VBE->x_resolution)
                {
                    DrawPixelAlpha(i, j, r, g, b, a);
                }
            }
        }
    }
}

void DrawRectGradient(int x, int y, int width, int height, int r1, int g1, int b1, int r2, int g2, int b2) {
    VBEInfoBlock* VBE = (VBEInfoBlock*) VBEInfoAddress;
    int original_x = x;
    
    for (int curr_y = 0; curr_y < height; curr_y++) {
        int curr_r = r1 + ((curr_y * (r2 - r1)) / height);
        int curr_g = g1 + ((curr_y * (g2 - g1)) / height);
        int curr_b = b1 + ((curr_y * (b2 - b1)) / height);
        
        for (int curr_x = 0; curr_x < width; curr_x++) {
            DrawPixel(original_x + curr_x, y + curr_y, curr_r, curr_g, curr_b);
        }
    }
}

void ClearScreenGradient(int startR, int startG, int startB, int endR, int endG, int endB) {
    VBEInfoBlock* VBE = (VBEInfoBlock*) VBEInfoAddress;
    int screenWidth = VBE->x_resolution;
    int screenHeight = VBE->y_resolution;
    
    for (int y = 0; y < screenHeight; y++) {
        int r = startR + ((y * (endR - startR)) / screenHeight);
        int g = startG + ((y * (endG - startG)) / screenHeight);
        int b = startB + ((y * (endB - startB)) / screenHeight);
        
        for (int x = 0; x < screenWidth; x++) {
            DrawPixel(x, y, r, g, b);
        }
    }
}

void DrawCircle(int x, int y, int radius, int r, int g, int b)
{
    int rr = radius * radius;
    unsigned int color = rgb(r, g, b);

    VBEInfoBlock *VBE = (VBEInfoBlock *)VBEInfoAddress;
    unsigned int *buffer = (unsigned int *)ScreenBufferAddress;

    for (int j = -radius; j <= radius; j++)
    {
        for (int i = -radius; i <= radius; i++)
        {
            if (i * i + j * j <= rr)
            {
                int px = x + i;
                int py = y + j;

                if (px >= 0 && px < VBE->x_resolution && py >= 0 && py < VBE->y_resolution)
                {
                    *(buffer + py * VBE->x_resolution + px) = color;
                }
            }
        }
    }
}

void DrawCircleAlpha(int x, int y, int radius, int r, int g, int b, int a)
{
    if (a <= 0) return;
    
    if (a >= 255) {
        DrawCircle(x, y, radius, r, g, b);
        return;
    }
    
    int rr = radius * radius;
    VBEInfoBlock *VBE = (VBEInfoBlock *)VBEInfoAddress;

    for (int j = -radius; j <= radius; j++)
    {
        for (int i = -radius; i <= radius; i++)
        {
            if (i * i + j * j <= rr)
            {
                int px = x + i;
                int py = y + j;

                if (px >= 0 && px < VBE->x_resolution && py >= 0 && py < VBE->y_resolution)
                {
                    DrawPixelAlpha(px, py, r, g, b, a);
                }
            }
        }
    }
}

void DrawCharacter(int (*f)(int, int), int font_width, int font_height, char character, int x, int y, int r, int g, int b)
{
    for (int j = 0; j < font_height; j++)
    {
        unsigned int row = (*f)((int)(character), j);
        int shift = font_width - 1;
        int bit_val = 0;

        for (int i = 0; i < font_width; i++)
        {
            bit_val = (row >> shift) & 0b001;
            if (bit_val == 1)
                DrawPixel(x + i, y + j, r, g, b);

            shift -= 1;
        }
    }
}

void DrawText(int (*f)(int, int), int font_width, int font_height, char *string, int x, int y, int r, int g, int b)
{
    int i = 0, j = 0;

    for (int k = 0; *(string + k) != 0; k++)
    {
        if (*(string + k) != '\n')
            DrawCharacter(f, font_width, font_height, *(string + k), x + i, y + j, r, g, b);

        i += font_width;

        if (*(string + k) == '\n')
        {
            i = 0;
            j += font_height;
        }
    }
}

void MouseGraphics(int x, int y, int r, int g, int b)
{
    for (int i = -12; i <= 12; i++) {
        DrawPixel(x + i, y, 165, 75, 160);
        DrawPixel(x, y + i, 165, 75, 160);
    }

    DrawRectAlpha(x - ofs, y - ofs, size, size, r, g, b, 120);
    DrawRect(x - ofs / 2, y - ofs / 2, size / 2, size / 2, 35, 25, 55);
    DrawRect(x - ofs / 3, y - ofs / 3, size / 3, size / 3, r - 135, g - 125, b - 155);

    // DrawIconBrand(x - ofs, y - ofs, 1, 1, r, g, b, 2);
}

// single-colour icons (use icon_library.h)
void DrawIconBrand(int x, int y, int width, int height, int r, int g, int b, int icon_index) {
    #include "images/icon_library.h"

    int *icon_arr = icon[icon_index];

    int icon_height = sizeof(icon_arr[0]) * 4;
    int icon_width = sizeof(icon_arr[0]) * 7 - 1;

    for (int j = 0; j < icon_height; j++)
    {
        unsigned int row = icon_arr[j];
        for (int i = 0; i < icon_width; i++)
        {

            int bit_val = (row >> (icon_width - 1 - i)) & 1;
            if (bit_val == 1) DrawRect(x + i * width, y + j * height, width, height, r, g, b);
        }
    }
}


void Refresh() {
    VBEInfoBlock *VBE = (VBEInfoBlock *)VBEInfoAddress;
    unsigned int *buffer = (unsigned int *)ScreenBufferAddress;
    unsigned char *screen = (unsigned char *)VBE->screen_ptr;
    int index;

    if (VBE->screen_ptr == 0){return;}

    if (VBE->x_resolution <= 0 || VBE->y_resolution <= 0){return;}

    for (int y = 0; y < VBE->y_resolution; y++)
    {
        for (int x = 0; x < VBE->x_resolution; x++)
        {
            index = y * VBE->x_resolution + x;

            if (index >= VBE->x_resolution * VBE->y_resolution){ return; }

            unsigned int color = *(buffer + index);
            unsigned char *pixel = screen + (index * 3);

            *pixel = color & 0xFF;
            *(pixel + 1) = (color >> 8) & 0xFF;
            *(pixel + 2) = (color >> 16) & 0xFF;
        }
    }
}