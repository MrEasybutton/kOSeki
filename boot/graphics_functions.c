// draws the exit button. It checks for mouse focus to prevent accidental window closure.
int DrawExitButton(int x, int y, int width, int height, int r, int g, int b, int process_inst) {
    int button_hover = (curr_mouse_target == process_inst && mx > x && mx < x + width && my > y && my < y + height);
    char closetext[] = "X\0";
    
    if (button_hover) {
        
        DrawRect(x, y, width, height, r-80, g, b); 
        DrawRect(x + 1, y + 1, width-1, height-1, r + 20, g + 20, b + 20); 
        DrawText(getFontCharacter, font_font_width, font_font_height, closetext, x + width / 10, y + height / 10, 200, 200, 200);
    } else {
        
        DrawRect(x, y, width, height, r-20, g, b); 
        DrawRect(x, y, width-2, height-2, r + 40, g + 40, b + 40); 
    }

    if (button_hover && left_clicked == TRUE) {
        left_clicked = FALSE;
        return 1; 
    }

    return 0;
}

// Draws normal button with custom text
int DrawButton(int x, int y, int width, int height, int r, int g, int b, char* text, int r1, int g1, int b1, int process_inst, int r2, int g2, int b2) {
    int button_hover = (curr_mouse_target == process_inst && mx > x && mx < x + width && my > y && my < y + height);

    if (button_hover) {
        DrawRect(x + 2, y + 2, width - 2, height - 2, r - 50, g - 20, b - 20); 
        if (!left_clicked) {
            DrawRect(x + 1, y + 1, width-2, height-2, r + 20, g + 20, b + 20); 
        } else {
            DrawRect(x + 2, y + 2, width-2, height-2, r + 20, g + 20, b + 20); 
        }
    } else {
        DrawRect(x + 4, y + 4, width - 4, height - 4, r - 80, g - 20, b - 20); 
        DrawRect(x, y, width - 2, height - 2, r, g, b); 
    }

    
    DrawText(getFontCharacter, font_font_width, font_font_height, text, x + width / 10, y + height / 10, r1, g1, b1);

    if (button_hover && left_clicked == TRUE) {
        left_clicked = FALSE;
        return 1; 
    }

    return 0;
}

// Implemented custom function to calculate the string length.
int strlen(const char* str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

// Drawr window with an exit button. 
int DrawWindow(int* x, int* y, int* width, int* height, int r, int g, int b, int* mouse_held, int process_inst) {
    VBEInfoBlock* VBE = (VBEInfoBlock*) VBEInfoAddress;
    if (left_clicked == FALSE)
        *mouse_held = FALSE;

    if (curr_mouse_target == process_inst && (*mouse_held == TRUE || 
        (left_clicked == TRUE && mx > *x &&
         mx < *x + *width - 30 &&
         my > *y &&
         my < *y + 20))) {
             left_clicked = FALSE;

             *mouse_held = TRUE;
             *x = mx - (*width / 2);
             *y = my - 10;
    }

    if (*x < 65)
        *x = 65;
    else if (*x + *width > VBE->x_resolution)
        *x = (VBE->x_resolution) - (*width + 10);

    if (*y < 5)
        *y = 5;
    else if (*y + *height > VBE->y_resolution)
        *y = (VBE->y_resolution) - (*height + 10);

    DrawRect(*x + 4, *y + 2, *width, *height + 22, 38, 36, 40); 
    DrawRect(*x, *y, *width, 19, 200, 200, 200);
    DrawRect(*x, *y + 20, *width, *height, r, g, b);

    if (right_clicked == TRUE)
        DrawRect(*x + 30, *y + 30, *width / 2, *height - 20, 10, 10, 10); 
    

    return DrawExitButton(*x + *width - 20, *y, 20, 20, 175, 0, 0, process_inst);
}

