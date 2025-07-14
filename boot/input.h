#ifndef INPUT_H
#define INPUT_H

#define TRUE 1
#define FALSE 0

extern unsigned int isr1, isr12;

extern int x, y;
extern int left_clicked, right_clicked, middle_clicked;

extern int Scancode;
extern int shift_pressed;
extern int caps_pressed;
extern int escape_pressed;
extern int backspace_pressed;
extern int alt_pressed;
extern int ctrl_pressed;
extern int enter_pressed;

extern unsigned int base;
extern unsigned int base12;

unsigned char ProcessScancode(int scancode);

void InitialiseMouse();
void InitialiseIDT();
void HandleMousePacket();
void HandleMouseInterrupt();
void HandleISR1();
void HandleISR12();

#endif