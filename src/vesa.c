#include "vesa.h"
#include "bios32.h"
#include "console.h"
#include "ports.h"
#include "serial.h"
#include "isr.h"
#include "string.h"
#include "types.h"
#include "kheap.h"

VBE20_INFOBLOCK g_vbe_infoblock;
VBE20_MODEINFOBLOCK g_vbe_modeinfoblock;

int g_selected_mode = -1;
uint32 g_width = 0, g_height = 0;
uint32 *g_vbe_buffer = NULL;
uint32 *g_back_buffer = NULL;

uint32 vbe_get_width() { return g_width; }
uint32 vbe_get_height() { return g_height; }

#define VBE_FAR_TO_LINEAR(fp) ((((fp) >> 16) << 4) + ((fp) & 0xFFFF))

int rq_info() {
    printf("1");
    REGISTERS16 in = {0}, out = {0};
    in.ax = 0x4F00;
    printf("2");
    in.di = BIOS_CONVENTIONAL_MEMORY;
    
    //request 2.0+ spec info
    memcpy((void *)BIOS_CONVENTIONAL_MEMORY, "VBE2", 4);
    
    int86(0x10, &in, &out);
    printf("3");
    memcpy(&g_vbe_infoblock, (void *)BIOS_CONVENTIONAL_MEMORY, sizeof(VBE20_INFOBLOCK));
    return (out.ax == 0x4F);
}

void vbe_rq_info(uint16 mode, VBE20_MODEINFOBLOCK *mode_info) {
    REGISTERS16 in = {0}, out = {0};
    in.ax = 0x4F01;
    in.cx = mode;
    in.di = BIOS_CONVENTIONAL_MEMORY + 1024;
    int86(0x10, &in, &out);
    memcpy(mode_info, (void *)BIOS_CONVENTIONAL_MEMORY + 1024, sizeof(VBE20_MODEINFOBLOCK));
}

void vbe_setmode(uint32 mode) {
    REGISTERS16 in = {0}, out = {0};
    in.ax = 0x4F02;
    in.bx = mode;
    int86(0x10, &in, &out);
}

uint32 vbe_srcmodes(uint32 width, uint32 height, uint32 bpp) {
    if (g_vbe_infoblock.VideoModePtr == 0) return -1;
    
    uint32 mode_list_ptr = VBE_FAR_TO_LINEAR(g_vbe_infoblock.VideoModePtr);
    if (mode_list_ptr == 0) return -1;

    // copy modelist to heap
    uint16 *orig_mode_list = (uint16 *)mode_list_ptr;
    int mode_count = 0;
    while (orig_mode_list[mode_count] != 0xffff && mode_count < 1024) {
        mode_count++;
    }

    uint16 *safe_mode_list = (uint16 *)kmalloc((mode_count + 1) * sizeof(uint16));
    if (!safe_mode_list) {
        kprint("[VBE] failed to allocate memory for safe list\n");
        return -1;
    }

    memcpy(safe_mode_list, orig_mode_list, mode_count * sizeof(uint16));
    safe_mode_list[mode_count] = 0xffff; // terminate list

    uint16 *current_mode = safe_mode_list;
    uint32 found_mode = -1;

    while (*current_mode != 0xffff) {
        uint16 mode = *current_mode++;
        vbe_rq_info(mode, &g_vbe_modeinfoblock);
        if (g_vbe_modeinfoblock.XResolution == width && 
            g_vbe_modeinfoblock.YResolution == height && 
            g_vbe_modeinfoblock.BitsPerPixel == bpp) {
            found_mode = mode;
            break;
        }
    }

    kfree(safe_mode_list);
    return found_mode;
}

uint32 blend_pixel(uint32 fg_color, uint32 bg_color) {
    uint8 fg_alpha = (fg_color >> 24) & 0xFF;
    uint8 fg_red = (fg_color >> 16) & 0xFF;
    uint8 fg_green = (fg_color >> 8) & 0xFF;
    uint8 fg_blue = fg_color & 0xFF;

    uint8 bg_red = (bg_color >> 16) & 0xFF;
    uint8 bg_green = (bg_color >> 8) & 0xFF;
    uint8 bg_blue = bg_color & 0xFF;

    uint8 out_red = (fg_red * fg_alpha + bg_red * (255 - fg_alpha)) / 255;
    uint8 out_green = (fg_green * fg_alpha + bg_green * (255 - fg_alpha)) / 255;
    uint8 out_blue = (fg_blue * fg_alpha + bg_blue * (255 - fg_alpha)) / 255;

    return RGBA(out_red, out_green, out_blue, 255);
}

//direct macro (internal only)
#define FF_PUTPIXEL(x, y, color) g_back_buffer[(y) * g_width + (x)] = (color)
#define FF_GETPIXEL(x, y) g_back_buffer[(y) * g_width + (x)]

void pixel(int x, int y, int color) {
    if ((unsigned)x >= g_width || (unsigned)y >= g_height) return; // just unsign

    uint8 alpha = (color >> 24) & 0xFF;
    uint32 i = y * g_width + x;

    if (alpha == 255) {
        g_back_buffer[i] = color;
    } else if (alpha > 0) {
        uint32 bg_color = g_back_buffer[i];
        g_back_buffer[i] = blend_pixel(color, bg_color);
    }
}

uint32 getpixel(int x, int y) {
    if ((unsigned)x >= g_width || (unsigned)y >= g_height) return 0;
    return g_back_buffer[y * g_width + x];
}

void swapbuf(void) {
    memcpy(g_vbe_buffer, g_back_buffer, g_width * g_height * sizeof(uint32));
}

void swapbuf_region(int x, int y, int w, int h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)g_width) w = g_width - x;
    if (y + h > (int)g_height) h = g_height - y;
    if (w <= 0 || h <= 0) return;

    for (int row = 0; row < h; row++) {
        uint32* src = &g_back_buffer[(y + row) * g_width + x];
        uint32* dst = &g_vbe_buffer[(y + row) * g_width + x];
        memcpy(dst, src, w * sizeof(uint32));
    }
}

void vesa_cleanup() {
    if (g_back_buffer) {
        kfree(g_back_buffer);
        g_back_buffer = NULL;
    }

    g_width = 0;
    g_height = 0;
    g_vbe_buffer = NULL;
    g_selected_mode = -1;
}

int vesa_init(uint32 width, uint32 height, uint32 bpp) {
    vesa_cleanup();

    bios32_init();
    printf("initializing vesa vbe 2.0\n");
    if (!rq_info()) {
        printf("No VBE2 was detected\n");
        return -1;
    }
    printf("\ngot info!");

    g_selected_mode = vbe_srcmodes(width, height, bpp);
    kprint("searching for mode %dx%d@%dbpp\n", width, height, bpp);
    printf("\nfound: %d\n", g_selected_mode);
    if (g_selected_mode == -1) {
        printf("failed to find mode for %d-%d\n", width, height);
        kprint("failed to find mode\n");
        return -1;
    }
    kprint("selected mode: 0x%x\n", g_selected_mode);
    printf("\nselected mode: %d \n", g_selected_mode);

    g_width = g_vbe_modeinfoblock.XResolution;
    g_height = g_vbe_modeinfoblock.YResolution;
    g_vbe_buffer = (uint32 *)g_vbe_modeinfoblock.PhysBasePtr;

    g_back_buffer = (uint32 *)kmalloc(g_width * g_height * sizeof(uint32));
    if (!g_back_buffer) {
        printf("failed to allocate back buffer!\n");
        return -1;
    }
    memset(g_back_buffer, 0, g_width * g_height * sizeof(uint32));

    vbe_setmode(g_selected_mode);

    return 0;
}