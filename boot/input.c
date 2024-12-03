#include <stdint.h>

int mx, my;                    
int left_clicked, right_clicked, middle_clicked; 
int current_byte = 0;           
uint8_t bytes[4] = { 0 };       
int mouse_speed = 3;            
int mspeed = 4;                 
int curr_mouse_target = 0; 


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


void HandleISR1() {
    outportb(0xa0, 0x20);
    outportb(0x20, 0x20);
}


void HandleISR12() {
    HandleMouseInterrupt();
    outportb(0xa0, 0x20);
    outportb(0x20, 0x20);
}


void MouseWait(unsigned char type) {
    int timeout = 100;
    while (timeout--) {
        if (type == 0 && (inportb(0x64) & 1)) return;
        if (type == 1 && !(inportb(0x64) & 2)) return;
    }
}

void MouseWrite(unsigned char data) {
    MouseWait(10);
    outportb(0x64, 0xd4);
    MouseWait(10);
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
    MouseWait(0);
    outportb(0x64, 0xa8);
    MouseWait(1);
    outportb(0x64, 0x20);
    MouseWait(0);
    status = (inportb(0x60) | 2);
    MouseWait(1);
    outportb(0x64, 0x60);
    MouseWait(1);
    outportb(0x60, status);

    MouseWrite(0xff);
    MouseRead();
    MouseWrite(0xf6);
    MouseRead();
    MouseWrite(0xf4);
    MouseRead();
}


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

    // movement
    mx += (change_mx > 0 ? mspeed : (change_mx < 0 ? -mspeed : 0)); 
    my += (change_my > 0 ? -mspeed : (change_my < 0 ? mspeed : 0));
    
    
    if (mx < 2) mx = 10;
    if (mx > VBE->x_resolution - 2) mx = VBE->x_resolution - 10;
    if (my < 2) my = 10;
    if (my > VBE->y_resolution - 2) my = VBE->y_resolution - 10;

}
