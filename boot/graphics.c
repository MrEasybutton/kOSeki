#include "graphics.h"

const int size = 8;
const int ofs = size / 2;

int rgb(int r, int g, int b)
{
    r = (r >> 3) & 0x1F;
    g = (g >> 2) & 0x3F;
    b = (b >> 3) & 0x1F;

    return (r << 11) | (g << 5) | b;
}

// This draws a pixel onscreen
void DrawPixel(int x, int y, int r, int g, int b)
{
    VBEInfoBlock *VBE = (VBEInfoBlock *)VBEInfoAddress;
    unsigned short *buffer = (unsigned short *)ScreenBufferAddress;

    int index = y * VBE->x_resolution + x;
    *(buffer + index) = rgb(r, g, b);
}

// This clears screen with a custom colour
void Clear(int r, int g, int b)
{
    VBEInfoBlock *VBE = (VBEInfoBlock *)VBEInfoAddress;
    unsigned short color = rgb(r, g, b);
    unsigned short *buffer = (unsigned short *)ScreenBufferAddress;

    for (int y = 0; y < VBE->y_resolution; y++)
    {
        for (int x = 0; x < VBE->x_resolution; x++)
        {
            *(buffer + y * VBE->x_resolution + x) = color;
        }
    }
}

// This draws a basic rectangle
void DrawRect(int x, int y, int width, int height, int r, int g, int b)
{
    VBEInfoBlock *VBE = (VBEInfoBlock *)VBEInfoAddress;
    unsigned short *buffer = (unsigned short *)ScreenBufferAddress;
    unsigned short color = rgb(r, g, b);

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

// This draws a basic circle
void DrawCircle(int x, int y, int radius, int r, int g, int b)
{
    int rr = radius * radius;
    unsigned short color = rgb(r, g, b);

    VBEInfoBlock *VBE = (VBEInfoBlock *)VBEInfoAddress;
    unsigned short *buffer = (unsigned short *)ScreenBufferAddress;

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

// This draws a character corresponding to the font array
void DrawCharacter(int (*f)(int, int), int font_width, int font_height, char character, int x, int y, int r, int g, int b)
{
    for (int j = 0; j < font_height; j++)
    {
        unsigned int row = (*f)((int)(character), j);
        int shift = font_width - 1;
        int bit_val = 0;

        for (int i = 0; i < font_width; i++)
        {
            bit_val = (row >> shift) & 0b00000000000000000000000000000001;
            if (bit_val == 1)
                DrawPixel(x + i, y + j, r, g, b);

            shift -= 1;
        }
    }
}

// This draws a string using DrawCharacter
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

// Mouse
void MouseGraphics(int x, int y, int r, int g, int b)
{
    for (int i = -10; i <= 10; i++)
    {
        DrawPixel(x + i, y, 165, 75, 160);
        DrawPixel(x, y + i, 165, 75, 160);
    }

    DrawRect(x - ofs, y - ofs, size, size, r, g, b);
    DrawRect(x - ofs / 2, y - ofs / 2, size / 2, size / 2, 235, 155, 235);
}

// This will draw a single-colour icon from the icon array. Use icon_index to select an indexed binary icon from the array (0 is kOSeki pebble logo)
void DrawIconBrand(int x, int y, int width, int height, int r, int g, int b, int icon_index)
{
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

            if (bit_val == 1)
            {
                DrawRect(x + i * width, y + j * height, width, height, r, g, b);
            }
        }
    }
}

// Update screen buffer
void Refresh()
{
    VBEInfoBlock *VBE = (VBEInfoBlock *)VBEInfoAddress;
    unsigned short *buffer = (unsigned short *)ScreenBufferAddress;
    int index;

    if (VBE->screen_ptr == 0){return;}

    if (VBE->x_resolution <= 0 || VBE->y_resolution <= 0){return;}

    for (int y = 0; y < VBE->y_resolution; y++)
    {
        for (int x = 0; x < VBE->x_resolution; x++)
        {
            index = y * VBE->x_resolution + x;

            if (index >= VBE->x_resolution * VBE->y_resolution){return;}

            *((unsigned short *)VBE->screen_ptr + index) = *(buffer + index);
        }
    }
}
