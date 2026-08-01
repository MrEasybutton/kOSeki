#include "bae.h"
#include "serial.h"
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

#define PS2_STATUS_OUTPUT_FULL (1 << 0)
#define PS2_STATUS_AUX_DATA (1 << 5)
#define PS2_KEYBOARD_DISABLE 0xAD
#define PS2_KEYBOARD_ENABLE 0xAE

int g_mouse_sensitivity = 100;

int m_width, m_height, m_row_padded;
static char *m_bmp_data, *m_pixel_data;

int mouse_getx(void) { return g_mouse_x_pos; }
int mouse_gety(void) { return g_mouse_y_pos; }
BOOL sneaky_bae = FALSE;

void mouse_wait(BOOL write) {
    uint32 t = 1000000;
    while (t--) {
        uint8 s = inportb(PS2_CMD_PORT);
        if (!write && (s & PS2_STATUS_OUTPUT_FULL) && (s & PS2_STATUS_AUX_DATA)) return;
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

static void PS2_flush_out(void) {
    for (uint32 count = 0; count < 10000; count++) {
        if (!(inportb(PS2_CMD_PORT) & PS2_STATUS_OUTPUT_FULL)) break;
        (void)inportb(MOUSE_DATA_PORT);
    }
}

static BOOL mouse_write_n_ack(uint8 cmd) {
    for (int attempt = 0; attempt < 3; attempt++) {
        mouse_write(cmd);
        if (mouse_read() == MOUSE_ACKNOWLEDGE) return TRUE;
    }
    return FALSE;
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
    if (!m_pixel_data || m_width <= 0 || m_height <= 0)
        return;

    int shadow_offset = 1;
    uint32 shadow_color = RGBA(0, 0, 0, 88);

    if (!sneaky_bae) {
        // i just love Latinas, Sonic
        for (int sy = 0; sy < m_height; sy++) {
            char *row = m_pixel_data + (m_height - 1 - sy) * m_row_padded;

            for (int sx = 0; sx < m_width; sx++) {
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

        for (int sy = 0; sy < m_height; sy++) {
            char *row = m_pixel_data + (m_height - 1 - sy) * m_row_padded;

            for (int sx = 0; sx < m_width; sx++) {
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
    static uint8 cycle = 0;
    static uint8 b[3];
    static int dbg = 0;
    if (dbg < 3) { kprint("[bae] mouse irq fired\n"); dbg++; }

    while (inportb(PS2_CMD_PORT) & PS2_STATUS_OUTPUT_FULL) {
        uint8 status = inportb(PS2_CMD_PORT);
        if (!(status & PS2_STATUS_AUX_DATA)) break;

        uint8 data = inportb(MOUSE_DATA_PORT);
        if (cycle == 0 && !(data & 0x08)) continue;

        b[cycle++] = data;
        if (cycle == 3) {
            get_mouse_status(b[0], &g_status);

            int w = m_width + 1;
            int h = m_height + 1;
            int sens = g_mouse_sensitivity;
            if (sens < 1) sens = 1;

            g_mouse_x_pos += ((sint8)b[1] * sens) / 100;
            g_mouse_y_pos -= ((sint8)b[2] * sens) / 100;

            if (g_mouse_x_pos < 0) g_mouse_x_pos = 0;
            if (g_mouse_y_pos < 0) g_mouse_y_pos = 0;
            if (g_mouse_x_pos > (int)SCREEN_WIDTH - w) g_mouse_x_pos = SCREEN_WIDTH - w;
            if (g_mouse_y_pos > (int)SCREEN_HEIGHT - h) g_mouse_y_pos = SCREEN_HEIGHT - h;

            cycle = 0;
        }
    }

}


void set_mouse_rate(uint8 rate) {
    if (!mouse_write_n_ack(MOUSE_CMD_SAMPLE_RATE)) {
        kprint("[bae] failed to send mouse sample rate command\n");
        return;
    }
    if (!mouse_write_n_ack(rate)) kprint("[bae] failed to send mouse sample rate data\n");
}

void mouse_init(void) {
    extern void splash();
    splash("init bae...");
    uint8 s;

    // block input until bae resolves so it wont enter the buffer, also flush output
    mouse_wait(TRUE);
    outportb(PS2_CMD_PORT, PS2_KEYBOARD_DISABLE);
    PS2_flush_out();

    g_mouse_x_pos = SCREEN_WIDTH / 2;
    g_mouse_y_pos = SCREEN_HEIGHT / 2;

    m_bmp_data = fat_read_file("/SYSTEM/mausgem.bmp");
    if (m_bmp_data) {
        BMP_FILE_H *fh = (BMP_FILE_H *)m_bmp_data;
        BMP_INFO_H *ih = (BMP_INFO_H *)(m_bmp_data + sizeof(*fh));

        if (fh->type == 0x4D42 && ih->bpp == 24 && ih->width > 0 && ih->height > 0) {
            m_width = ih->width;
            m_height = ih->height;
            m_row_padded = (m_width * 3 + 3) & ~3;
            
            uint32 image_size = m_row_padded * m_height;
            m_pixel_data = (char*)kmalloc(image_size);
            memcpy(m_pixel_data, m_bmp_data + fh->offset, image_size);
            kfree(m_bmp_data);
        } else {
            kfree(m_bmp_data);
            m_bmp_data = m_pixel_data = NULL;
        }
    }

    mouse_wait(TRUE);
    outportb(PS2_CMD_PORT, 0xA8);

    mouse_write(MOUSE_CMD_MOUSE_ID);
    if (mouse_read() != MOUSE_ACKNOWLEDGE)
        kprint("[bae] mouse did not ack ID command\n");
    mouse_read(); // consume device id

    set_mouse_rate(67);

    mouse_wait(TRUE);
    outportb(PS2_CMD_PORT, 0x20);
    mouse_wait(FALSE);
    s = inportb(MOUSE_DATA_PORT) | 2;
    mouse_wait(TRUE);
    outportb(PS2_CMD_PORT, MOUSE_DATA_PORT);
    mouse_wait(TRUE);
    outportb(MOUSE_DATA_PORT, s);

    if (!mouse_write_n_ack(MOUSE_CMD_SET_DEFAULTS)) {
        kprint("[bae] failed to ack set_defaults\n");
        mouse_wait(TRUE);
        outportb(PS2_CMD_PORT, PS2_KEYBOARD_ENABLE);
        return;
    }

    if (!mouse_write_n_ack(MOUSE_CMD_ENABLE_PACKET_STREAMING)) {
        kprint("[bae] failed to ack enable_packet_sstreaming\n");
        mouse_wait(TRUE);
        outportb(PS2_CMD_PORT, PS2_KEYBOARD_ENABLE);
        return;
    }

    register_interrupt_handler(IRQ_BASE + 12, mouse_handler);
    pic8259_unmask(12);
    mouse_wait(TRUE);
    outportb(PS2_CMD_PORT, PS2_KEYBOARD_ENABLE);
    kprint("[bae] mouse init complete! (irq12 registered)\n");
}