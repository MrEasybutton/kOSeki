#include "../lib_include.h"
#include "../../input.h"
#include "../../graphics.h"
#include "../../filesystem.h"
#include "../../text_input.h"
#include "../../utilities.h"
#include <stdbool.h>

#define MAX_LINES 50
#define MAX_LINE_LENGTH 80
#define WIN_X         0
#define WIN_Y         1
#define WIN_WIDTH     2
#define WIN_HEIGHT    3

typedef struct {
    char lines[MAX_LINES][MAX_LINE_LENGTH];
    int line_count;
    int current_line;
    int cursor_x;
    char filename[13];
    int modified;
} NovellaState;

static NovellaState editor = {0};

static char default_filename[] = "UNTITLED.TXT";
static TextInputField* filename_input = 0;

int save_document(const char* filename) {
    int fd = fs_open(filename ? filename : default_filename, FS_MODE_WRITE | FS_MODE_CREATE);
    if (fd < 0) return -1; // fail
    
    for (int i = 0; i <= editor.line_count; i++) {
        fs_write(fd, editor.lines[i], MAX_LINE_LENGTH - 1);
        
        if (i < editor.line_count || editor.lines[editor.line_count][0] != '\0') {
            char newline = '\n';
            fs_write(fd, &newline, 1);
        }
    }
    
    fs_close(fd);
    
    int j = 0;
    while (filename[j] != '\0' && j < 12) {
        editor.filename[j] = filename[j];
        j++;
    }
    editor.filename[j] = '\0';
    editor.modified = 0;
    
    return 0;
}

int load_document(const char* filename) {
    int fd = fs_open(filename ? filename : default_filename, FS_MODE_READ);
    if (fd < 0) return -1; // fail

    editor.line_count = 0;
    editor.current_line = 0;
    editor.cursor_x = 0;
    for (int i = 0; i < MAX_LINES; i++) {
        editor.lines[i][0] = '\0';
    }

    char buffer[MAX_LINE_LENGTH];
    int buffer_pos = 0;
    char c;
    int line = 0;
    
    while (line < MAX_LINES && fs_read(fd, &c, 1) == 1) {
        if (c == '\n' || c == '\r') {
            buffer[buffer_pos] = '\0';

            int j = 0;
            while (buffer[j] != '\0' && j < MAX_LINE_LENGTH - 1) {
                editor.lines[line][j] = buffer[j];
                j++;
            }
            editor.lines[line][j] = '\0';

            line++;
            buffer_pos = 0;
            if (c == '\r') fs_read(fd, &c, 1);
        } else {
            if (buffer_pos < MAX_LINE_LENGTH - 1) {
                buffer[buffer_pos++] = c;
            }
        }
    }

    if (buffer_pos > 0 && line < MAX_LINES) {
        buffer[buffer_pos] = '\0';
        int j = 0;
        while (buffer[j] != '\0' && j < MAX_LINE_LENGTH - 1) {
            editor.lines[line][j] = buffer[j];
            j++;
        }
        editor.lines[line][j] = '\0';
        line++;
    }

    editor.line_count = line > 0 ? line - 1 : 0;
    
    if (editor.line_count >= 0) {
        editor.current_line = editor.line_count;
        editor.cursor_x = 0;
        
        while (editor.lines[editor.current_line][editor.cursor_x] != '\0' && 
               editor.cursor_x < MAX_LINE_LENGTH - 1) {
            editor.cursor_x++;
        }
    }
    
    fs_close(fd);

    int j = 0;
    while (filename[j] != '\0' && j < 12) {
        editor.filename[j] = filename[j];
        j++;
    }
    editor.filename[j] = '\0';
    editor.modified = 0;
    
    return 0;
}

void int_to_str(int num, char* str) {
    int i = 0;
    int is_negative = 0;

    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    
    while (num > 0) {
        str[i++] = (num % 10) + '0';
        num /= 10;
    }
    
    if (is_negative) {
        str[i++] = '-';
    }
    
    // reverse
    str[i] = '\0';
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

int Novella(int process_inst) {
    int *params = &iparams[process_inst * procparamlen];
    
    static int initialized = 0;
    if (!initialized) {
        initialized = 1;
        
        if (process[process_inst].i1 == 1) {
            load_document(process[process_inst].ca1);

            if (filename_input) {
                set_text_input_value(filename_input, editor.filename);
            }
        }
    }
    
    int closeClicked = DrawWindow(
        &params[WIN_X], &params[WIN_Y], &params[WIN_WIDTH], &params[WIN_HEIGHT],
        20, 12, 20, &params[9], process_inst
    );

    if (closeClicked) CloseProcess(process_inst);

    char title[] = "Novella Editor";
    DrawText(getFontCharacter, font_font_width, font_font_height, 
             title, params[WIN_X] + 5, params[WIN_Y], 0, 0, 0);

    // debug shit
    char debugKey[20] = "Key: ";
    char debugScancode[30] = "Scan: ";
    char debugShift[20] = "Shift: ";
    char debugStatus[40] = "Status: Waiting for input";
    char debugPos[30] = "Position: ";
    
    static int last_scancode = -1;
    static int key_processed = FALSE;
    
    int_to_str(Scancode, debugScancode + 6);
    int_to_str(shift_pressed, debugShift + 7);
    
    char posInfo[30];
    int_to_str(editor.current_line, posInfo);
    int posLen = 0;
    while (posInfo[posLen] != '\0') posLen++;
    posInfo[posLen++] = ',';
    posInfo[posLen] = '\0';
    int_to_str(editor.cursor_x, posInfo + posLen);
    
    int i = 0;
    while (posInfo[i] != '\0') {
        debugPos[10 + i] = posInfo[i];
        i++;
    }
    debugPos[10 + i] = '\0';
    
    int available_width = (params[WIN_WIDTH] - 20) / font_font_width;
    if (available_width > MAX_LINE_LENGTH - 1) {
        available_width = MAX_LINE_LENGTH - 1;
    }
    
    if (Scancode != last_scancode) {
        last_scancode = Scancode;
        key_processed = FALSE;
    }

    if (Scancode > 0 && !key_processed && active_input_field == 0) {
        key_processed = TRUE;
        
        char key = ProcessScancode(Scancode);
        debugKey[5] = key ? key : '-';
        
        if (Scancode == 0x48) {
            if (editor.current_line > 0) {
                editor.current_line--;
                int line_len = 0;
                while (editor.lines[editor.current_line][line_len] != '\0' && line_len < MAX_LINE_LENGTH - 1) {
                    line_len++;
                }
                if (editor.cursor_x > line_len) {
                    editor.cursor_x = line_len;
                }
            }
        } else if (Scancode == 0x50) {
            if (editor.current_line < editor.line_count) {
                editor.current_line++;
                int line_len = 0;
                while (editor.lines[editor.current_line][line_len] != '\0' && line_len < MAX_LINE_LENGTH - 1) {
                    line_len++;
                }
                if (editor.cursor_x > line_len) {
                    editor.cursor_x = line_len;
                }
            }
        } else if (Scancode == 0x4B) {
            if (editor.cursor_x > 0) {
                editor.cursor_x--;
            } else if (editor.current_line > 0) {
                editor.current_line--;
                editor.cursor_x = 0;
                while (editor.lines[editor.current_line][editor.cursor_x] != '\0' && 
                       editor.cursor_x < MAX_LINE_LENGTH - 1) {
                    editor.cursor_x++;
                }
            }
        } else if (Scancode == 0x4D) {
            if (editor.lines[editor.current_line][editor.cursor_x] != '\0' && 
                editor.cursor_x < MAX_LINE_LENGTH - 1) {
                editor.cursor_x++;
            } else if (editor.current_line < editor.line_count) {
                editor.current_line++;
                editor.cursor_x = 0;
            }
        } else if (key == 8) {
            if (editor.cursor_x > 0) {
                for (int j = editor.cursor_x - 1; j < MAX_LINE_LENGTH - 1; j++) {
                    editor.lines[editor.current_line][j] = editor.lines[editor.current_line][j+1];
                }
                editor.cursor_x--;
                editor.modified = 1;
            } else if (editor.current_line > 0) {
                editor.current_line--;
                editor.cursor_x = 0;
                while (editor.lines[editor.current_line][editor.cursor_x] != '\0' && editor.cursor_x < MAX_LINE_LENGTH - 1) {
                    editor.cursor_x++;
                }
                editor.modified = 1;
            }
        } else if (key == 10) {
            if (editor.line_count < MAX_LINES - 1) {
                for (int j = 0; j < MAX_LINE_LENGTH - 1; j++) {
                    if (editor.cursor_x + j < MAX_LINE_LENGTH) {
                        editor.lines[editor.current_line + 1][j] = editor.lines[editor.current_line][editor.cursor_x + j];
                        editor.lines[editor.current_line][editor.cursor_x + j] = '\0';
                    } else {
                        editor.lines[editor.current_line + 1][j] = '\0';
                    }
                }
                
                editor.current_line++;
                if (editor.current_line > editor.line_count) {
                    editor.line_count = editor.current_line;
                }
                editor.cursor_x = 0;
                editor.modified = 1;
            }
        } else if (key != '\0') {
            if (editor.cursor_x >= available_width) {
                int last_space = -1;
                for (int j = 0; j < editor.cursor_x; j++) {
                    if (editor.lines[editor.current_line][j] == ' ') {
                        last_space = j;
                    }
                }
                
                if (last_space != -1 && editor.line_count < MAX_LINES - 1) {
                    int idx = 0;
                    for (int j = last_space + 1; j < MAX_LINE_LENGTH && editor.lines[editor.current_line][j] != '\0'; j++) {
                        editor.lines[editor.current_line + 1][idx++] = editor.lines[editor.current_line][j];
                        editor.lines[editor.current_line][j] = '\0';
                    }
                    editor.lines[editor.current_line + 1][idx] = '\0';
                    
                    editor.current_line++;
                    if (editor.current_line > editor.line_count) {
                        editor.line_count = editor.current_line;
                    }
                    editor.cursor_x = idx;
                    
                    if (editor.cursor_x < MAX_LINE_LENGTH - 1) {
                        for (int j = MAX_LINE_LENGTH - 1; j > editor.cursor_x; j--) {
                            editor.lines[editor.current_line][j] = editor.lines[editor.current_line][j-1];
                        }
                        editor.lines[editor.current_line][editor.cursor_x++] = key;
                    }
                    editor.modified = 1;
                } else if (editor.line_count < MAX_LINES - 1) {
                    editor.current_line++;
                    if (editor.current_line > editor.line_count) {
                        editor.line_count = editor.current_line;
                    }
                    editor.cursor_x = 0;
                    editor.lines[editor.current_line][editor.cursor_x++] = key;
                    editor.lines[editor.current_line][editor.cursor_x] = '\0';
                    editor.modified = 1;
                }
            } else {
                if (editor.cursor_x < MAX_LINE_LENGTH - 1) {
                    for (int j = MAX_LINE_LENGTH - 1; j > editor.cursor_x; j--) {
                        editor.lines[editor.current_line][j] = editor.lines[editor.current_line][j-1];
                    }
                    editor.lines[editor.current_line][editor.cursor_x++] = key;
                    editor.modified = 1;
                }
            }
        }
        
        char* msg = "key processed";
        i = 0;
        while (msg[i] != '\0') {
            debugStatus[i] = msg[i];
            i++;
        }
        debugStatus[i] = '\0';
    }

    else if (active_input_field != 0) {
        char* msg = "edit filename";
        i = 0;
        while (msg[i] != '\0') {
            debugStatus[i] = msg[i];
            i++;
        }
        debugStatus[i] = '\0';
    }
    

    for (int i = 0; i <= editor.line_count; i++) {
        DrawText(getFontCharacter, font_font_width, font_font_height, 
                 editor.lines[i], 
                 params[WIN_X] + 10, 
                 params[WIN_Y] + 30 + (i * 15), 
                 240, 240, 240);
    }

    DrawRect(
        params[WIN_X] + 10 + (editor.cursor_x * font_font_width), 
        params[WIN_Y] + 30 + (editor.current_line * 15), 
        2, 
        font_font_height, 
        250, 250, 250
    );

    DrawRect(
        params[WIN_X], 
        params[WIN_Y] + 436, 
        params[WIN_WIDTH], 
        font_font_height + 6, 
        90, 30, 90
    );
    
    char suf_filename[] = "file: ";
    DrawText(getFontCharacter, font_font_width, font_font_height, 
             suf_filename, params[WIN_X] + 10, params[WIN_Y] + 438, 
             240, 240, 240);

    if (!filename_input) {
        filename_input = register_text_input_field(
            process_inst, 1,
            params[WIN_X] + 50, params[WIN_Y] + 436,
            params[WIN_WIDTH] - 80, font_font_height + 5,
            240, 240, 240,
            90, 30, 90
        );

        if (editor.filename[0] != '\0') {
            set_text_input_value(filename_input, editor.filename);
        } else {
            set_text_input_value(filename_input, default_filename);
        }
    } else {
        filename_input->input_x = params[WIN_X] + 80;
        filename_input->input_y = params[WIN_Y] + 436;
        filename_input->input_width = params[WIN_WIDTH] - 80;
    }

    draw_textfield(filename_input);
    char suf_modify[] = "*";
    if (editor.modified) {
        DrawText(getFontCharacter, font_font_width, font_font_height, 
                suf_modify, params[WIN_X] + params[WIN_WIDTH] - 20, params[WIN_Y] + 438, 
                250, 200, 200);
    }

    DrawRect(
        params[WIN_X], 
        params[WIN_Y] + 465, 
        params[WIN_WIDTH], 
        font_font_height + 25, 
        80, 20, 80
    );

    char saveText[] = "SAVE";
    if (DrawButton(params[WIN_X] + 10, params[WIN_Y] + 470, 80, 30, 100, 60, 120,
        saveText, 180, 180, 180, process_inst, 120, 80, 120) == TRUE) {
        const char* input_filename = get_text_input_value(filename_input);
        
        int j = 0;
        while (input_filename[j] != '\0' && j < 12) {
            editor.filename[j] = input_filename[j];
            j++;
        }
        editor.filename[j] = '\0';

        if (editor.filename[0] != '\0') {
            save_document(editor.filename);
        } else {
            save_document(default_filename);
        }
    }
    
    char loadText[] = "LOAD";
    if (DrawButton(params[WIN_X] + 100, params[WIN_Y] + 470, 80, 30, 100, 60, 120,
        loadText, 180, 180, 180, process_inst, 120, 80, 120) == TRUE) {
        const char* input_filename = get_text_input_value(filename_input);
        int j = 0;
        while (input_filename[j] != '\0' && j < 12) {
            editor.filename[j] = input_filename[j];
            j++;
        }
        editor.filename[j] = '\0';

        if (editor.filename[0] != '\0') {
            load_document(editor.filename);
        } else {
            load_document(default_filename);
        }
    }

    char newText[] = "NEW";
    if (DrawButton(params[WIN_X] + 190, params[WIN_Y] + 470, 80, 30, 100, 60, 120,
        newText, 180, 180, 180, process_inst, 120, 80, 120) == TRUE) {
        editor.line_count = 0;
        editor.current_line = 0;
        editor.cursor_x = 0;
        for (int i = 0; i < MAX_LINES; i++) {
            editor.lines[i][0] = '\0';
        }
        editor.modified = 0;
        editor.filename[0] = '\0';

        set_text_input_value(filename_input, default_filename);
    }

    char clearText[] = "CLEAR";
    if (DrawButton(params[WIN_X] + 280, params[WIN_Y] + 470, 80, 30, 120, 60, 125,
        clearText, 180, 180, 180, process_inst, 120, 80, 120) == TRUE) {
        editor.line_count = 0;
        editor.current_line = 0;
        editor.cursor_x = 0;
        for (int i = 0; i < MAX_LINES; i++) {
            editor.lines[i][0] = '\0';
        }
        editor.modified = 1;
    }

    return 0;
}