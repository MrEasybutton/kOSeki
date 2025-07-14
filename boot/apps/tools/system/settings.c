#include "../../lib_include.h"
#include <stdbool.h>


int Settings(int process_inst) {
    int* r = &iparams[process_inst * procparamlen + 4];
    int* g = &iparams[process_inst * procparamlen + 5];
    int* b = &iparams[process_inst * procparamlen + 6];
    int closeClicked = DrawWindow(
        &iparams[process_inst * procparamlen + 0],
        &iparams[process_inst * procparamlen + 1],
        &iparams[process_inst * procparamlen + 2],
        &iparams[process_inst * procparamlen + 3],
        *r,
        *g,
        *b,
        &iparams[process_inst * procparamlen + 9],
        process_inst);

    int x = iparams[process_inst * procparamlen + 0];
    int y = iparams[process_inst * procparamlen + 1];
    int width = iparams[process_inst * procparamlen + 2];
    int height = iparams[process_inst * procparamlen + 3];
    extern int mspeed;

    char ProgramTitle[] = "Customize your Biboo";
    DrawText(getFontCharacter, font_font_width, font_font_height, ProgramTitle, x + 5, y, 0, 0, 0);

    if (closeClicked == true)
        CloseProcess(process_inst);

    char dark[] = "81800\0";
    char light[] = ":D\0";
    char mouse_sens[] = "Mouse Sensitivity";
    char low[] = "LOW", high[] = "HIGH";
    char info[] = "kOSeki MOAI Edition (v2.0)\0";
    char refresh[] = "REFRESH SCREEN\0";

    char BG_Label[] = "Background Theme";
    DrawText(getFontCharacter, font_font_width, font_font_height, BG_Label, x + 10, y + 30, 200, 200, 200);

    if (
        DrawButton(x + 10, y + 60, 60, 30, 80, 70, 100,
            dark, 200, 180, 210, process_inst, 200, 180, 210
    ) == true) {
        bg_r1 = 80; bg_g1 = 72; bg_b1 = 86; bg_r2 = 68; bg_g2 = 52; bg_b2 = 90;
    }

    if (
        DrawButton(x + 80, y + 60, 30, 30, 90, 80, 100,
            light, 200, 180, 210, process_inst, 200, 180, 210
    ) == true) {
        bg_r1 = 204; bg_g1 = 190; bg_b1 = 212; bg_r2 = 210; bg_g2 = 182; bg_b2 = 216;
    }

    if (
        DrawButton(x + 10, y + 100, 170, 30, 100, 80, 110,
            refresh, 200, 180, 210, process_inst, 200, 180, 210
    ) == true) {
        Clear(0,0,0);
    }

    DrawText(getFontCharacter, font_font_width, font_font_height, mouse_sens, x + 10, y + 140, 205, 200, 210);

    DrawText(getFontCharacter, font_font_width, font_font_height, low, x + 10, y + 160, 200, 180, 210);
    DrawText(getFontCharacter, font_font_width, font_font_height, high, x + width - 50, y + 160, 200, 180, 210);
    
    DrawSlider(x + 10, y + 180, width - 25, 20, 1, 10, &mspeed, 100, 90, 120, process_inst);
    
    char speed_value[4];
    speed_value[0] = (mspeed / 10) + '0';
    speed_value[1] = (mspeed % 10) + '0';
    speed_value[2] = '\0';
    
    if (speed_value[0] == '0') {
        speed_value[0] = speed_value[1];
        speed_value[1] = '\0';
    }
    
    DrawText(getFontCharacter, font_font_width, font_font_height, speed_value, 
             x + width / 2, y + 160, 230, 230, 230);

    DrawText(getFontCharacter, font_font_width, font_font_height, info, x + 10, y + 240, 205, 200, 210);

    return 0;
}