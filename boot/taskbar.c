#include "taskbar.h"
#include "apps/process_system.h"
#include "graphics.h"
#include "app_loader.h"
#include "input.h"

static AppButton app_registry[MAX_APP_BUTTONS];
static int registered_apps = 0;

void AppendRegistry(AppButton *app) {
    if (registered_apps < MAX_APP_BUTTONS) {
        app_registry[registered_apps] = *app;
        registered_apps++;
    }
}

void InitRegistry() {
    AppButton app;
    registered_apps = 0;

    app.text_r = 1; app.text_g = 125; app.text_b = 186;
    
    app.param4 = 80; app.param5 = 80; app.param6 = 80;
    app.param7 = 20; app.param8 = 10;
    
    app.label[0] = 0;
    app.app_function = &Settings;
    app.bg_r = 91; app.bg_g = 53; app.bg_b = 145;
    app.alt_r = 0; app.alt_g = 140; app.alt_b = 0;
    app.width = 300; app.height = 300;
    AppendRegistry(&app);
    
    app.label[0] = 'A'; app.label[1] = 'M'; app.label[2] = 'E'; 
    app.label[3] = 0;
    app.app_function = &CaseFiles;
    app.bg_r = 251; app.bg_g = 201; app.bg_b = 107;
    app.alt_r = 220; app.alt_g = 220; app.alt_b = 255;
    app.width = 400; app.height = 350;
    AppendRegistry(&app);

    app.text_r = 220; app.text_g = 210; app.text_b = 230;
    
    app.label[0] = 'N'; app.label[1] = 'O'; app.label[2] = 'V'; 
    app.label[3] = 0;
    app.app_function = &Novella;
    app.bg_r = 47; app.bg_g = 52; app.bg_b = 56;
    app.alt_r = 0; app.alt_g = 140; app.alt_b = 0;
    app.width = 400; app.height = 500;
    AppendRegistry(&app);
    
    app.label[0] = 'W'; app.label[1] = 'A'; app.label[2] = 'H'; 
    app.label[3] = 0;
    app.app_function = &WAHtercolour;
    app.bg_r = 75; app.bg_g = 69; app.bg_b = 112;
    app.alt_r = 220; app.alt_g = 220; app.alt_b = 255;
    app.width = 500; app.height = 400;
    AppendRegistry(&app);
    
    app.label[0] = 'G'; app.label[1] = 'I'; app.label[2] = 'G'; 
    app.label[3] = 'I'; app.label[4] = 0;
    app.app_function = &Chaser;
    app.bg_r = 216; app.bg_g = 140; app.bg_b = 53;
    app.alt_r = 0; app.alt_g = 140; app.alt_b = 0;
    app.width = 300; app.height = 300;
    AppendRegistry(&app);
}

void EvalPos(int i, int window_width, int window_height, int* x, int* y) {
    VBEInfoBlock* VBE = (VBEInfoBlock*) VBEInfoAddress;
    int cascade_step = i % MAX_CASCADE_STEPS;
    *x = 80 + (cascade_step * CASCADE_OFFSET);
    *y = 60 + (cascade_step * CASCADE_OFFSET);

    if (*x + window_width > VBE->x_resolution) *x = 80;
    if (*y + window_height > VBE->y_resolution) *y = 60;
}

bool DrawAppButton(int x, int y, int width, int height, AppButton *app, int process_inst, int *app_counter) {
    for (int i = 0; i < ProcessLen; i++) {
        if (process[i].function == app->app_function && process[i].priority == 2) {
            DrawRect(2, y + 2, 4, height - 4, 245, 120, 200);
            break;
        }
    }
    
    if (DrawButton(x, y, width, height, app->bg_r, app->bg_g, app->bg_b, 
                  app->label, app->text_r, app->text_g, app->text_b, 
                  process_inst, app->alt_r, app->alt_g, app->alt_b) == true) {
        if (ProcessLen < 256) {
            process[ProcessLen].priority = 0;
            process[ProcessLen].process_inst = ProcessLen;
            process[ProcessLen].function = app->app_function;
            
            int window_x, window_y;
            EvalPos(*app_counter, app->width, app->height, &window_x, &window_y);
            
            iparams[ProcessLen * procparamlen + 0] = window_x;
            iparams[ProcessLen * procparamlen + 1] = window_y;
            iparams[ProcessLen * procparamlen + 2] = app->width;
            iparams[ProcessLen * procparamlen + 3] = app->height;
            
            if (app->param4 != 0) iparams[ProcessLen * procparamlen + 4] = app->param4;
            if (app->param5 != 0) iparams[ProcessLen * procparamlen + 5] = app->param5;
            if (app->param6 != 0) iparams[ProcessLen * procparamlen + 6] = app->param6;
            if (app->param7 != 0) iparams[ProcessLen * procparamlen + 7] = app->param7;
            if (app->param8 != 0) iparams[ProcessLen * procparamlen + 8] = app->param8;
            
            ProcessLen++;
            
            (*app_counter)++;
            return true;
        }
    }
    
    return false;
}

int TaskbarSystem(int process_inst) {
    static bool initialized = false;
    static int last_key = 0;
    
    if (!initialized) {
        InitRegistry();
        initialized = true;
    }
    
    if (ctrl_pressed && Scancode != last_key && Scancode >= 2 && Scancode <= 7) {
        int idx = Scancode - 2;
        if (idx < registered_apps && ProcessLen < 255) {
            AppButton* app = &app_registry[idx];
            process[ProcessLen].priority = 0;
            process[ProcessLen].process_inst = ProcessLen;
            process[ProcessLen].function = app->app_function;
            iparams[ProcessLen * procparamlen + 0] = 100;
            iparams[ProcessLen * procparamlen + 1] = 100;
            iparams[ProcessLen * procparamlen + 2] = app->width;
            iparams[ProcessLen * procparamlen + 3] = app->height;
            ProcessLen++;
        }
        last_key = Scancode;
    } else if (!ctrl_pressed) {
        last_key = 0;
    }

    VBEInfoBlock* VBE = (VBEInfoBlock*) VBEInfoAddress;
    
    int taskbar_width = 60; 
    DrawRect(0, 0, taskbar_width, VBE->y_resolution, 160, 160, 160);
    DrawRectGradient(2, 2, taskbar_width - 4, VBE->y_resolution - 4, 160, 160, 160, 120, 115, 125);
    DrawRectAlpha(taskbar_width, 0, 2, VBE->y_resolution, 20, 20, 20, 90);
    DrawRect(taskbar_width + 2, 0, 2, VBE->y_resolution, 105, 95, 115);

    int y_offset = 10;  
    int button_height = 40;
    int marginal = 25;

    int *app_counter = &iparams[process_inst * procparamlen + 4];
    
    int active_app_count = 0;
    for (int i = 0; i < ProcessLen; i++) {
        if (i > 2 && process[i].priority >= 0) {
            active_app_count++;
        }
    }
    
    if (active_app_count == 0) {
        *app_counter = 1;
    }
    
    if (registered_apps > 0) {
        DrawAppButton(5, y_offset, taskbar_width - 10, button_height, &app_registry[0], process_inst, app_counter);
        DrawIconBrand(-2, y_offset + 4, 2, 2, 170, 160, 180, 1);
        y_offset += button_height + marginal;
        
        for (int i = 1; i < registered_apps; i++) {
            DrawAppButton(5, y_offset, taskbar_width - 10, button_height, &app_registry[i], process_inst, app_counter);
            y_offset += button_height + marginal;
        }
    }

    DrawIconBrand(-2, VBE->y_resolution - 42, 2, 2, 70, 60, 80, 0);
    return 0;
}