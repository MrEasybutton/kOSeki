#include "keyboard.h"
#include "video.h"
#include "kernel.h"
#include "console.h"
#include "idt.h"
#include "ports.h"
#include "isr.h"
#include "types.h"
#include "string.h"

// this implementation is adapted from https://github.com/xing1357/SimpleOS/blob/main/src/keyboard.c because the old one sucks

static BOOL g_caps_lock = FALSE;
static BOOL g_shift_pressed = FALSE;
static BOOL g_ctrl_pressed = FALSE;
static BOOL g_key_states[256];
volatile unsigned char g_ch = 0;
volatile char g_scan_code = 0;
extern uint8 g_video_mode;

char g_scan_code_chars[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0
};


static int get_scancode() {
    int i, scancode = 0;

    for (i = 1000; i > 0; i--) {
        if ((inportb(KEYBOARD_STATUS_PORT) & 1) == 0) continue;
        scancode = inportb(KEYBOARD_DATA_PORT);
        break;
    }
    if (i > 0) return scancode;
    return 0;
}

char alternate_chars(char ch) {
    switch(ch) {
        case '`': return '~';
        case '1': return '!';
        case '2': return '@';
        case '3': return '#';
        case '4': return '$';
        case '5': return '%';
        case '6': return '^';
        case '7': return '&';
        case '8': return '*';
        case '9': return '(';
        case '0': return ')';
        case '-': return '_';
        case '=': return '+';
        case '[': return '{';
        case ']': return '}';
        case '\\': return '|';
        case ';': return ':';
        case '\'': return '\"';
        case ',': return '<';
        case '.': return '>';
        case '/': return '?';
        default: return ch;
    }
}

void keyboard_handler(REGISTERS *r) {
    int scancode;

    scancode = get_scancode();
    g_scan_code = scancode;

    if (scancode & 0x80) {
        g_key_states[scancode & 0x7F] = FALSE;
        switch(scancode) {
            case SCAN_CODE_KEY_LEFT_CTRL_RELEASE:
                g_ctrl_pressed = FALSE;
                break;
            case SCAN_CODE_KEY_LEFT_SHIFT_RELEASE:
            case SCAN_CODE_KEY_RIGHT_SHIFT_RELEASE:
                g_shift_pressed = FALSE;
                break;
        }
    } else {
        g_key_states[scancode & 0x7F] = TRUE;
        switch(scancode) {
            case SCAN_CODE_KEY_CAPS_LOCK:
                if (g_caps_lock == FALSE)
                    g_caps_lock = TRUE;
                else
                    g_caps_lock = FALSE;
                break;

            case SCAN_CODE_KEY_ENTER:
                g_ch = '\n';
                break;

            case SCAN_CODE_KEY_TAB:
                g_ch = '\t';
                break;

            case SCAN_CODE_KEY_BACKSPACE:
                g_ch = '\b';
                break;

            case SCAN_CODE_KEY_LEFT_SHIFT:
                g_shift_pressed = TRUE;
                g_ch = KEY_LSHIFT;
                break;

            case SCAN_CODE_KEY_RIGHT_SHIFT:
                g_shift_pressed = TRUE;
                break;

            case SCAN_CODE_KEY_LEFT_CTRL:
                g_ctrl_pressed = TRUE;
                break;

            case SCAN_CODE_KEY_UP:
                g_ch = KEY_UP;
                break;

            case SCAN_CODE_KEY_DOWN:
                g_ch = KEY_DOWN;
                break;

            case SCAN_CODE_KEY_LEFT:
                g_ch = KEY_LEFT;
                break;

            case SCAN_CODE_KEY_RIGHT:
                g_ch = KEY_RIGHT;
                break;

            case SCAN_CODE_KEY_HOME:
                g_ch = KEY_HOME;
                break;

            case SCAN_CODE_KEY_END:
                g_ch = KEY_END;
                break;

            case SCAN_CODE_KEY_DELETE:
                g_ch = KEY_DELETE;
                break;

            default:
                if (scancode < sizeof(g_scan_code_chars) / sizeof(g_scan_code_chars[0])) {
                    char mapped_char = g_scan_code_chars[scancode];

                    if (mapped_char >= ' ' && mapped_char <= '~') 
                        g_ch = mapped_char; 
                    else g_ch = 0;
                } else { 
                    g_ch = 0; 
                }

                if (g_ch == 0) break;
                
                if (g_ctrl_pressed && isalpha(g_ch)) {
                    g_ch = lower(g_ch) - 'a' + 1;
                    break;
                }
                
                if (g_caps_lock) {
                    if (g_shift_pressed) g_ch = alternate_chars(g_ch);
                    else g_ch = upper(g_ch);
                } else {
                    if (g_shift_pressed) {
                        if (isalpha(g_ch)) g_ch = upper(g_ch);
                        else g_ch = alternate_chars(g_ch);
                    } else {
                        g_ch = g_scan_code_chars[scancode];
                    }
                }
                break;
        }
    }
}

void keyboard_init() {
    register_interrupt_handler(IRQ_BASE + 1, keyboard_handler);
}

//blocking
unsigned char kb_getchar() {
    unsigned char c;

    while(g_ch <= 0);
    c = g_ch;
    g_ch = 0;
    return c;
}

unsigned char kb_get_scancode() {
    unsigned char code;

    while(g_scan_code <= 0);
    code = g_scan_code;
    g_ch = 0;
    g_scan_code = 0;
    return code;
}

unsigned char kb_getchar_nb() {
    if (g_ch != 0) {
        unsigned char c = g_ch;
        g_ch = 0;
        return c;
    }
    return 0;
}

unsigned char kb_get_scancode_nb() {
    if (g_scan_code != 0) {
        unsigned char code = g_scan_code;
        g_scan_code = 0;
        return code;
    }
    return 0;
}

// nonblocking for DAR
BOOL kb_is_key_pressed(unsigned char scancode) {
    return g_key_states[scancode & 0x7F];
}