#include "apps/process_system.h"
#include <stdbool.h>
#include "graphics.h"
#include "app_loader.h"

int TaskbarSystem(int process_inst) {
    VBEInfoBlock* VBE = (VBEInfoBlock*) VBEInfoAddress;

    
    int taskbar_width = 60; 
    DrawRect(0, 0, taskbar_width, VBE->y_resolution, 160, 160, 160);
    DrawRect(taskbar_width, 0, 2, VBE->y_resolution, 10, 10, 10); // Shadow
    DrawRect(taskbar_width + 2, 0, 2, VBE->y_resolution, 135, 135, 135);

    int y_offset = 10;  
    int button_height = 40;
    int marginal_offset = 25;

    static bool greet_trigger = false;

    int i = iparams[process_inst * procparamlen + 4];
    
    char AppTestDesc[] = "KRON\0"; // Kronii's Stopwatch
    if (DrawButton(5, y_offset, taskbar_width - 10, button_height, 151, 113, 205, AppTestDesc, 200, 190, 210, process_inst, 20, 20, 20) == true) {
        process[ProcessLen].priority = 0;
        process[ProcessLen].process_inst = ProcessLen;
        process[ProcessLen].function = &Stopwatch;
        iparams[ProcessLen * procparamlen + 0] = i * 60;
        iparams[ProcessLen * procparamlen + 1] = i * 60;
        iparams[ProcessLen * procparamlen + 2] = 300;
        iparams[ProcessLen * procparamlen + 3] = 300;
        iparams[ProcessLen * procparamlen + 4] = 50;
        iparams[ProcessLen * procparamlen + 5] = 54;
        iparams[ProcessLen * procparamlen + 6] = 75;
        ProcessLen++;
        iparams[process_inst * procparamlen + 4]++;
    }
    
    
    y_offset += button_height + marginal_offset;  

    
    char BallAppDesc[] = "KINE\0"; // Kinema Physics Simulator
    if (DrawButton(5, y_offset, taskbar_width - 10, button_height, 121, 83, 175, BallAppDesc, 210, 200, 220, process_inst, 0, 140, 0) == true) {
        process[ProcessLen].priority = 0;
        process[ProcessLen].process_inst = ProcessLen;
        process[ProcessLen].function = &Kinema;
        iparams[ProcessLen * procparamlen + 0] = i * 60;
        iparams[ProcessLen * procparamlen + 1] = i * 60;
        iparams[ProcessLen * procparamlen + 2] = 300;
        iparams[ProcessLen * procparamlen + 3] = 450;
        iparams[ProcessLen * procparamlen + 4] = 20;
        iparams[ProcessLen * procparamlen + 5] = 20;
        iparams[ProcessLen * procparamlen + 6] = 50;
        iparams[ProcessLen * procparamlen + 7] = 14;
        iparams[ProcessLen * procparamlen + 8] = 20;

        ProcessLen++;
        iparams[process_inst * procparamlen + 4]++;
    }

    
    y_offset += button_height + marginal_offset;  

    char AppSettingsDesc[] = "SYST\0"; // Settings 
    if (DrawButton(5, y_offset, taskbar_width - 10, button_height, 91, 53, 145, AppSettingsDesc, 220, 210, 230, process_inst, 0, 140, 0) == true) {
        process[ProcessLen].priority = 0;
        process[ProcessLen].process_inst = ProcessLen;
        process[ProcessLen].function = &Settings;
        iparams[ProcessLen * procparamlen + 0] = i * 60;
        iparams[ProcessLen * procparamlen + 1] = i * 60;
        iparams[ProcessLen * procparamlen + 2] = 300;
        iparams[ProcessLen * procparamlen + 3] = 300;
        iparams[ProcessLen * procparamlen + 4] = 80;
        iparams[ProcessLen * procparamlen + 5] = 80;
        iparams[ProcessLen * procparamlen + 6] = 80;
        iparams[ProcessLen * procparamlen + 7] = 20;
        iparams[ProcessLen * procparamlen + 8] = 10;
        ProcessLen++;
        iparams[process_inst * procparamlen + 4]++;
    }

    DrawIconBrand(-2, VBE->y_resolution - 32 - 10, 2, 2, 100, 100, 100, 0); // Pebble Icon
    
}
