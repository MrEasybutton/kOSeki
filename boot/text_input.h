#ifndef TEXT_INPUT_H
#define TEXT_INPUT_H

typedef struct {
    char buffer[128];
    int buffer_size;
    int cursor_position;
    int input_x;
    int input_y;
    int input_width;
    int input_height;
    int text_color_r;
    int text_color_g;
    int text_color_b;
    int bg_color_r;
    int bg_color_g;
    int bg_color_b;
    int focused;
    int process_id;
    int field_id;
} TextInputField;

// Global state for text input management
extern TextInputField* active_input_field;
extern int text_input_initialized;

// Initialize the text input system
void init_text_input_system();

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
);

// Draw a text input field
void draw_textfield(TextInputField* field);

// Process keyboard input for the active text input field
void process_text_input(int scancode, int shift_pressed);

// Check if a point is inside a text input field
int is_point_in_text_input_field(TextInputField* field, int x, int y);

// Set focus to a text input field
void focus_text_input_field(TextInputField* field);

// Remove focus from all text input fields
void clear_text_input_focus();

const char* get_text_input_value(TextInputField* field);
void set_text_input_value(TextInputField* field, const char* value);

#endif