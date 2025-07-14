int ProcessLen = 0;

#define process_type_void 0
#define process_type_string_buffer 1
#define procparamlen 10

#include "apps/process_system.h"
#include <stdbool.h>
#include "graphics.h"
#include "text_input.h"

struct Process process[256];
int iparams[100] = {10};

void CollectProcess() {
    static int previous_left_clicked = false;
    static int last_scancode = -1;
    static int key_processed = 0;
    
    bool new_click = left_clicked && !previous_left_clicked;
    
    if (new_click) {
        bool found_window = false;
        
        if (curr_mouse_target >= 0 && curr_mouse_target < ProcessLen) {
            if (mx > iparams[curr_mouse_target * procparamlen + 0] &&
                mx < iparams[curr_mouse_target * procparamlen + 0] + iparams[curr_mouse_target * procparamlen + 2] &&
                my > iparams[curr_mouse_target * procparamlen + 1] &&
                my < iparams[curr_mouse_target * procparamlen + 1] + iparams[curr_mouse_target * procparamlen + 3]) {
                found_window = true;
            }
        }
        
        if (!found_window) {
            for (int i = ProcessLen - 1; i >= 0; i--) {
                if (process[i].priority >= 0 &&
                    mx > iparams[i * procparamlen + 0] &&
                    mx < iparams[i * procparamlen + 0] + iparams[i * procparamlen + 2] &&
                    my > iparams[i * procparamlen + 1] &&
                    my < iparams[i * procparamlen + 1] + iparams[i * procparamlen + 3]) {
                    
                    if (curr_mouse_target >= 0 && curr_mouse_target < ProcessLen) {
                        process[curr_mouse_target].priority = 0;
                    }
                    
                    curr_mouse_target = i;
                    process[i].priority = 2;
                    found_window = true;
                    break;
                }
            }
        }
        
        int found_text_field = 0;
        for (int i = 0; i < text_input_field_count; i++) {
            if (is_point_in_text_input_field(&text_input_fields[i], mx, my)) {
                focus_text_input_field(&text_input_fields[i]);
                found_text_field = 1;
                break;
            }
        }
        
        if (!found_text_field) {
            clear_text_input_focus();
        }
    }
    
    if (Scancode != last_scancode) {
        last_scancode = Scancode;
        key_processed = 0;
    }
    
    // ctrl+x shortcut
    if (ctrl_pressed && Scancode == 0x2D) {
        if (curr_mouse_target >= 3 && curr_mouse_target < ProcessLen) {
            if (process[curr_mouse_target].priority >= 0) {
                CloseProcess(curr_mouse_target);
                key_processed = 1;
            }
        }
    } else if (Scancode > 0 && !key_processed) {
        if (active_input_field) {
            process_text_input(Scancode, shift_pressed);
            key_processed = 1;
        }
    }
    
    previous_left_clicked = left_clicked;

    for (int priority = 0; priority <= 10; priority++) {
        for (int i = 0; i < ProcessLen; i++) {
            if (process[i].priority == priority) {
                process[i].function(process[i].process_inst);
            }
        }
    }
}

int NullProcess(int process_inst) {
    return 0;
}


void CloseProcess(int process_inst) {
    if (process_inst < 0 || process_inst >= ProcessLen) return;

    for (int i = 0; i < 100; i++) {
        process[process_inst].ca1[i] = 0;
    }
    
    process[process_inst].i1 = 0;
    process[process_inst].function = &NullProcess;
    process[process_inst].priority = -1;
    
    for (int i = 0; i < procparamlen; i++) {
        iparams[process_inst * procparamlen + i] = 0;
    }

    if (process_inst == ProcessLen - 1) {
        while (ProcessLen > 0 && process[ProcessLen - 1].priority == -1) {
            ProcessLen--; 
        }
    }
    
}

int ClearFunc(int process_inst) {
    VBEInfoBlock* VBE = (VBEInfoBlock*) VBEInfoAddress;

    //Clear(bg_r1, bg_g1, bg_b1);
    ClearScreenGradient(bg_r1, bg_g1, bg_b1, bg_r2, bg_g2, bg_b2);
    DrawIconBrand(VBE->x_resolution / 2 - 458, VBE->y_resolution / 2 - 298, 30, 30, 188, 160, 200, 0);
    DrawIconBrand(VBE->x_resolution / 2 - 460, VBE->y_resolution / 2 - 300, 30, 30, 194, 166, 206, 0);

    char info[] = "kOSeki MOAI Edition [v2.0]";
    DrawText(getFontCharacter, font_font_width, font_font_height, 
             info, VBE->x_resolution - 260, VBE->y_resolution - 20, 40, 30, 54);
    return 0;
}

int MouseFunc(int process_inst) {
    MouseGraphics(mx, my, 105, 25, 100);
    return 0;
}