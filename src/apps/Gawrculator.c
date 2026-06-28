#include "apps/Gawrculator.h"
#include "procsys.h"
#include "gui.h"
#include "pon.h"
#include "graphics.h"
#include "vesa.h"
#include "string.h"
#include "kheap.h"
#include "fonts.h"
#include "console.h"
#include "kmath.h"
#include "utils.h"
#include "bmp.h"
#include "cmos.h"

#define COLOR_OCEAN RGB(19, 114, 156)
#define COLOR_AQUA RGB(70, 197, 212)
#define COLOR_TONGUE RGB(173, 54, 62)
#define COLOR_TEETH RGB(230, 233, 237)

#define COLOR_BG_DARK RGB(12, 68, 92)
#define COLOR_BG_LIGHT RGB(22, 100, 130)
#define COLOR_TEXT COLOR_TEETH
#define COLOR_ACCENT COLOR_OCEAN
#define COLOR_BTN_OP RGB(12, 96, 132)

#define MAX_PARTICLES 32

typedef struct {
    float x, y;
    float vx, vy;
    uint32 color;
    BOOL active;
} Particle;

typedef struct {
    char display[128];
    BOOL numisnew;
    BOOL is_undefined;
    PON_Comp* root;
    PON_Comp* display_text;

    BOOL anim_active;
    BOOL anim_impacted; //clear when typing sumn else
    float trident_x;
    float trident_y;
    float trident_speed;
    preloaded_t* trident_bmp;
    preloaded_t* gur_bmp;
    BOOL manipulated;
    Particle particles[MAX_PARTICLES];
} GawrculatorData;

typedef struct {
    const char* input;
    const char* output;
} SpecEx;

static const SpecEx special_expressions[] = {
    {"shark", "shork"},
    {"biboo", "81800"},
    {"67", "ROKU NANA!"},
    {"kOSeki", ":D"},
    {"math", ""},
    {"gura", "A"},
    {"dum", ""},
    {"/0", ""},
    {"0/0", ""},
    {NULL, NULL}
};

typedef struct {
    const char* p;
    BOOL error;
} Parser;

static double expression(Parser* p);

static double factor(Parser* p) {
    while (*p->p == ' ') p->p++;
    
    if (strncmp(p->p, "PI", 2) == 0) {
        p->p += 2;
        return (double)PI;
    }
    if (strncmp(p->p, "e", 1) == 0 && !(p->p[1] >= 'a' && p->p[1] <= 'z')) {
        p->p += 1;
        return (double)E;
    }

    if (strncmp(p->p, "ln(", 3) == 0) {
        p->p += 3;
        double val = expression(p);
        if (*p->p == ')') p->p++;
        return (val > 0) ? (double)logf((float)val) : 0;
    }
    if (strncmp(p->p, "lg(", 3) == 0) {
        p->p += 3;
        double val = expression(p);
        if (*p->p == ')') p->p++;
        return (val > 0) ? (double)log10f((float)val) : 0;
    }
    if (strncmp(p->p, "log(", 4) == 0) {
        p->p += 4;
        double base = expression(p);
        if (*p->p == ',') p->p++;
        double val = expression(p);
        if (*p->p == ')') p->p++;
        if (base > 0 && base != 1 && val > 0) {
            return (double)(logf((float)val) / logf((float)base));
        }
        return 0;
    }

    if (*p->p == '(') {
        p->p++;
        double val = expression(p);
        if (*p->p == ')') p->p++;
        return val;
    }
    if (*p->p == '-') {
        p->p++;
        return -factor(p);
    }
    
    double val = atof(p->p);

    while ((*p->p >= '0' && *p->p <= '9') || *p->p == '.') p->p++;
    return val;
}

static double term(Parser* p) {
    double val = factor(p);
    while (1) {
        while (*p->p == ' ') p->p++;
        if (*p->p == '*') {
            p->p++;
            val *= factor(p);
        } else if (*p->p == '/') {
            p->p++;
            double f = factor(p);
            if (f != 0) val /= f;
            else {
                p->error = TRUE;
                val = 0;
            }
        } else break;
    }
    return val;
}

static double expression(Parser* p) {
    double val = term(p);
    while (1) {
        while (*p->p == ' ') p->p++;
        if (*p->p == '+') {
            p->p++;
            val += term(p);
        } else if (*p->p == '-') {
            p->p++;
            val -= term(p);
        } else break;
    }
    return val;
}

static double eval_ex(const char* expr, BOOL* undefined) {
    Parser p = { expr, FALSE };
    double res = expression(&p);
    if (undefined) *undefined = p.error;
    return res;
}

static void update_display(GawrculatorData* data) {
    if (data->display_text) {
        if (data->is_undefined) {
            update_str(data->display_text, "undefined");
        } else {
            update_str(data->display_text, data->display);
        }
    }
}

static void calculate(GawrculatorData* data) {
    for (int i = 0; special_expressions[i].input != NULL; i++) {
        if (strcmp(data->display, (char*)special_expressions[i].input) == 0) {
            if (strcmp(data->display, "dum") == 0 || strcmp(data->display, "math") == 0 || strcmp(data->display, "/0") == 0 || strcmp(data->display, "0/0") == 0) {
                data->anim_active = TRUE;
                data->anim_impacted = FALSE;
                data->trident_x = (float)(data->root->width - 25);
                data->trident_y = 15;
                data->trident_speed = 2.0f;
                for (int j = 0; j < MAX_PARTICLES; j++) data->particles[j].active = FALSE;
            } else {
                strcpy(data->display, (char*)special_expressions[i].output);
            }
            data->numisnew = TRUE;
            data->is_undefined = (strcmp(data->display, "/0") == 0 || strcmp(data->display, "0/0") == 0) ? TRUE : FALSE;
            update_display(data);
            is_dirty(TRUE);
            return;
        }
    }

    BOOL undef = FALSE;
    double result = eval_ex(data->display, &undef);
    if (undef) {
        data->is_undefined = TRUE;
        strcpy(data->display, "0");
    } else {
        data->is_undefined = FALSE;

        static uint32 seed = 0;
        if (seed == 0) {
            time_t t;
            get_time(&t);
            seed = t.second + t.minute * 60 + t.hour * 3600;
        }
        seed = seed * 1103515245 + 12345;
        uint32 r_val = (seed / 65536) % 1000;
        if (r_val < 400) {
            data->manipulated = TRUE;
            // ofs 1, 5, 10
            seed = seed * 1103515245 + 12345;
            int choices[6] = {-10, -5, -1, 1, 5, 10};
            int off = choices[(seed / 65536) % 6];
            double offset = (double)off;

            // ofs half of num floored (~7%)
            if (r_val < 75) {
                long long half_tens = ((long long)(result < 0 ? -result : result) / 2 / 10) * 10;
                if (half_tens > 0) {
                    seed = seed * 1103515245 + 12345;
                    offset = (double)((seed & 1) ? half_tens : -half_tens);
                }
            }

            result += offset;
        }

        sprintf(data->display, "%.8f", result);
        char* dot = strchr(data->display, '.');
        if (dot) {
            char* p = data->display + strlen(data->display) - 1;
            while (p > dot && *p == '0') *p-- = '\0';
            if (*p == '.') *p = '\0';
        }
    }
    data->numisnew = TRUE;
    update_display(data);
    is_dirty(TRUE);
}

static void on_digit(PON_Comp* comp, int x, int y) {
    (void)x; (void)y;
    GawrculatorData* data = (GawrculatorData*)comp->appdata;
    PON_Button_d* bd = (PON_Button_d*)comp->data;
    if (!data || !bd || !bd->label) return;

    data->manipulated = FALSE;
    if (data->anim_active) {
        data->anim_active = FALSE;
        data->anim_impacted = FALSE;
        for (int i = 0; i < MAX_PARTICLES; i++) data->particles[i].active = FALSE;
    }

    if (data->numisnew || data->is_undefined) {
        strcpy(data->display, bd->label);
        data->numisnew = FALSE;
        data->is_undefined = FALSE;
    } else {
        if (strlen(data->display) < 120) {
            if (strcmp(data->display, "0") == 0) strcpy(data->display, bd->label);
            else strcat(data->display, bd->label);
        }
    }
    update_display(data);
    is_dirty(TRUE);
}

static void on_op(PON_Comp* comp, int x, int y) {
    (void)x; (void)y;
    GawrculatorData* data = (GawrculatorData*)comp->appdata;
    PON_Button_d* bd = (PON_Button_d*)comp->data;
    if (!data || !bd || !bd->label) return;

    if (data->numisnew) data->numisnew = FALSE;
    data->is_undefined = FALSE;
    data->manipulated = FALSE;

    if (strlen(data->display) < 120) {
        strcat(data->display, bd->label);
    }
    update_display(data);
    is_dirty(TRUE);
}

static void on_equals(PON_Comp* comp, int x, int y) {
    (void)x; (void)y;
    GawrculatorData* data = (GawrculatorData*)comp->appdata;
    if (!data) return;
    calculate(data);
}

static void on_clear(PON_Comp* comp, int x, int y) {
    (void)x; (void)y;
    GawrculatorData* data = (GawrculatorData*)comp->appdata;
    if (!data) return;
    strcpy(data->display, "0");
    data->numisnew = TRUE;
    data->is_undefined = FALSE;
    data->manipulated = FALSE;
    update_display(data);
    is_dirty(TRUE);
}

static void on_key(Window* win, unsigned int key) {
    Process* p = get_process(win->pid);
    if (!p) return;
    GawrculatorData* data = (GawrculatorData*)p->data;
    if (!data) return;

    if (key != '\n' && key != '=') data->manipulated = FALSE;

    if (data->anim_active) {
        data->anim_active = FALSE;
        data->anim_impacted = FALSE;
        for (int i = 0; i < MAX_PARTICLES; i++) data->particles[i].active = FALSE;
        data->numisnew = TRUE;
    }

    if (key == '\b') {
        if (data->is_undefined) {
            on_clear(NULL, 0, 0);
            return;
        }
        int len = strlen(data->display);
        if (len > 0) {
            data->display[len-1] = '\0';
            if (strlen(data->display) == 0) strcpy(data->display, "0");
        }
    } else if (key == '\n' || key == '=') {
        calculate(data);
        return;
    } else if (key == 27) { // esc
        on_clear(NULL, 0, 0);
        return;
    } else if (key >= 32 && key <= 126) {
        BOOL is_math = (strchr("0123456789.+-*/(), ", key) != NULL);
        BOOL is_alpha = (key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z');

        if (is_math || is_alpha) {
            if (data->numisnew || data->is_undefined) {
                data->display[0] = '\0';
                data->numisnew = FALSE;
                data->is_undefined = FALSE;
            }
            if (strlen(data->display) < 120) {
                char s[2] = {(char)key, 0};
                if (strcmp(data->display, "0") == 0 && key != '.' && !is_alpha) {
                    strcpy(data->display, s);
                } else {
                    if (strcmp(data->display, "0") == 0 && is_alpha) data->display[0] = '\0';
                    strcat(data->display, s);
                }
            }
        }
    }
    update_display(data);
    is_dirty(TRUE);
}

#define OCEAN_SURFACE_Y 58

static inline int ocean_wave(int x) {
    (void)x;
    return 0;
}

void bg(int start_x, int start_y, int width, int height)
{
    const uint8 top_r = 18;
    const uint8 top_g = 95;
    const uint8 top_b = 140;

    const uint8 bottom_r = 6;
    const uint8 bottom_g = 35;
    const uint8 bottom_b = 70;

    for (int y = 0; y < height; y++)
    {
        int r = top_r +
            ((bottom_r - top_r) * y) / height;

        int g = top_g +
            ((bottom_g - top_g) * y) / height;

        int b = top_b +
            ((bottom_b - top_b) * y) / height;

        rect(
            start_x,
            start_y + y,
            width,
            1,
            RGB(r, g, b)
        );
    }

    const uint32 ocean_color =
        RGB(28, 135, 180);

    for (int x = 0; x < width; x++)
    {
        int surface =
            OCEAN_SURFACE_Y + ocean_wave(x);

        if (surface < height)
        {
            rect(
                start_x + x,
                start_y + surface,
                1,
                height - surface,
                ocean_color
            );
        }
    }
    
    const uint32 highlight =
        RGB(90, 190, 225);

    for (int x = 0; x < width; x++)
    {
        int surface =
            OCEAN_SURFACE_Y + ocean_wave(x);

        int end = surface + 5;

        if (end >= height)
            end = height - 1;

        if (surface + 1 < height)
        {
            rect(
                start_x + x,
                start_y + surface + 1,
                1,
                end - surface,
                highlight
            );
        }
    }

    const uint32 foam_color = RGB(225, 250, 255);

    for (int x = 0; x < width; x++)
    {
        int surface =
            OCEAN_SURFACE_Y + ocean_wave(x);

        if (surface >= 0 && surface < height)
            pixel(start_x + x, start_y + surface, foam_color);

        if (surface + 1 < height)
            pixel(start_x + x, start_y + surface + 1, foam_color);
    }
}

static void gawrculator_render(Window* win) {
    Process* p = get_process(win->pid);
    if (!p) return;
    GawrculatorData* data = (GawrculatorData*)p->data;
    if (!data || !data->root) return;

    int content_x = win->x + WIN_BORDER;
    int content_y = win->y + TITLEBAR_H + WIN_BORDER;

    {
        int rw = data->root->width;
        int rh = data->root->height;
        int tile = 14;

        static const uint32 mosaic_pal[4] = {
            RGB(22, 124, 188),
            RGB(20, 74, 118),
            RGB(18, 76, 116),
            RGB(16, 68, 112),
        };

        for (int ty = 0; ty < rh; ty += tile) {
            int th = (ty + tile > rh) ? rh - ty : tile;
            int tile_row = ty / tile;
            for (int tx = 0; tx < rw; tx += tile) {
                int tw = (tx + tile > rw) ? rw - tx : tile;
                int tile_col = tx / tile;
                int idx;
                int cell = (tile_row * 7 + tile_col * 3);
                if (cell % 25 == 0 && cell != 0) {
                    idx = 0;
                } else if (cell % 9 == 0) {
                    idx = 1;
                } else if ((tile_row + tile_col) % 2 == 0) {
                    idx = 2;
                } else idx = 3;

                rect(content_x + tx, content_y + ty, tw, th, mosaic_pal[idx]);
            }
        }
    }
    bg(content_x, content_y, data->root->width, data->root->height);
    PON_render(data->root, content_x, content_y);

    if (data->manipulated && data->gur_bmp) {
        int icon_w = 24;
        int icon_h = 24;
        int panel_w = data->root->width - 21;
        int icon_x = content_x + 10 + panel_w - icon_w - 8;
        int icon_y = content_y + 10 + (40 - icon_h) / 2;
        predraw_bmp_icn(data->gur_bmp->pixel_data,
                       data->gur_bmp->width, data->gur_bmp->height, data->gur_bmp->row_padded,
                       icon_x, icon_y, icon_w, icon_h);
    }

    if (data->anim_active) {
        int clip_x = content_x + 10;
        int clip_y = content_y + 10;
        int clip_w = data->root->width - 25;
        int clip_h = 40;
        set_clip(clip_x, clip_y, clip_w, clip_h);

        int trident_exit_x = 116.0f;

        if (!data->anim_impacted) {
            if (data->trident_x > trident_exit_x) {
                data->trident_x -= data->trident_speed;
                data->trident_speed += 0.67f;
                if (data->trident_x < trident_exit_x) data->trident_x = trident_exit_x;
            }
        }

        if (data->trident_bmp) {
            predraw_bmp_icn(data->trident_bmp->pixel_data,
                           data->trident_bmp->width, data->trident_bmp->height, data->trident_bmp->row_padded,
                           content_x + (int)data->trident_x - 108, content_y + (int)data->trident_y,
                           128, 32);
        }

        if (!data->anim_impacted && data->trident_x <= trident_exit_x) {
            data->anim_impacted = TRUE;
            strcpy(data->display, "0");
            update_display(data);
            
            static const uint32 particle_colors[] = {
                RGB(100, 220, 255),
                RGB(180, 240, 255),
                RGB(60, 180, 255),
                RGB(200, 255, 255),
                RGB(80, 200, 230),
                RGB(245, 245, 196),
                RGB(255, 200, 100),
            };
            int num_colors = 7;

            for (int i = 0; i < MAX_PARTICLES; i++) {
                // lcg
                uint32 seed = (uint32)(i * 1115190511090209101ull + 701231007211001ull); //derived from wordsum (kosekibijou, gawrgura) but i truncate to valid, 8>0
                seed ^= seed >> 17;
                seed ^= seed << 5;

                float spawn_x = 10.0f + (float)(seed & 0x7) * 1.0f;
                float spawn_y =  4.0f + (float)((seed >> 4) & 0x1F) * 1.05f;

                // +-80deg>rad
                int angle_idx = (int)((seed >> 10) & 0x1F);
                float angle = -1.4f + (float)angle_idx * (2.8f / 31.0f);
                int speed_idx = (int)((seed >> 16) & 0xF);
                float speed = 0.88f + (float)speed_idx * 0.18f;

                data->particles[i].active = TRUE;
                data->particles[i].x  = spawn_x;
                data->particles[i].y  = spawn_y;
                data->particles[i].vx = speed * cosf(angle);
                data->particles[i].vy = speed * sinf(angle);
                data->particles[i].color = particle_colors[i % num_colors];
            }
        }

        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (data->particles[i].active) {
                data->particles[i].x += data->particles[i].vx;
                data->particles[i].y += data->particles[i].vy;

                int psz = (i & 1) ? 2 : 3;
                rect(content_x + (int)data->particles[i].x,
                     content_y + 10 + (int)data->particles[i].y,
                     psz, psz, data->particles[i].color);

                // cull
                if (data->particles[i].x < 0.0f || data->particles[i].x > (float)(clip_w + 10) ||
                    data->particles[i].y < 0.0f || data->particles[i].y > (float)(clip_h - 2))
                    data->particles[i].active = FALSE;
            }
        }

        reset_clip();
        is_dirty(TRUE);
    }
}

static void pref_m_down(Window* win, int x, int y) {
    Process* p = get_process(win->pid);
    if (!p) return;
    GawrculatorData* data = (GawrculatorData*)p->data;
    if (handle_mouse(data->root, 0, 0, x, y, PON_MOUSE_DOWN)) is_dirty(TRUE);
}

static void m_up(Window* win, int x, int y) {
    Process* p = get_process(win->pid);
    if (!p) return;
    GawrculatorData* data = (GawrculatorData*)p->data;
    if (handle_mouse(data->root, 0, 0, x, y, PON_MOUSE_UP)) is_dirty(TRUE);
}

static void m_move(Window* win, int x, int y) {
    Process* p = get_process(win->pid);
    if (!p) return;
    GawrculatorData* data = (GawrculatorData*)p->data;
    if (handle_mouse(data->root, 0, 0, x, y, PON_MOUSE_MOVE)) is_dirty(TRUE);
}

static void cleanup(Window* win) {
    Process* p = get_process(win->pid);
    if (p && p->data) {
        GawrculatorData* data = (GawrculatorData*)p->data;
        if (data->trident_bmp) free_preloaded_bmp(data->trident_bmp);
        if (data->gur_bmp) free_preloaded_bmp(data->gur_bmp);
        if (data->root) PON_free(data->root);
        kfree(data);
        p->data = NULL;
    }
}

void launch_gawrculator() {
    Process* p = create_process("gawrculator");
    if (!p) return;

    GawrculatorData* data = (GawrculatorData*)kmalloc(sizeof(GawrculatorData));
    memset(data, 0, sizeof(GawrculatorData));
    p->data = (char*)data;
    strcpy(data->display, "0");
    data->numisnew = TRUE;
    data->trident_bmp = preload_bmp("/SYSTEM/trid.bmp");
    data->gur_bmp = preload_bmp("/SYSTEM/gur-ic.bmp");

    int win_w = 220;
    int win_h = 300;
    Window* win = window(p->pid, "Gawrculator", -1, -1, win_w, win_h);
    if (!win) return;

    win->content_renderer = gawrculator_render;
    win->on_mouse_down = pref_m_down;
    win->on_mouse_up = m_up;
    win->on_mouse_move = m_move;
    win->on_key_press = on_key;
    win->on_close = cleanup;

    data->root = PANEL(0, 0, win_w - 4, win_h - 24, COLOR_BG_DARK);
    data->root->draw = NULL;
    
    PON_Comp* disp_bg = PANEL(10, 10, win_w - 25, 40, COLOR_BG_LIGHT);
    PON_child(data->root, disp_bg);
    data->display_text = TEXT(10, 10, "0", COLOR_TEXT);
    PON_child(disp_bg, data->display_text);

    int bw = 45;
    int bh = 45;
    int gap = 5;
    int start_x = 10;
    int start_y = 72;

    const char* buttons[] = {
        "7", "8", "9", "/",
        "4", "5", "6", "*",
        "1", "2", "3", "-",
        "C", "0", "=", "+"
    };

    for (int i = 0; i < 16; i++) {
        int row = i / 4;
        int col = i % 4;
        uint32 color = RGB(19, 114, 156);
        event_cb_t action = on_digit;

        if (i == 12) {
            action = on_clear;
            color = COLOR_TONGUE;
        } else if (i == 14) {
            action = on_equals;
            color = RGB(42, 170, 188);
        } else if (col == 3) {
            action = on_op;
            color = COLOR_BTN_OP;
        }

        PON_Comp* btn = BUTTON(start_x + col * (bw + gap), start_y + row * (bh + gap), bw, bh, buttons[i], action);
        if (btn) {
            btn->appdata = data;
            PON_Button_d* bd = (PON_Button_d*)btn->data;
            bd->text_color = RGB(180, 240, 255);
            bd->bg_color = color;
            bd->text_bold = TRUE;
            PON_child(data->root, btn);
        }
    }

    is_dirty(TRUE);
}