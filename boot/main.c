#include "graphics.h"
#include "app_loader.h"
#include "taskbar.c"
#include "apps/process_system.h"

int start() {
    VBEInfoBlock* VBE = (VBEInfoBlock*) VBEInfoAddress;

    mx = VBE->x_resolution / 2;		// Mouse goes to middle of screen x
    my = VBE->y_resolution / 2;		// Mouse goes to middle of screen y

	// BG Colour Initial Settings
	bg_r = 200; 
	bg_g = 182;
	bg_b = 201;

	base = (unsigned int) &isr1;
	base12 = (unsigned int) &isr12;

    mspeed = 10;

	// BG Colour Process, it clears the screen with the specific colour (bg_r, bg_g, bg_b)
	process[ProcessLen].priority = 0;
	process[ProcessLen].function = &ClearFunc;
	ProcessLen++;

	// Taskbar Process, wraps around y_res. By modifying the taskbar function and editing params 2 and 3, you can get other taskbar alignments.
	process[ProcessLen].priority = 3;
	process[ProcessLen].function = &TaskbarSystem;
	process[ProcessLen].process_inst = ProcessLen;
	iparams[ProcessLen * procparamlen + 0] = 0;
	iparams[ProcessLen * procparamlen + 1] = 0;
	iparams[ProcessLen * procparamlen + 2] = 60;
	iparams[ProcessLen * procparamlen + 3] = VBE->y_resolution;
	iparams[ProcessLen * procparamlen + 4] = 1;
	ProcessLen++;


	// Mouse Function
	process[ProcessLen].priority = 5;
	process[ProcessLen].function = &MouseFunc;
	ProcessLen++;


    while(1) {
        UpdateMouse();
	    UpdateIDT();
        CollectProcess();
        Refresh();
    }
}