#include "graphics.h"
#include "filesystem.h"
#include "text_input.h"
#include "app_loader.h"
#include "taskbar.c"
#include "apps/process_system.h"
#include "startup_screen.c"
#include "time.h"
#include "kronii.c"

int start() {
    VBEInfoBlock* VBE = (VBEInfoBlock*) VBEInfoAddress;

    STARTUP(); // to remind you this is extremely useless but it looks cool so if you dont like it just comment it out

    fs_init();
    fs_mount();

    init_text_input_system();

    mx = VBE->x_resolution / 2;
    my = VBE->y_resolution / 2;

	bg_r1 = 204; bg_g1 = 190; bg_b1 = 212; bg_r2 = 210; bg_g2 = 182; bg_b2 = 216;
	current_size = 4;

	base = (unsigned int) &isr1;
	base12 = (unsigned int) &isr12;

    mspeed = 7;

	process[ProcessLen].priority = 0;
	process[ProcessLen].function = &ClearFunc;
	ProcessLen++;

	process[ProcessLen].priority = 3;
	process[ProcessLen].function = &TaskbarSystem;
	process[ProcessLen].process_inst = ProcessLen;
	iparams[ProcessLen * procparamlen + 0] = 0;
	iparams[ProcessLen * procparamlen + 1] = 0;
	iparams[ProcessLen * procparamlen + 2] = 60;
	iparams[ProcessLen * procparamlen + 3] = VBE->y_resolution;
	iparams[ProcessLen * procparamlen + 4] = 1;
	ProcessLen++;

	process[ProcessLen].priority = 5;
	process[ProcessLen].function = &MouseFunc;
	ProcessLen++;
	
	process[ProcessLen].priority = 1;
	process[ProcessLen].function = &Clock;
	ProcessLen++;

	UpdateIDT();
	UpdateMouse();
    
    while(1) {
        CollectProcess();
        Refresh();
    }
}