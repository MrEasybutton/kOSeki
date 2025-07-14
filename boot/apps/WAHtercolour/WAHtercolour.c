#include "../lib_include.h"
#include "../../input.h"
#include "../../graphics.h"
#include <stdbool.h>

#define WIN_X         0
#define WIN_Y         1
#define WIN_WIDTH     2
#define WIN_HEIGHT    3

#define MAX_SEGMENTS 5000

#define BRUSH_CIRCLE  0
#define BRUSH_SQUARE  1
#define BRUSH_SPRAY   2
#define BRUSH_STAMP   3

#define STAMP_READY   0
#define STAMP_PLACED  1

typedef struct {
    int start_x;
    int start_y;
    int end_x;
    int end_y;
    int color_r;
    int color_g;
    int color_b;
    int size;
    int brush_type;
    int stamp_index;
    bool active;
} Segment;

static Segment canvas_segments[MAX_SEGMENTS];
static int segment_count = 0;
static int current_size = 4;
static int current_color_r = 0;
static int current_color_g = 0;
static int current_color_b = 0;
static int current_brush_type = BRUSH_CIRCLE;
static int current_stamp_index = 0;
static bool is_drawing = false;
static int last_mouse_x = -1;
static int last_mouse_y = -1;
static int stamp_state = STAMP_READY;

// moves with window (duct tape fix)
static int prev_window_x = 0;
static int prev_window_y = 0;

static int next_segment_index = 0;

void DrawCircleBrush(int x, int y, int thickness, int r, int g, int b) {
    DrawCircle(x, y, thickness, r, g, b);
}

void DrawSquareBrush(int x, int y, int thickness, int r, int g, int b) {
    DrawRect(x - thickness, y - thickness, thickness * 2, thickness * 2, r, g, b);
}

void DrawSprayBrush(int x, int y, int thickness, int r, int g, int b) {
    int spray_area = thickness * 3;
    int num_particles = thickness * 6;
    
    for (int i = 0; i < num_particles; i++) {
        int seed = x * 73856093 ^ y * 19349663 ^ i * 83492791;
        int offset_x = srand_clamp(spray_area * 2, seed) - spray_area;
        int offset_y = srand_clamp(spray_area * 2, seed + 1) - spray_area;

        if (offset_x*offset_x + offset_y*offset_y <= spray_area*spray_area) {
            DrawPixel(x + offset_x, y + offset_y, r, g, b);
        }
    }
}

void DrawStampBrush(int x, int y, int thickness, int r, int g, int b, int stamp_index) {
    int scale = thickness / 2;
    if (scale < 1) scale = 1;
    DrawIconBrand(x - (11 * scale/2), y - (8 * scale/2), scale, scale, r, g, b, stamp_index);
}

void DrawBrush(int x, int y, int thickness, int r, int g, int b, int brush_type, int stamp_index) {
    switch (brush_type) {
        case BRUSH_CIRCLE:
            DrawCircleBrush(x, y, thickness, r, g, b);
            break;
        case BRUSH_SQUARE:
            DrawSquareBrush(x, y, thickness, r, g, b);
            break;
        case BRUSH_SPRAY:
            DrawSprayBrush(x, y, thickness, r, g, b);
            break;
        case BRUSH_STAMP:
            DrawStampBrush(x, y, thickness, r, g, b, stamp_index);
            break;
    }
}

void DrawLine(int x1, int y1, int x2, int y2, int thickness, int r, int g, int b, int brush_type, int stamp_index) {
    if (brush_type == BRUSH_STAMP) {
        DrawBrush(x2, y2, thickness, r, g, b, brush_type, stamp_index);
        return;
    }
    
    int dx = x2 - x1;
    int dy = y2 - y1;
    int steps = (abs(dx) > abs(dy)) ? abs(dx) : abs(dy);
    if (steps == 0) steps = 1;
    
    float x_increment = dx / (float)steps;
    float y_increment = dy / (float)steps;
    
    float x = x1;
    float y = y1;
    
    for (int i = 0; i <= steps; i++) {
        DrawBrush((int)x, (int)y, thickness, r, g, b, brush_type, stamp_index);
        x += x_increment;
        y += y_increment;
    }
}

int WAHtercolour(int process_inst) {
    int *params = &iparams[process_inst * procparamlen];

    int window_x_offset = params[WIN_X] - prev_window_x;
    int window_y_offset = params[WIN_Y] - prev_window_y;
    prev_window_x = params[WIN_X];
    prev_window_y = params[WIN_Y];

    int closeClicked = DrawWindow(
        &params[WIN_X], &params[WIN_Y], &params[WIN_WIDTH], &params[WIN_HEIGHT],
        121, 101, 121, &params[9], process_inst
    );

    if (closeClicked) CloseProcess(process_inst);

    char title[] = "WAHtercolour";
    DrawText(getFontCharacter, font_font_width, font_font_height, 
             title, params[WIN_X] + 5, params[WIN_Y], 0, 0, 0);

    int canvas_x = params[WIN_X] + 10;
    int canvas_y = params[WIN_Y] + 30;
    int canvas_width = params[WIN_WIDTH] - 120;
    int canvas_height = params[WIN_HEIGHT] - 40;

    DrawRect(canvas_x, canvas_y, canvas_width, canvas_height, 255, 255, 255);
    
    int toolbar_x = canvas_x + canvas_width + 6;
    int toolbar_y = canvas_y;
    int toolbar_width = 100;
    int toolbar_height = canvas_height;
    DrawRect(toolbar_x, toolbar_y, toolbar_width, toolbar_height, 204, 172, 152);
    
    int palette_y = toolbar_y + 10;
    int color_size = 20;
    int color_spacing = 5;
    
    char label_pal_r[] = "R", label_pal_g[] = "G", label_pal_b[] = "B";
    DrawRectAlpha(toolbar_x + 5, toolbar_y + 5, toolbar_width - 10, 70, 104, 102, 106, 160);
    DrawText(getFontCharacter, font_font_width, font_font_height, label_pal_r, toolbar_x + 10, palette_y - 5, 200, 0, 0);
    DrawSlider(toolbar_x + 20, palette_y, toolbar_width - 35, 10, 0, 255, &current_color_r, 150, 90, 120, process_inst);
    DrawText(getFontCharacter, font_font_width, font_font_height, label_pal_g, toolbar_x + 10, palette_y + 20, 0, 200, 0);
    DrawSlider(toolbar_x + 20, palette_y + 25, toolbar_width - 35, 10, 0, 255, &current_color_g, 100, 140, 120, process_inst);
    DrawText(getFontCharacter, font_font_width, font_font_height, label_pal_b, toolbar_x + 10, palette_y + 45, 0, 0, 200);
    DrawSlider(toolbar_x + 20, palette_y + 50, toolbar_width - 35, 10, 0, 255, &current_color_b, 100, 90, 170, process_inst);
    
    int size_y = palette_y + color_size + 50;
    char size_label[] = "SIZE:";
    DrawText(getFontCharacter, font_font_width, font_font_height, 
             size_label, toolbar_x + 10, size_y, 0, 0, 0);
    
    size_y += 20;

    DrawSlider(toolbar_x + 10, size_y + 10, toolbar_width - 25, 10, 1, 10, &current_size, 100, 90, 120, process_inst);
    
    int brush_y = size_y + 25;
    char brush_label[] = "BRUSH:";
    DrawText(getFontCharacter, font_font_width, font_font_height, 
             brush_label, toolbar_x + 10, brush_y, 0, 0, 0);
    
    brush_y += 20;
    
    char circle_label[] = "O";
    int highlight_r = current_brush_type == BRUSH_CIRCLE ? 230 : 180;
    int highlight_g = current_brush_type == BRUSH_CIRCLE ? 230 : 180;
    int highlight_b = current_brush_type == BRUSH_CIRCLE ? 250 : 180;
    if (DrawButton(toolbar_x + 10, brush_y, 25, 25, 
                  highlight_r, highlight_g, highlight_b, circle_label, 0, 0, 0, process_inst, 200, 200, 220) == true) {
        current_brush_type = BRUSH_CIRCLE;
    }
    
    char square_label[] = "[]";
    highlight_r = current_brush_type == BRUSH_SQUARE ? 230 : 180;
    highlight_g = current_brush_type == BRUSH_SQUARE ? 230 : 180;
    highlight_b = current_brush_type == BRUSH_SQUARE ? 250 : 180;
    if (DrawButton(toolbar_x + 40, brush_y, 25, 25, 
                  highlight_r, highlight_g, highlight_b, square_label, 0, 0, 0, process_inst, 200, 200, 220) == true) {
        current_brush_type = BRUSH_SQUARE;
    }
    
    char spray_label[] = "*";
    highlight_r = current_brush_type == BRUSH_SPRAY ? 230 : 180;
    highlight_g = current_brush_type == BRUSH_SPRAY ? 230 : 180;
    highlight_b = current_brush_type == BRUSH_SPRAY ? 250 : 180;
    if (DrawButton(toolbar_x + 70, brush_y, 25, 25, 
                  highlight_r, highlight_g, highlight_b, spray_label, 0, 0, 0, process_inst, 200, 200, 220) == true) {
        current_brush_type = BRUSH_SPRAY;
    }

    int stamp_y = brush_y + 40;
    char stamp_label[] = "STAMPS:";
    DrawText(getFontCharacter, font_font_width, font_font_height, 
             stamp_label, toolbar_x + 10, stamp_y, 0, 0, 0);
    
    stamp_y += 20;
    
    highlight_r = current_brush_type == BRUSH_STAMP && current_stamp_index == 0 ? 230 : 180;
    highlight_g = current_brush_type == BRUSH_STAMP && current_stamp_index == 0 ? 230 : 180;
    highlight_b = current_brush_type == BRUSH_STAMP && current_stamp_index == 0 ? 250 : 180;
    if (DrawButton(toolbar_x + 10, stamp_y, 25, 25, 
                  highlight_r, highlight_g, highlight_b, "", 0, 0, 0, process_inst, 200, 200, 220) == true) {
        current_brush_type = BRUSH_STAMP;
        current_stamp_index = 0;
    }
    DrawIconBrand(toolbar_x + 5, stamp_y + 3, 1, 1, 0, 0, 0, 0);
    
    highlight_r = current_brush_type == BRUSH_STAMP && current_stamp_index == 2 ? 230 : 180;
    highlight_g = current_brush_type == BRUSH_STAMP && current_stamp_index == 2 ? 230 : 180;
    highlight_b = current_brush_type == BRUSH_STAMP && current_stamp_index == 2 ? 250 : 180;
    if (DrawButton(toolbar_x + 40, stamp_y, 25, 25, 
                  highlight_r, highlight_g, highlight_b, "", 0, 0, 0, process_inst, 200, 200, 220) == true) {
        current_brush_type = BRUSH_STAMP;
        current_stamp_index = 2;
    }
    DrawIconBrand(toolbar_x + 35, stamp_y + 3, 1, 1, 0, 0, 0, 2);

    highlight_r = current_brush_type == BRUSH_STAMP && current_stamp_index == 3 ? 230 : 180;
    highlight_g = current_brush_type == BRUSH_STAMP && current_stamp_index == 3 ? 230 : 180;
    highlight_b = current_brush_type == BRUSH_STAMP && current_stamp_index == 3 ? 250 : 180;
    if (DrawButton(toolbar_x + 70, stamp_y, 25, 25, 
                  highlight_r, highlight_g, highlight_b, "", 0, 0, 0, process_inst, 200, 200, 220) == true) {
        current_brush_type = BRUSH_STAMP;
        current_stamp_index = 3;
    }
    DrawIconBrand(toolbar_x + 65, stamp_y + 3, 1, 1, 0, 0, 0, 3);
    
    int clear_y = stamp_y + 40;
    char clear_label[] = "Clear";
    if (DrawButton(toolbar_x + 10, clear_y, 80, 30, 
                  210, 150, 150, clear_label, 0, 0, 0, process_inst, 230, 160, 160) == true) {
        segment_count = 0;
        next_segment_index = 0;
    }
    
    int preview_y = clear_y + 50;
    char current_label[] = "CURRENT:";
    DrawText(getFontCharacter, font_font_width, font_font_height, 
             current_label, toolbar_x + 10, preview_y, 0, 0, 0);
    
    preview_y += 20;
    DrawRect(toolbar_x + 10, preview_y, 80, 30, current_color_r, current_color_g, current_color_b);

    for (int i = 0; i < segment_count; i++) {
        if (canvas_segments[i].active) {
            int abs_start_x = params[WIN_X] + canvas_segments[i].start_x;
            int abs_start_y = params[WIN_Y] + canvas_segments[i].start_y;
            int abs_end_x = params[WIN_X] + canvas_segments[i].end_x;
            int abs_end_y = params[WIN_Y] + canvas_segments[i].end_y;
            
            DrawLine(
                abs_start_x, abs_start_y,
                abs_end_x, abs_end_y,
                canvas_segments[i].size,
                canvas_segments[i].color_r, 
                canvas_segments[i].color_g, 
                canvas_segments[i].color_b,
                canvas_segments[i].brush_type,
                canvas_segments[i].stamp_index
            );

        }
    }
    
    if (mx > canvas_x && mx < canvas_x + canvas_width &&
        my > canvas_y && my < canvas_y + canvas_height) {

        if (current_brush_type != BRUSH_STAMP || (current_brush_type == BRUSH_STAMP && stamp_state == STAMP_READY)) {
            DrawBrush(mx, my, current_size, current_color_r, current_color_g, current_color_b, 
                     current_brush_type, current_stamp_index);
        }
        
        if (left_clicked) {
            if (current_brush_type == BRUSH_STAMP) {
                

                if (stamp_state == STAMP_READY) {
                    if (segment_count < MAX_SEGMENTS) {
                        canvas_segments[next_segment_index].start_x = mx - params[WIN_X];
                        canvas_segments[next_segment_index].start_y = my - params[WIN_Y];
                        canvas_segments[next_segment_index].end_x = mx - params[WIN_X];
                        canvas_segments[next_segment_index].end_y = my - params[WIN_Y];
                        canvas_segments[next_segment_index].color_r = current_color_r;
                        canvas_segments[next_segment_index].color_g = current_color_g;
                        canvas_segments[next_segment_index].color_b = current_color_b;
                        canvas_segments[next_segment_index].size = current_size;
                        canvas_segments[next_segment_index].brush_type = current_brush_type;
                        canvas_segments[next_segment_index].stamp_index = current_stamp_index;
                        canvas_segments[next_segment_index].active = true;

                        next_segment_index = (next_segment_index + 1) % MAX_SEGMENTS;
                        if (segment_count < MAX_SEGMENTS) {
                            segment_count++;
                        }
                        
                        DrawStampBrush(mx, my, current_size, current_color_r, current_color_g, current_color_b, current_stamp_index);
                        
                        stamp_state = STAMP_PLACED;
                    }
                }
            }
            else {
                if (!is_drawing) {
                    is_drawing = true;
                    last_mouse_x = mx;
                    last_mouse_y = my;
                } else if (last_mouse_x != -1 && last_mouse_y != -1) {
                    if (segment_count < MAX_SEGMENTS) {
                        canvas_segments[next_segment_index].start_x = last_mouse_x - params[WIN_X];
                        canvas_segments[next_segment_index].start_y = last_mouse_y - params[WIN_Y];
                        canvas_segments[next_segment_index].end_x = mx - params[WIN_X];
                        canvas_segments[next_segment_index].end_y = my - params[WIN_Y];
                        canvas_segments[next_segment_index].color_r = current_color_r;
                        canvas_segments[next_segment_index].color_g = current_color_g;
                        canvas_segments[next_segment_index].color_b = current_color_b;
                        canvas_segments[next_segment_index].size = current_size;
                        canvas_segments[next_segment_index].brush_type = current_brush_type;
                        canvas_segments[next_segment_index].stamp_index = current_stamp_index;
                        canvas_segments[next_segment_index].active = true;

                        next_segment_index = (next_segment_index + 1) % MAX_SEGMENTS;
                        if (segment_count < MAX_SEGMENTS) {
                            segment_count++;
                        }
                    } else {
                        canvas_segments[next_segment_index].start_x = last_mouse_x - params[WIN_X];
                        canvas_segments[next_segment_index].start_y = last_mouse_y - params[WIN_Y];
                        canvas_segments[next_segment_index].end_x = mx - params[WIN_X];
                        canvas_segments[next_segment_index].end_y = my - params[WIN_Y];
                        canvas_segments[next_segment_index].color_r = current_color_r;
                        canvas_segments[next_segment_index].color_g = current_color_g;
                        canvas_segments[next_segment_index].color_b = current_color_b;
                        canvas_segments[next_segment_index].size = current_size;
                        canvas_segments[next_segment_index].brush_type = current_brush_type;
                        canvas_segments[next_segment_index].stamp_index = current_stamp_index;
                        canvas_segments[next_segment_index].active = true;
                        
                        next_segment_index = (next_segment_index + 1) % MAX_SEGMENTS;
                    }
                    
                    DrawLine(
                        last_mouse_x, last_mouse_y,
                        mx, my,
                        current_size,
                        current_color_r, current_color_g, current_color_b,
                        current_brush_type, current_stamp_index
                    );
                }
                
                last_mouse_x = mx;
                last_mouse_y = my;
            }
        } else {
            is_drawing = false;
            
            if (current_brush_type == BRUSH_STAMP) {
                stamp_state = STAMP_READY;
            }
        }
    } else {
        is_drawing = false;
        if (current_brush_type == BRUSH_STAMP) {
            stamp_state = STAMP_READY;
        }
    }
    
    return 0;
}