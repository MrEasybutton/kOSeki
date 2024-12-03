#include "../../lib_include.h"
#include <stdint.h>
#include <stdbool.h>


typedef struct {
    uint32_t stopwatch_time_seconds; 
    uint32_t tick_counter;           // Temporary (it lags)
    bool stopwatch_running;          
} StopwatchData;

int Stopwatch(int process_inst) {
    int x = iparams[process_inst * procparamlen + 0];
    int y = iparams[process_inst * procparamlen + 1];
    int width = iparams[process_inst * procparamlen + 2];
    int height = iparams[process_inst * procparamlen + 3];
    int* r = &iparams[process_inst * procparamlen + 4];
    int* g = &iparams[process_inst * procparamlen + 5];
    int* b = &iparams[process_inst * procparamlen + 6];

    static StopwatchData stopwatchData[7] = {0}; 
    StopwatchData* data = &stopwatchData[process_inst];


    int closeClicked = DrawWindow(
        &iparams[process_inst * procparamlen + 0],
        &iparams[process_inst * procparamlen + 1],
        &iparams[process_inst * procparamlen + 2],
        &iparams[process_inst * procparamlen + 3],
        *r, *g, *b,
        &iparams[process_inst * procparamlen + 9],
        process_inst);

    if (closeClicked == true)
        CloseProcess(process_inst);

    
    char ProgramTitle[] = "Kronii's Stopwatch";
    DrawText(getFontCharacter, font_font_width, font_font_height, ProgramTitle, x + 5, y, 0, 0, 0);
    
    if (data->stopwatch_running) {
        if (++data->tick_counter >= 10) { // Again, temporary. Works best with 1 window open. Do you reckon decreasing the tick threshold with every subsequent open window will work?
            data->tick_counter = 0;      
            data->stopwatch_time_seconds++; 
        }
    }

    
    int seconds = data->stopwatch_time_seconds % 60;
    int minutes = (data->stopwatch_time_seconds / 60) % 60;
    int hours = (data->stopwatch_time_seconds / 3600);

    
    char timeString[9] = "00:00:00";
    timeString[0] = '0' + (hours / 10);
    timeString[1] = '0' + (hours % 10);
    timeString[3] = '0' + (minutes / 10);
    timeString[4] = '0' + (minutes % 10);
    timeString[6] = '0' + (seconds / 10);
    timeString[7] = '0' + (seconds % 10);

    
    DrawText(getFontCharacter, font_font_width * 2, font_font_height * 2, timeString, x + 60, y + 35, 200, 180, 210);

    
    char startStopText[] = "Start/Stop";
    if (
        DrawButton(x + 10, y + height - 20, 120, 30, 100, 80, 110,
            startStopText, 200, 180, 210, process_inst, 200, 180, 210
        ) == true) {
        data->stopwatch_running = !data->stopwatch_running; 
    }

    
    char resetText[] = "Reset";
    if (
        DrawButton(x + 160, y + height - 20, 60, 30, 100, 80, 110,
            resetText, 200, 180, 210, process_inst, 200, 180, 210
        ) == true) {
        data->stopwatch_time_seconds = 0;   
        data->stopwatch_running = false;   
        data->tick_counter = 0;            
    }

    DrawCircle(x + 140, y + 160, 100, 120, 120, 120);
    if (data->stopwatch_running) {
        DrawCircle(x + 140, y + 160, 95, 0, 255, 0);
        char state_count[] = "Playing";
        DrawText(getFontCharacter, font_font_width, font_font_height, state_count, x + 110, y + 160, 20, 18, 21);
    } else {
        DrawCircle(x + 140, y + 160, 95, 255, 0, 0);
        char state_count[] = "Stopped";
        DrawText(getFontCharacter, font_font_width, font_font_height, state_count, x + 110, y + 160, 200, 180, 210);
    }

    return 0;
}
