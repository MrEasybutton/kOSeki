#include "time.h"
#include "graphics.h"

int Clock(int process_inst);

static char time_buffer[9];
static char date_buffer[11];
static RTCTime current_time;

int Clock(int process_inst) {
    VBEInfoBlock* VBE = (VBEInfoBlock*) VBEInfoAddress;
    
    read_time(&current_time);
    
    format_time(&current_time, time_buffer);
    format_date(&current_time, date_buffer);

    DrawRect(VBE->x_resolution - 102, 2, 102, 22, 160, 140, 180);
    DrawText(getFontCharacter, font_font_width, font_font_height, 
            date_buffer, VBE->x_resolution - 98, 4, 40, 30, 54);

    DrawRect(VBE->x_resolution - 85, 26, 100, 20, 160, 140, 180);
    DrawText(getFontCharacter, font_font_width, font_font_height, 
            time_buffer, VBE->x_resolution - 80, 28, 40, 30, 54);
    
    return 0;
}