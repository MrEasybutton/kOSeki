#ifndef TASKBAR_H
#define TASKBAR_H
#include <stdbool.h>

#define CASCADE_OFFSET 40
#define MAX_CASCADE_STEPS 10
#define MAX_APP_BUTTONS 10

typedef struct {
    char label[32];
    int (*app_function)(int);
    int bg_r, bg_g, bg_b;
    int text_r, text_g, text_b;
    int alt_r, alt_g, alt_b;
    int width;
    int height;
    int param4;
    int param5;
    int param6;
    int param7;
    int param8;
} AppButton;

void AppendRegistry(AppButton *app);
void InitRegistry(void);
void EvalPos(int i, int window_width, int window_height, int* x, int* y);
bool DrawAppButton(int x, int y, int width, int height, AppButton *app, int process_inst, int *app_counter);

int TaskbarSystem(int process_inst);

#endif