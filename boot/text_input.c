// this file is coded by Claude cause I'm lazy. come back to it when I have the time

#include "text_input.h"
#include "graphics.h"
#include "input.h"
#include <stdbool.h>

#define MAX_TEXT_INPUT_FIELDS 32

TextInputField text_input_fields[MAX_TEXT_INPUT_FIELDS];
TextInputField* active_input_field = 0;
int text_input_initialized = 0;
int text_input_field_count = 0;

void init_text_input_system() {
    if (!text_input_initialized) {
        for (int i = 0; i < MAX_TEXT_INPUT_FIELDS; i++) {
            text_input_fields[i].buffer[0] = '\0';
            text_input_fields[i].buffer_size = 0;
            text_input_fields[i].cursor_position = 0;
            text_input_fields[i].focused = 0;
            text_input_fields[i].process_id = -1;
            text_input_fields[i].field_id = -1;
        }
        
        text_input_field_count = 0;
        active_input_field = 0;
        text_input_initialized = 1;
    }
}

TextInputField* register_text_input_field(
    int process_id, 
    int field_id,
    int x, 
    int y, 
    int width, 
    int height, 
    int text_r, 
    int text_g, 
    int text_b,
    int bg_r, 
    int bg_g, 
    int bg_b
) {
    if (!text_input_initialized) {
        init_text_input_system();
    }
    
    for (int i = 0; i < text_input_field_count; i++) {
        if (text_input_fields[i].process_id == process_id && 
            text_input_fields[i].field_id == field_id) {
            
            text_input_fields[i].input_x = x;
            text_input_fields[i].input_y = y;
            text_input_fields[i].input_width = width;
            text_input_fields[i].input_height = height;
            text_input_fields[i].text_color_r = text_r;
            text_input_fields[i].text_color_g = text_g;
            text_input_fields[i].text_color_b = text_b;
            text_input_fields[i].bg_color_r = bg_r;
            text_input_fields[i].bg_color_g = bg_g;
            text_input_fields[i].bg_color_b = bg_b;
            
            return &text_input_fields[i];
        }
    }
    
    if (text_input_field_count < MAX_TEXT_INPUT_FIELDS) {
        int idx = text_input_field_count++;
        
        text_input_fields[idx].buffer[0] = '\0';
        text_input_fields[idx].buffer_size = 0;
        text_input_fields[idx].cursor_position = 0;
        text_input_fields[idx].input_x = x;
        text_input_fields[idx].input_y = y;
        text_input_fields[idx].input_width = width;
        text_input_fields[idx].input_height = height;
        text_input_fields[idx].text_color_r = text_r;
        text_input_fields[idx].text_color_g = text_g;
        text_input_fields[idx].text_color_b = text_b;
        text_input_fields[idx].bg_color_r = bg_r;
        text_input_fields[idx].bg_color_g = bg_g;
        text_input_fields[idx].bg_color_b = bg_b;
        text_input_fields[idx].focused = 0;
        text_input_fields[idx].process_id = process_id;
        text_input_fields[idx].field_id = field_id;
        
        return &text_input_fields[idx];
    }
    
    return 0;
}

void draw_textfield(TextInputField* field) {
    if (!field) return;
    DrawRect(
        field->input_x,
        field->input_y,
        field->input_width,
        field->input_height,
        field->bg_color_r,
        field->bg_color_g,
        field->bg_color_b
    );
    
    if (field->focused) {
        DrawRect(
            field->input_x,
            field->input_y,
            field->input_width,
            1,
            field->text_color_r,
            field->text_color_g,
            field->text_color_b
        );
        DrawRect(
            field->input_x,
            field->input_y + field->input_height - 1,
            field->input_width,
            1,
            field->text_color_r,
            field->text_color_g,
            field->text_color_b
        );
        DrawRect(
            field->input_x,
            field->input_y,
            1,
            field->input_height,
            field->text_color_r,
            field->text_color_g,
            field->text_color_b
        );
        DrawRect(
            field->input_x + field->input_width - 1,
            field->input_y,
            1,
            field->input_height,
            field->text_color_r,
            field->text_color_g,
            field->text_color_b
        );
    }
    
    DrawText(
        getFontCharacter,
        font_font_width,
        font_font_height,
        field->buffer,
        field->input_x + 5,
        field->input_y + 5,
        field->text_color_r,
        field->text_color_g,
        field->text_color_b
    );
    
    if (field->focused) {
        int cursor_x = field->input_x + 5 + (field->cursor_position * font_font_width);
        DrawRect(
            cursor_x,
            field->input_y + 5,
            2,
            font_font_height,
            field->text_color_r,
            field->text_color_g,
            field->text_color_b
        );
    }
}

void process_text_input(int scancode, int shift_pressed) {
    if (!active_input_field) return;
    
    char key = ProcessScancode(scancode);
    
    if (key == 8) {
        if (active_input_field->cursor_position > 0) {
            for (int i = active_input_field->cursor_position - 1; i < active_input_field->buffer_size; i++) {
                active_input_field->buffer[i] = active_input_field->buffer[i + 1];
            }
            active_input_field->buffer_size--;
            active_input_field->cursor_position--;
        }
    }

    else if (key == 10) {
        active_input_field->focused = 0;
        active_input_field = 0;
    }

    else if (key >= 32 && key <= 126 && active_input_field->buffer_size < 127) {
        for (int i = active_input_field->buffer_size; i > active_input_field->cursor_position; i--) {
            active_input_field->buffer[i] = active_input_field->buffer[i - 1];
        }
        
        active_input_field->buffer[active_input_field->cursor_position] = key;
        active_input_field->buffer_size++;
        active_input_field->cursor_position++;
        active_input_field->buffer[active_input_field->buffer_size] = '\0';
    }
}

int is_point_in_text_input_field(TextInputField* field, int x, int y) {
    return (x >= field->input_x && 
            x < field->input_x + field->input_width &&
            y >= field->input_y && 
            y < field->input_y + field->input_height);
}

void focus_text_input_field(TextInputField* field) {
    if (!field) return;
    
    for (int i = 0; i < text_input_field_count; i++) {
        text_input_fields[i].focused = 0;
    }
    
    field->focused = 1;
    active_input_field = field;
    
    field->cursor_position = field->buffer_size;
}

void clear_text_input_focus() {
    for (int i = 0; i < text_input_field_count; i++) {
        text_input_fields[i].focused = 0;
    }
    active_input_field = 0;
}

const char* get_text_input_value(TextInputField* field) {
    if (!field) return "";
    return field->buffer;
}

void set_text_input_value(TextInputField* field, const char* value) {
    if (!field) return;
    
    int i = 0;
    while (value[i] != '\0' && i < 127) {
        field->buffer[i] = value[i];
        i++;
    }
    
    field->buffer[i] = '\0';
    field->buffer_size = i;
    field->cursor_position = i;
}