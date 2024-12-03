int ProcessLen = 0;

#define process_type_void 0
#define process_type_string_buffer 1
#define procparamlen 10

#include "apps/process_system.h"
#include <stdbool.h>
#include "graphics.h"

struct Process process[256];
int iparams[100] = {10};

void CollectProcess() {
    int priority;
    int i = 0;

    priority = 5;

    // Priority setting for window processes (focus windows basically)
    while (priority >= 0) {
        i = curr_mouse_target;
        if (left_clicked == true &&
                mx > iparams[i * procparamlen + 0] &&
                mx < iparams[i * procparamlen + 0] + iparams[i * procparamlen + 2] &&
                my > iparams[i * procparamlen + 1] &&
                my < iparams[i * procparamlen + 1] + iparams[i * procparamlen + 3])
                break;
        for (i = 0; i < ProcessLen; i++) {
            if (left_clicked == true &&
                mx > iparams[i * procparamlen + 0] &&
                mx < iparams[i * procparamlen + 0] + iparams[i * procparamlen + 2] &&
                my > iparams[i * procparamlen + 1] &&
                my < iparams[i * procparamlen + 1] + iparams[i * procparamlen + 3]) {
                    process[curr_mouse_target].priority = 0;
                    curr_mouse_target = i;
                    process[i].priority = 2;
                    left_clicked = false;
                }
        }

        priority--;
    }

    priority = 0;
    while (priority <= 5) {
        for (int i = 0; i < ProcessLen; i++) {
            if (process[i].priority == priority) {
                process[i].function(process[i].process_inst);
            }
        }

        priority++;
    }
}

int NullProcess(int process_inst) {
    return 0;
}

void CloseProcess(int process_inst) {
    if (process_inst < 0 || process_inst >= ProcessLen) {
        return; 
    }

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


// Clear Function (Background colour and icon)
int ClearFunc(int process_inst) {
    Clear(bg_r, bg_g, bg_b);
    VBEInfoBlock* VBE = (VBEInfoBlock*) VBEInfoAddress;
    
    if (bg_r == 200) {
        DrawIconBrand(VBE->x_resolution / 2 - 460, VBE->y_resolution / 2 - 300, 30, 30, 180, 160, 190, 0);
    } else {
        DrawIconBrand(VBE->x_resolution / 2 - 460, VBE->y_resolution / 2 - 300, 30, 30, 100, 80, 110, 0);
    }
    DrawIconBrand(VBE->x_resolution / 2 - 152, VBE->y_resolution / 2 - 85, 10, 10, 30, 10, 40, 0);
    return 0;
}

// Display mouse
int MouseFunc(int process_inst) {
    MouseGraphics(mx, my, 105, 25, 100);

    return 0;
}

