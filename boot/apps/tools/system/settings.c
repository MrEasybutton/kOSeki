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

    char ProgramTitle[] = "Settings";
    DrawText(getFontCharacter, font_font_width, font_font_height, ProgramTitle, x + 5, y, 0, 0, 0);

    if (closeClicked == true)
        CloseProcess(process_inst);

    char text[] = "Dark\0";
    char text1[] = "Light\0";
    char refresh[] = "Refresh Screen\0";

    char BG_Label[] = "Background Theme";
    DrawText(getFontCharacter, font_font_width, font_font_height, BG_Label, x + 10, y + 30, 200, 200, 200);

    if (
        DrawButton(x + 10, y + 60, 60, 30, 100, 80, 110,
            text, 200, 180, 210, process_inst, 200, 180, 210
    ) == true) {
        bg_r = 109;
        bg_g = 88;
        bg_b = 119;
    }

    if (
        DrawButton(x + 120, y + 60, 60, 30, 100, 80, 110,
            text1, 200, 180, 210, process_inst, 200, 180, 210
    ) == true) {
        bg_r = 200;
        bg_g = 182;
        bg_b = 201;
    }

    if (
        DrawButton(x + 10, y + 100, 170, 30, 100, 80, 110,
            refresh, 200, 180, 210, process_inst, 200, 180, 210
    ) == true) {
        Clear(0,0,0);
    }



    return 0;
}