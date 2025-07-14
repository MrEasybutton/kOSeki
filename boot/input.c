#include <stdint.h>

int mx, my;
int left_clicked, right_clicked, middle_clicked;
int current_byte = 0;
uint8_t bytes[4] = { 0 };
int mspeed = 20;
int curr_mouse_target = 0; 

int Scancode = -1;
int shift_pressed = 0;
int caps_pressed = 0;
int escape_pressed = 0;
int backspace_pressed = 0;
int alt_pressed = 0;
int ctrl_pressed = 0;
int enter_pressed = 0;

#define TRUE 1
#define FALSE 0

#define pic1_command 0x20
#define pic1_data 0x21
#define pic2_command 0xa0
#define pic2_data 0xa1
#define icw1_def 0x10
#define icw1_icw4 0x01
#define icw4_x86 0x01

#define y_overflow       0b10000000
#define x_overflow       0b01000000
#define y_negative       0b00100000
#define x_negative       0b00010000
#define always_set       0b00001000
#define middle_click     0b00000100
#define right_click      0b00000010
#define left_click       0b00000001
#define unused_a         0b10000000
#define unused_b         0b01000000

void UpdateIDT();
extern void LoadIDT();
void HandleISR1();
void HandleISR12();
void RemapPIC();
void HandleMouseInterrupt();
void HandleMousePacket();

struct IDTElement {
    unsigned short lower;
    unsigned short selector;
    unsigned char zero;
    unsigned char flags;
    unsigned short higher;
};

struct IDTElement _idt[256];
extern unsigned int isr1, isr12;
unsigned int base, base12;

unsigned char inportb(unsigned short port) {
    unsigned char value;

    __asm__ __volatile__ ("inb %1, %0" : "=a" (value) : "dN" (port));

    return value;
}

void outportb(unsigned short port, unsigned char data) {
    __asm__ __volatile__ ("outb %1, %0" : : "dN" (port), "a" (data));
}

void UpdateIDT() {
    _idt[1].lower = (base & 0xffff);
    _idt[1].higher = (base >> 16) & 0xffff;
    _idt[1].selector = 0x08;
    _idt[1].zero = 0;
    _idt[1].flags = 0x8e;

    _idt[12].lower = (base12 & 0xffff);
    _idt[12].higher = (base12 >> 16) & 0xffff;
    _idt[12].selector = 0x08;
    _idt[12].zero = 0;
    _idt[12].flags = 0x8e;

    RemapPIC();

    outportb(0x21, 0b11111001);
    outportb(0xa1, 0x00);

    LoadIDT();
}

void RemapPIC() {
    unsigned char a, b;

    a = inportb(pic1_data);
    b = inportb(pic2_data);

    outportb(pic1_command, icw1_def | icw1_icw4);
    outportb(pic2_command, icw1_def | icw1_icw4);

    outportb(pic1_data, 0);
    outportb(pic2_data, 8);

    outportb(pic1_data, 4);
    outportb(pic2_data, 2);

    outportb(pic1_data, icw4_x86);
    outportb(pic2_data, icw4_x86);

    outportb(pic1_data, a);
    outportb(pic2_data, b);
}

static unsigned char last_raw_scancode = 0;

void HandleISR1() {
    unsigned char raw_scancode = inportb(0x60);
    
    if (!(raw_scancode & 0x80)) {
        if (raw_scancode != last_raw_scancode || (raw_scancode == last_raw_scancode && Scancode < 0)) {
            Scancode = raw_scancode;
        }
        
        if (raw_scancode == 0x2A || raw_scancode == 0x36) {
            shift_pressed = TRUE;
        } else if (raw_scancode == 0x0E) {
            backspace_pressed = TRUE;
        } else if (raw_scancode == 0x1C) {
            enter_pressed = TRUE;
        } else if (raw_scancode == 0x1D) {
            ctrl_pressed = TRUE;
        } else if (raw_scancode == 0x38) {
            alt_pressed = TRUE;
        } else if (raw_scancode == 0x3A) {
            caps_pressed = !caps_pressed;
        }
    } else {
        unsigned char key_code = raw_scancode & 0x7F;
        Scancode = -key_code;
        
        if (key_code == 0x2A || key_code == 0x36) {
            shift_pressed = FALSE;
        } else if (key_code == 0x0E) {
            backspace_pressed = FALSE;
        } else if (key_code == 0x1C) {
            enter_pressed = FALSE;
        } else if (key_code == 0x1D) {
            ctrl_pressed = FALSE;
        } else if (key_code == 0x38) {
            alt_pressed = FALSE;
        }
    }
    
    last_raw_scancode = raw_scancode;
    
    outportb(0xa0, 0x20);
    outportb(0x20, 0x20);
}

void HandleMouseInterrupt();
void HandleISR12() {
    HandleMouseInterrupt();
    outportb(0xa0, 0x20);
    outportb(0x20, 0x20);
}

void MouseWait(unsigned char type) {
    int time_out = 100000;

    if (type == 0) {
        while (time_out--) {
            if ((inportb(0x64) & 1) == 1)
                return;
        }
        return;
    }
    else {
        while (time_out--) {
            if ((inportb(0x64) & 2) == 0)
                return;
        } 
        return;
    }
}

void MouseWrite(unsigned char data) {
    MouseWait(1);
    outportb(0x64, 0xd4);

    MouseWait(1);
    outportb(0x60, data);
}

unsigned char MouseRead() {
    MouseWait(0);
    return inportb(0x60);
}

void UpdateMouse() {
    unsigned char status;

    MouseWait(1);
    outportb(0x64, 0xd4);

    MouseWait(1);
    outportb(0x64, 0xa8);

    MouseWait(1);
    outportb(0x64, 0x20);
    MouseWait(0);
    status = inportb(0x60);
    
    status |= 2;
    status &= ~0x20;

    MouseWait(1);
    outportb(0x64, 0x60);
    MouseWait(1);
    outportb(0x60, status);

    MouseWrite(0xff);
    MouseRead();

    MouseWrite(0xf6);
    MouseRead();
    

    MouseWrite(0xf3);
    MouseRead();
    MouseWrite(0xf4);
    MouseRead();
    
    MouseWrite(0xf4);
    MouseRead();

    MouseWrite(0xf4);
    MouseRead();
    MouseWrite(0x03);
    MouseRead();
}

void HandleMousePacket();
void HandleMouseInterrupt() {
    uint8_t byte = MouseRead();
    if (current_byte == 0 && !(byte & always_set)) return;

    bytes[current_byte] = byte;
    current_byte++;

    if (current_byte >= 4) current_byte = 0;

    if (current_byte == 0) {
        HandleMousePacket();
    }
}


void HandleMousePacket() {
    VBEInfoBlock* VBE = (VBEInfoBlock*) VBEInfoAddress;

    uint8_t status = bytes[0];
    int32_t change_mx = (int32_t) bytes[1];
    int32_t change_my = (int32_t) bytes[2];

    if (status & x_overflow || status & y_overflow) return;

    if (status & x_negative) change_mx |= 0xffffff00;
    if (status & y_negative) change_my |= 0xffffff00;

    left_clicked = status & left_click;
    right_clicked = status & right_click;
    middle_clicked = status & middle_click;

    mx += (change_mx > 0 ? mspeed : (change_mx < 0 ? -mspeed : 0)); 
    my += (change_my > 0 ? -mspeed : (change_my < 0 ? mspeed : 0));
    
    if (mx < 2) mx = 2;
    if (mx > VBE->x_resolution - 2) mx = VBE->x_resolution - 2;
    if (my < 2) my = 2;
    if (my > VBE->y_resolution - 2) my = VBE->y_resolution - 2;
}

unsigned char ProcessScancode(int scancode) {
    if (scancode ==  0x01)
        escape_pressed = TRUE;
        
    else if (scancode ==  0x02)
        if (shift_pressed == TRUE)
            return '!';
        else 
            return '1';
        
    else if (scancode ==  0x03)
        if (shift_pressed == TRUE)
            return '"';
        else 
            return '2';
        
    else if (scancode ==  0x04)
        if (shift_pressed == TRUE)
            return '#';
        else 
            return '3';
        
    else if (scancode ==  0x05)
        if (shift_pressed == TRUE)
            return '$';
        else 
            return '4';
            
    else if (scancode == 0x06)
        if (shift_pressed == TRUE)
            return '%';
        else 
            return '5';
        
    else if (scancode == 0x07)
        if (shift_pressed == TRUE)
            return '^';
        else 
            return '6';
        
    else if (scancode == 0x08)
        if (shift_pressed == TRUE)
            return '&';
        else 
            return '7';
        
    else if (scancode == 0x09)
        if (shift_pressed == TRUE)
            return '*';
        else 
            return '8';
        
    else if (scancode == 0x0A)
        if (shift_pressed == TRUE)
            return '(';
        else 
            return '9';
        
    else if (scancode == 0x0B)
        if (shift_pressed == TRUE)
            return ')';
        else 
            return '0';
        
    else if (scancode == 0x0C)
        if (shift_pressed == TRUE)
            return '_';
        else 
            return '-';
            
    else if (scancode == 0x0D)
        if (shift_pressed == TRUE)
            return '+';
        else 
            return '=';
        
    else if (scancode == 0x0E) {
        backspace_pressed = TRUE;
        return 8;
    }
        
        
    else if (scancode == 0x0F)
        return '\t';
        
    else if (scancode == 0x10)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'Q';
        else 
            return 'q';
        
    else if (scancode == 0x11)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'W';
        else 
            return 'w';
        
    else if (scancode == 0x12)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'E';
        else 
            return 'e';
        
    else if (scancode == 0x13)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'R';
        else 
            return 'r';
        
    else if (scancode == 0x14)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'T';
        else 
            return 't';
        
    else if (scancode == 0x15)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'Y';
        else 
            return 'y';
        
    else if (scancode == 0x16)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'U';
        else 
            return 'u';
        
    else if (scancode == 0x17)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'I';
        else 
            return 'i';
        
    else if (scancode == 0x18)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'O';
        else 
            return 'o';
        
    else if (scancode == 0x19)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'P';
        else 
            return 'p';
        
    else if (scancode == 0x1A)
        if (shift_pressed == TRUE)
            return '{';
        else 
            return '[';
        
    else if (scancode == 0x1B)
        if (shift_pressed == TRUE)
            return '}';
        else 
            return ']';
        
    else if (scancode == 0x1C) {
        enter_pressed = TRUE;
        return 10;
    }
        
    else if (scancode == 0x1D)
        ctrl_pressed = TRUE;
        
        
    else if (scancode == 0x1E)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'A';
        else 
            return 'a';
        
    else if (scancode == 0x1F)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'S';
        else 
            return 's';
        
    else if (scancode == 0x20)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'D';
        else 
            return 'd';
        
    else if (scancode == 0x21)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'F';
        else 
            return 'f';
        
    else if (scancode == 0x22)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'G';
        else 
            return 'g';
        
    else if (scancode == 0x23)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'H';
        else 
            return 'h';
        
    else if (scancode == 0x24)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'J';
        else 
            return 'j';
        
    else if (scancode == 0x25)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'K';
        else 
            return 'k';
        
    else if (scancode == 0x26)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'L';
        else 
            return 'l';
        
    else if (scancode == 0x27)
        if (shift_pressed == TRUE)
            return ':';
        else 
            return ';';
        
    else if (scancode == 0x28) 
        if (shift_pressed == TRUE)
            return '@';
        else 
            return '\'';
        
    else if (scancode == 0x29)
        if (shift_pressed == TRUE)
            return '~';
        else 
            return '`';
        
    else if (scancode == 0x2A)
        shift_pressed = TRUE;
        
        
    else if (scancode == 0x2B)
        if (shift_pressed == TRUE)
            return '|';
        else 
            return '\\';
    
    else if (scancode == 0x2C)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'Z';
        else 
            return 'z';
    
    else if (scancode == 0x2D)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'X';
        else 
            return 'x';
        
    else if (scancode == 0x2E)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'C';
        else 
            return 'c';
        
    else if (scancode == 0x2F)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'V';
        else 
            return 'v';
        
    else if (scancode == 0x30)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'B';
        else 
            return 'b';
        
    else if (scancode == 0x31)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'N';
        else 
            return 'n';
        
    else if (scancode == 0x32)
        if (shift_pressed == TRUE || caps_pressed == TRUE)
            return 'M';
        else 
            return 'm';
        
    else if (scancode == 0x33)
        if (shift_pressed == TRUE)
            return '<';
        else 
            return ',';
        
    else if (scancode == 0x34)
        if (shift_pressed == TRUE)
            return '>';
        else 
            return '.';
    
    else if (scancode == 0x35)
        if (shift_pressed == TRUE)
            return '?';
        else 
            return '/';

    else if (scancode == 0x36)
        shift_pressed = TRUE;
        
    else if (scancode == 0x38)
        alt_pressed = TRUE;
        
        
    else if (scancode == 0x39)
        return ' ';
    
    else if (scancode == 0x3A)
        return '\0';
        
    if (scancode == 0xAA || scancode == 0xB6)
        return '\0';

    return '\0';
}