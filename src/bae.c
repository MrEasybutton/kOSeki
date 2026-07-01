#include "bae.h"
#include "8259_pic.h"
#include "kheap.h"
#include "console.h"
#include "idt.h"
#include "ports.h"
#include "isr.h"
#include "string.h"
#include "types.h"
#include "vesa.h"
#include "bitmap.h"
#include "bmp.h"
#include "fat32.h"
#include "gui.h"

// ok listen i know bae is a rat, just hear me out
// edit: bae now has her own app but imma just leave this as is

int g_mouse_x_pos, g_mouse_y_pos;
MOUSE_STATUS g_status;

int g_mouse_sensitivity = 100;

int cursor_width, cursor_height, cursor_row_padded;
static char *cursor_bmp_data, *cursor_pixel_data;

int mouse_getx(void) { return g_mouse_x_pos; }
int mouse_gety(void) { return g_mouse_y_pos; }
BOOL sneaky_bae = FALSE;

void mouse_wait(BOOL write) {
    uint32 t = 1000000;
    while (t--) {
        uint8 s = inportb(PS2_CMD_PORT);
        if (!write && (s & 1)) return;
        if ( write && !(s & 2)) return;
    }
}

void mouse_write(uint8 data) {
    mouse_wait(TRUE);
    outportb(PS2_CMD_PORT, 0xD4);
    mouse_wait(TRUE);
    outportb(MOUSE_DATA_PORT, data);
}

uint8 mouse_read(void) {
    mouse_wait(FALSE);
    return inportb(MOUSE_DATA_PORT);
}

void get_mouse_status(char b, MOUSE_STATUS *s) {
    memset(s, 0, sizeof(*s));
    s->left_button = b & 0x01;
    s->right_button = b & 0x02;
    s->middle_button = b & 0x04;
    s->always_1 = b & 0x08;
    s->x_sign = b & 0x10;
    s->y_sign = b & 0x20;
    s->x_overflow = b & 0x40;
    s->y_overflow = b & 0x80;
}

void m_render(int x, int y) {
    if (!cursor_pixel_data || cursor_width <= 0 || cursor_height <= 0)
        return;

    int shadow_offset = 1;
    uint32 shadow_color = RGBA(0, 0, 0, 88);

    if (!sneaky_bae) {
        // i just love Latinas, Sonic
        for (int sy = 0; sy < cursor_height; sy++) {
            char *row = cursor_pixel_data + (cursor_height - 1 - sy) * cursor_row_padded;

            for (int sx = 0; sx < cursor_width; sx++) {
                uint8 b = row[sx * 3 + 0];
                uint8 g = row[sx * 3 + 1];
                uint8 r = row[sx * 3 + 2];

                if (!(r == 255 && g == 255 && b == 255)) {
                    int dx = x + sx + shadow_offset;
                    int dy = y + sy + shadow_offset;
                    pixel(dx, dy, shadow_color);
                }
            }
        }

        for (int sy = 0; sy < cursor_height; sy++) {
            char *row = cursor_pixel_data + (cursor_height - 1 - sy) * cursor_row_padded;

            for (int sx = 0; sx < cursor_width; sx++) {
                uint8 b = row[sx * 3 + 0];
                uint8 g = row[sx * 3 + 1];
                uint8 r = row[sx * 3 + 2];

                if (r == 255 && g == 255 && b == 255)
                    continue;

                uint32 c = RGB(r, g, b);
                pixel(x + sx, y + sy, c);
            }
        }
        
    }
}

void mouse_handler(REGISTERS *r) {
    static uint8 cycle;
    static char b[3];
    
    if (cycle < 2) {
        b[cycle] = mouse_read();
        if (cycle == 0) get_mouse_status(b[0], &g_status);

        cycle++;
    } 
    else {
        b[2] = mouse_read();

        int w = cursor_width + 1;
        int h = cursor_height + 1;

        int dx = ((sint8)b[1] * g_mouse_sensitivity) / 100;
        int dy = ((sint8)b[2] * g_mouse_sensitivity) / 100;

        g_mouse_x_pos += dx;
        g_mouse_y_pos -= dy;

        if (g_mouse_x_pos < 0) g_mouse_x_pos = 0;
        if (g_mouse_y_pos < 0) g_mouse_y_pos = 0;

        if (g_mouse_x_pos > (int)SCREEN_WIDTH - w)
            g_mouse_x_pos = SCREEN_WIDTH - w;

        if (g_mouse_y_pos > (int)SCREEN_HEIGHT - h)
            g_mouse_y_pos = SCREEN_HEIGHT - h;

        cycle = 0;
    }

    end_interrupt(IRQ_BASE + 12);
}


void set_mouse_rate(uint8 rate) {
    uint8 s;
    outportb(MOUSE_DATA_PORT, MOUSE_CMD_SAMPLE_RATE);
    
    if ((s = mouse_read()) != MOUSE_ACKNOWLEDGE) {
        printf("error: failed to send mouse sample rate command\n");
        return;
    }
    outportb(MOUSE_DATA_PORT, rate);
    if ((s = mouse_read()) != MOUSE_ACKNOWLEDGE)
        printf("error: failed to send mouse sample rate data\n");
}

void mouse_init(void) {
    uint8 s;

    g_mouse_x_pos = SCREEN_WIDTH / 2;
    g_mouse_y_pos = SCREEN_HEIGHT / 2;

    cursor_bmp_data = fat_read_file("/SYSTEM/mausgem.bmp");
    if (cursor_bmp_data) {
        BMP_FILE_H *fh = (BMP_FILE_H *)cursor_bmp_data;
        BMP_INFO_H *ih = (BMP_INFO_H *)(cursor_bmp_data + sizeof(*fh));

        if (fh->type == 0x4D42 && ih->bpp == 24 && ih->width > 0 && ih->height > 0) {
            cursor_width = ih->width;
            cursor_height = ih->height;
            cursor_row_padded = (cursor_width * 3 + 3) & ~3;
            
            uint32 image_size = cursor_row_padded * cursor_height;
            cursor_pixel_data = (char*)kmalloc(image_size);
            memcpy(cursor_pixel_data, cursor_bmp_data + fh->offset, image_size);
            kfree(cursor_bmp_data);
        } else {
            kfree(cursor_bmp_data);
            cursor_bmp_data = cursor_pixel_data = NULL;
        }
    }

    mouse_wait(TRUE);
    outportb(PS2_CMD_PORT, 0xA8);

    outportb(MOUSE_DATA_PORT, MOUSE_CMD_MOUSE_ID);
    mouse_read();

    set_mouse_rate(67);

    mouse_wait(TRUE);
    outportb(PS2_CMD_PORT, 0x20);
    mouse_wait(FALSE);
    s = inportb(MOUSE_DATA_PORT) | 2;
    mouse_wait(TRUE);
    outportb(PS2_CMD_PORT, MOUSE_DATA_PORT);
    mouse_wait(TRUE);
    outportb(MOUSE_DATA_PORT, s);

    mouse_write(MOUSE_CMD_SET_DEFAULTS);
    if (mouse_read() != MOUSE_ACKNOWLEDGE) return;

    mouse_write(MOUSE_CMD_ENABLE_PACKET_STREAMING);
    if (mouse_read() != MOUSE_ACKNOWLEDGE) return;

    register_interrupt_handler(IRQ_BASE + 12, mouse_handler);
}