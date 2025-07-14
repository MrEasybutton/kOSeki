#include "graphics.h"
#include <stdbool.h>

// this is very extra, but maybe its indicative of system performance???
void STARTUP() {
    VBEInfoBlock* VBE = (VBEInfoBlock*) VBEInfoAddress;
    int screenWidth = VBE->x_resolution;
    int screenHeight = VBE->y_resolution;
    
    ClearScreenGradient(202, 188, 210, 212, 165, 218);
    DrawIconBrand(screenWidth / 2 - 220, screenHeight / 2 - 160, 14, 14, 205, 185, 210, 2);
    DrawIconBrand(screenWidth / 2 - 220, screenHeight / 2 - 160, 14, 14, 40, 30, 50, 0);

    char beebstitle[] = "BIBOO is now preparing, please wait warmly";
    DrawText(getFontCharacter, font_font_width, font_font_height, 
             beebstitle, screenWidth / 2 - 200, screenHeight / 2 + 80, 20, 0, 24);
    
    int barWidth = 300;
    int barHeight = 10;
    int barX = (screenWidth - barWidth) / 2;
    int barY = screenHeight / 2 + 120;
    
    DrawRectAlpha(barX - 10, barY - 10, barWidth + 20, barHeight + 20, 120, 80, 160, 40);
    DrawRectAlpha(barX - 6, barY - 6, barWidth + 12, barHeight + 12, 120, 80, 160, 80);
    
    DrawRect(barX - 2, barY - 2, barWidth + 4, barHeight + 4, 120, 90, 140);

    for (volatile int i = 0; i < 500000; i++) {}

    for (int progress = 0; progress <= barWidth; progress += 5) {
        DrawRectAlpha(barX + progress - 20, barY - 5, 40, barHeight + 10, 220, 180, 240, 20);
        
        for (int i = 0; i < barHeight; i++) {
            int ratio = (i * 100) / barHeight;
            int r = 200 + ((ratio * 40) / 100);
            int g = 180 + ((ratio * 30) / 100);
            int b = 240 - ((ratio * 30) / 100);
            DrawRect(barX, barY + i, progress, 1, r, g, b);
        }
        Refresh();
        
        for (volatile int i = 0; i < 500000; i++) {}
    }

    char beebstitle_inv[] = "BIBOO is now preparing, please wait warmly";
    DrawText(getFontCharacter, font_font_width, font_font_height, 
        beebstitle_inv, screenWidth / 2 - 200, screenHeight / 2 + 80, 212, 165, 218);
    
    char kirakira[] = "KIRA KIRA KOSEKI!";
    DrawText(getFontCharacter, font_font_width, font_font_height, 
             kirakira, screenWidth / 2 - 95, barY - 40, 40, 30, 54);

    Refresh();
    for (volatile int i = 0; i < 10000 * 10000; i++) {}
}
