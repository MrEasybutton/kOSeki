#ifndef HTML_ENGINE_H
#define HTML_ENGINE_H

#include "types.h"

#define MAX_RENDER_ELEMENTS 1024
#define MAX_FORM_FIELDS 32

typedef enum {
    ELEM_TEXT,
    ELEM_NEWLINE,
    ELEM_INPUT_TEXT,
    ELEM_INPUT_SUBMIT,
    ELEM_INPUT_HIDDEN,
    ELEM_IMAGE,
} ElemType;

typedef struct {
    ElemType type;

    char* text;
    uint32 color;
    BOOL bold;
    BOOL newline; //bc
    int scale;
    char* link_url;

    char src_url[1024];
    char alt_text[128];
    int img_width;
    int img_height;
    uint32* pixels;

    uint32** frames;
    int* delays;
    int frame_count;
    int current_frame;
    uint32 last_frame_ticks;

    int form_id;
    char field_name[64];
    char field_value[256];
    char placeholder[128];
    BOOL is_password;
    int input_width;
    BOOL is_focused;
} RenderElement;

typedef struct {
    int id;
    char action[1024];
    char method[8];
} FormDescriptor;

typedef struct {
    RenderElement elements[MAX_RENDER_ELEMENTS];
    int count;

    FormDescriptor forms[MAX_FORM_FIELDS];
    int form_count;

    int focused_element;
} RenderList;

void html_parse(const char* html, RenderList* list);
void html_free_list(RenderList* list);

int html_form_encode(const RenderList* list, int form_id, char* out_buf, int out_len);

#endif