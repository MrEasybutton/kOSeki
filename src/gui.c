#include "gui.h"
#include "vesa.h"
#include "bmp.h"
#include "kheap.h"
#include "string.h"
#include "graphics.h"
#include "fat32.h"
#include "console.h"
#include "bae.h"
#include "cmos.h"
#include "kernel.h"
#include "kronii.h"
#include "utils.h"
#include "serial.h"
#include "procsys.h"
#include "keyboard.h"
#include "apps/CLStudio.h"
#include "apps/CLStudio.c"

#define MAX_BUTTONS 20

#define MAX_PANES (MAX_DIVIDERS + 1)
#define SPLIT_MIN_THICC 40
#define SPLIT_HIT_TOL 4
#define SPLIT_FREQ 67
#define SPLIT_AMP 67

extern char* fat_read_file(char* fpath);
extern char* g_wllp_data;
extern int g_wllp_width;
extern int g_wllp_height;
extern int g_wallpaper_row_padded; 
extern int CLOCK_X, CLOCK_Y;
extern uint32 *g_back_buffer;
extern uint32_t timer_ticks;

static icon_button_t* button_list[MAX_BUTTONS];
static int button_count = 0;

static Window* window_list[MAX_WINDOWS];
static int wincount = 0;
static int next_window_id = 0;

static preloaded_t* g_close_icon = NULL;

static Window* g_dragging_window = NULL;
static Window* g_mouse_capture_window = NULL;
static Window* g_last_mouse_down_win = NULL;
static int g_last_mouse_down_x = -1;
static int g_last_mouse_down_y = -1;
static int g_drag_offset_x = 0;
static int g_drag_offset_y = 0;
static BOOL g_mouse_down_on_title_bar = FALSE;

static uint8 g_curr_r = 140, g_curr_g = 90, g_curr_b = 200;

void button_v(int x, int y, int width, int height, BUTTON_STATE state, uint8 r, uint8 g, uint8 b);
void button(int x, int y, int width, int height, BUTTON_STATE state);
static void notif();

BOOL g_screen_dirty = TRUE;
BOOL g_bg_dirty = TRUE;

static BOOL _kb_combo(BOOL* was_pressed, uint8 key1, uint8 key2) {
    BOOL pressed = ((kb_is_key_pressed(SCAN_CODE_KEY_LEFT_SHIFT) || kb_is_key_pressed(SCAN_CODE_KEY_RIGHT_SHIFT))
                && kb_is_key_pressed(key1) && kb_is_key_pressed(key2));

    if (pressed && !*was_pressed) {
        *was_pressed = TRUE;
        return TRUE;
    }
    if (!pressed) *was_pressed = FALSE;
    return FALSE;
}

static BOOL _kb_split(void) {
    static BOOL was_pressed = FALSE;
    return _kb_combo(&was_pressed, SCAN_CODE_KEY_6, SCAN_CODE_KEY_7);
}

static void _toggle_split(void) {
    Window* win = get_active_win();
    if (!win) return;

    if (win->split_panes_is_active) {
        win->split_panes_is_active = FALSE;
    } else {
        win->split_panes_is_active = TRUE;
        if (win->split_count <= 0 && win->split_pos <= 0) {
            int content_w = win->width - (2 * WIN_BORDER);
            if (content_w < SPLIT_MIN_THICC * 2) content_w = SPLIT_MIN_THICC * 2;
            win->split_pos = content_w / 2;
        }
    }

    win->split_dragging = FALSE;
    win->dragging_div_idx = -1;
    win->split_anim_active = FALSE;
    win->split_is_active = FALSE;
    is_dirty(TRUE);
}

static BOOL _kb_pixelate(void) {
    static BOOL was_pressed = FALSE;
    return _kb_combo(&was_pressed, SCAN_CODE_KEY_8, SCAN_CODE_KEY_8);
}

static void _toggle_pixelate(void) {
    Window* win = get_active_win();
    if (!win) return;

    win->pixelate_is_active = !win->pixelate_is_active;
    if (win->pixelate_is_active && win->pixelate_block_size <= 0) {
        win->pixelate_block_size = 4;
    }
    is_dirty(TRUE);
}

static void _apply_pixelate(int content_x, int content_y, int content_w, int content_h, int block_size, int screen_width, int screen_height) {
    if (block_size < 1) block_size = 1;

    for (int by = 0; by < content_h; by += block_size) {
        for (int bx = 0; bx < content_w; bx += block_size) {
            int bw = (bx + block_size <= content_w) ? block_size : content_w - bx;
            int bh = (by + block_size <= content_h) ? block_size : content_h - by;

            // average the block
            uint32 r_sum = 0, g_sum = 0, b_sum = 0, n = 0;
            for (int y = 0; y < bh; y++) {
                int sy = content_y + by + y;
                if (sy < 0 || sy >= screen_height) continue;
                for (int x = 0; x < bw; x++) {
                    int sx = content_x + bx + x;
                    if (sx < 0 || sx >= screen_width) continue;
                    uint32 px = g_back_buffer[sy * screen_width + sx];
                    r_sum += (px >> 16) & 0xFF;
                    g_sum += (px >> 8) & 0xFF;
                    b_sum += px & 0xFF;
                    n++;
                }
            }
            if (n == 0) continue;
            uint32 avg = RGB(r_sum / n, g_sum / n, b_sum / n);

            // fill block with average col
            for (int y = 0; y < bh; y++) {
                int sy = content_y + by + y;
                if (sy < 0 || sy >= screen_height) continue;
                for (int x = 0; x < bw; x++) {
                    int sx = content_x + bx + x;
                    if (sx < 0 || sx >= screen_width) continue;
                    g_back_buffer[sy * screen_width + sx] = avg;
                }
            }
        }
    }
}

void g_init() {
    g_close_icon = preload_bmp("SYSTEM/exit-ic.bmp");

    for (int i = 0; i < MAX_BUTTONS; i++) button_list[i] = NULL;
}

void is_dirty(BOOL dirty) {
    g_screen_dirty = dirty;
}

void is_bg_dirty(BOOL dirty) {
    g_bg_dirty = dirty;
    if (dirty) g_screen_dirty = TRUE;
}

static int _split_gcd(int a, int b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b) { int t = b; b = a % b; a = t; }
    return (a == 0) ? 1 : a;
}

// approx sin(deg)*1000
static int _sinx1000(int deg) {
    deg %= 360;
    if (deg < 0) deg += 360;

    int sign = 1;
    if (deg > 180) { deg -= 180; sign = -1; }

    long num = 4L * deg * (180 - deg);
    long den = 40500L - (long)deg * (180 - deg);
    if (den <= 0) den = 1;

    return (int)(sign * (num * 1000) / den);
}

// 0,0 for unset
static void _get_split_normal(Window* win, int* out_nx, int* out_ny) {
    int ddx = win->split_dir_x;
    int ddy = win->split_dir_y;
    if (ddx == 0 && ddy == 0) { ddx = 0; ddy = 1; }

    int g = _split_gcd(ddx, ddy);
    ddx /= g; ddy /= g;

    int nx = -ddy, ny = ddx;
    if (nx < 0 || (nx == 0 && ny < 0)) { nx = -nx; ny = -ny; }

    *out_nx = nx; *out_ny = ny;
}

// projection of line onto normal line
static int _split_project(int nx, int ny, int x, int y) {
    return nx * x + ny * y;
}

// get bounds of projected onto normal line
static void _get_split_bounds(int content_w, int content_h, int nx, int ny, int* out_min, int* out_max) {
    int corners[4] = {
        _split_project(nx, ny, 0, 0),
        _split_project(nx, ny, content_w, 0),
        _split_project(nx, ny, 0, content_h),
        _split_project(nx, ny, content_w, content_h)
    };
    int min_s = corners[0], max_s = corners[0];
    for (int i = 1; i < 4; i++) {
        if (corners[i] < min_s) min_s = corners[i];
        if (corners[i] > max_s) max_s = corners[i];
    }
    *out_min = min_s; *out_max = max_s;
}

static int _get_split_ofs(Window* win, int content_w, int content_h, int nx, int ny, int* out_offsets) {
    if (win->split_count <= 0) {
        int split = win->split_pos;
        if (split < SPLIT_MIN_THICC) split = SPLIT_MIN_THICC;
        if (split > content_w - SPLIT_MIN_THICC) split = content_w - SPLIT_MIN_THICC;
        out_offsets[0] = split;
        return 1;
    }

    int k = win->split_count;
    if (k > MAX_DIVIDERS) k = MAX_DIVIDERS;

    for (int i = 0; i < k; i++) out_offsets[i] = win->split_ofs[i];

    for (int i = 1; i < k; i++) { //sort
        int v = out_offsets[i];
        int j = i - 1;
        while (j >= 0 && out_offsets[j] > v) {
            out_offsets[j + 1] = out_offsets[j];
            j--;
        }
        out_offsets[j + 1] = v;
    }

    int min_s, max_s;
    _get_split_bounds(content_w, content_h, nx, ny, &min_s, &max_s);
    int lo = min_s + SPLIT_MIN_THICC;
    int hi = max_s - SPLIT_MIN_THICC;
    if (hi < lo) hi = lo;

    for (int i = 0; i < k; i++) {
        int floor_v = (i == 0) ? lo : out_offsets[i - 1] + SPLIT_MIN_THICC;
        if (out_offsets[i] < floor_v) out_offsets[i] = floor_v;
    }
    for (int i = k - 1; i >= 0; i--) {
        int ceil_v = (i == k - 1) ? hi : out_offsets[i + 1] - SPLIT_MIN_THICC;
        if (out_offsets[i] > ceil_v) out_offsets[i] = ceil_v;
    }

    return k;
}

static BOOL _get_pane_for_point(Window* win, int mouse_x, int mouse_y, int* pane_index, int* rel_x, int* rel_y, BOOL* on_divider) {
    if (!win || !pane_index || !rel_x || !rel_y || !on_divider) return FALSE;

    if (!win->split_panes_is_active) return FALSE;

    int content_x = win->x + WIN_BORDER;
    int content_y = win->y + TITLEBAR_H + WIN_BORDER;
    int content_w = win->width - (2 * WIN_BORDER);
    int content_h = win->height - TITLEBAR_H - WIN_BORDER;

    if (mouse_x < content_x || mouse_x >= content_x + content_w ||
        mouse_y < content_y || mouse_y >= content_y + content_h) {
        return FALSE;
    }

    int nx, ny;
    _get_split_normal(win, &nx, &ny);

    int offsets[MAX_DIVIDERS];
    int k = _get_split_ofs(win, content_w, content_h, nx, ny, offsets);

    int px = mouse_x - content_x;
    int py = mouse_y - content_y;
    int s = _split_project(nx, ny, px, py);

    *rel_x = px;
    *rel_y = py;

    // return either divider or pane idx
    for (int i = 0; i < k; i++) {
        int d = s - offsets[i];
        if (d < 0) d = -d;
        if (d <= SPLIT_HIT_TOL) {
            *on_divider = TRUE;
            *pane_index = i;
            return TRUE;
        }
    }

    *on_divider = FALSE;
    int pane = 0;
    for (int i = 0; i < k; i++) {
        if (s >= offsets[i]) pane++;
    }
    *pane_index = pane;
    return TRUE;
}

static BOOL _draw_winframe(Window* win) {
    if (!win) {
        printf("ERROR: Null window in _gui_draw_window_frame\n");
        return FALSE;
    }

    if (win->width <= 0 || win->height <= 0) return FALSE;

    int screen_width = vbe_get_width();
    int screen_height = vbe_get_height();

    if (win->x >= screen_width || win->y >= screen_height ||
        win->x + win->width <= 0 || win->y + win->height <= 0) {
        return FALSE;
    }

    int safe_w = (win->width < TITLEBAR_H + 10) ? TITLEBAR_H + 10 : win->width;
    int safe_h = (win->height < TITLEBAR_H + 10) ? TITLEBAR_H + 10 : win->height;

    BOOL can_use_cache = FALSE;

    // as long as seiso and size fits
    if (win->frame_cache && !win->frame_cache_dirty &&
        win->width == win->last_w && win->height == win->last_h) {
        can_use_cache = TRUE;
    }

    if (can_use_cache) {
        int cache_width = safe_w;
        int cache_height = safe_h;

        if (win->x >= 0 && win->y >= 0 && win->x + cache_width <= screen_width && win->y + cache_height <= screen_height) {
            for (int y = 0; y < cache_height; y++) {
                uint32 *src = &win->frame_cache[y * cache_width];
                uint32 *dst = &g_back_buffer[(win->y + y) * screen_width + win->x];
                memcpy(dst, src, cache_width * sizeof(uint32));
            }
            return TRUE;
        }

        for (int y = 0; y < cache_height; y++) {
            int screen_y = win->y + y;
            if (screen_y >= 0 && screen_y < screen_height) {
                uint32 *src_row = &win->frame_cache[y * cache_width];
                uint32 *dst_row = &g_back_buffer[screen_y * screen_width + win->x];

                int visible_width = cache_width;
                if (win->x < 0) {
                    src_row -= win->x;
                    dst_row -= win->x;
                    visible_width += win->x;
                }
                if (win->x + visible_width > screen_width) {
                    visible_width = screen_width - win->x;
                }
                
                if (visible_width > 0) {
                    if (win->x >= 0 && win->x + cache_width <= screen_width) {
                        memcpy(dst_row, src_row, visible_width * sizeof(uint32));
                    }
                    else {
                        for (int x = 0; x < visible_width; x++) {
                            int screen_x = win->x + x;
                            if (screen_x >= 0 && screen_x < screen_width) {
                                g_back_buffer[screen_y * screen_width + screen_x] = src_row[x];
                            }
                        }
                    }
                }
            }
        }
        return TRUE;
    }

    // rebuild cache
    if (!win->frame_cache ||
        win->width != win->last_w || win->height != win->last_h) {

        if (win->frame_cache) {
            kfree(win->frame_cache);
            win->frame_cache = NULL;
        }
        
        if (win->width != win->last_w || win->height != win->last_h) {
            is_bg_dirty(TRUE);
        }

        win->frame_cache = (uint32*)kmalloc(safe_w * safe_h * sizeof(uint32));
        if (!win->frame_cache) {
            kprint("W: Frame cache alloc failed for window %d\n", win->id);
        }
    }

    const char* title = (win->title != NULL) ? win->title : "Untitled";

    rect(win->x, win->y, safe_w, safe_h, RGB(230, 200, 250));
    rect(win->x + 1, win->y + 1, safe_w - 2, safe_h - 2, RGB(140, 100, 180));

    int title_y = win->y + WIN_BORDER;
    rect_grad(win->x + WIN_BORDER, title_y,
              safe_w - (2 * WIN_BORDER), TITLEBAR_H,
              RGB(win->avg_r, win->avg_g, win->avg_b),
              RGB(win->avg_r * 7 / 10, win->avg_g * 7 / 10, win->avg_b * 7 / 10));

    int highlight_width = safe_w - (2 * WIN_BORDER);
    if (highlight_width > 0) {
        for (int i = 0; i < 2; i++) {
            uint32 highlight = RGBA(255, 255, 255, 50 - (i * 20));

            int y_pos = title_y + i;
            if (y_pos >= 0 && y_pos < screen_height) {
                for (int x = 0; x < highlight_width; x++) {
                    int x_pos = win->x + WIN_BORDER + x;
                    if (x_pos >= 0 && x_pos < screen_width) {
                        pixel(x_pos, y_pos, highlight);
                    }
                }
            }
        }
    }

    text(title, win->x + WIN_BORDER + 6, win->y + WIN_BORDER + 2,
         RGBA(0, 0, 0, 120), FONT_KALNIA, FALSE);
    text(title, win->x + WIN_BORDER + 5, win->y + WIN_BORDER + 1,
         RGB(255, 255, 255), FONT_KALNIA, FALSE);

    rect(win->x + WIN_BORDER, win->y + TITLEBAR_H + WIN_BORDER,
         safe_w - (2 * WIN_BORDER),
         safe_h - TITLEBAR_H - (2 * WIN_BORDER),
         RGB(230, 220, 250));

    if (safe_w >= TITLEBAR_H + WIN_BORDER + 10) {
        int close_x = win->x + safe_w - TITLEBAR_H - WIN_BORDER + 2;
        int close_y = win->y + WIN_BORDER + 2;
        int btn_size = TITLEBAR_H - 4;

        if (g_close_icon && g_close_icon->pixel_data &&
            g_close_icon->width > 0 && g_close_icon->height > 0 && btn_size > 0) {

            if (close_x + btn_size > 0 && close_x < screen_width &&
                close_y + btn_size > 0 && close_y < screen_height) {

                predraw_bmp_fit(g_close_icon->pixel_data, g_close_icon->width,
                                      g_close_icon->height, g_close_icon->row_padded,
                                      close_x, close_y, btn_size, btn_size);
            }
        }
    }

    // capture drawn frame to cache
    if (win->frame_cache) {
        for (int y = 0; y < safe_h; y++) {
            int screen_y = win->y + y;
            if (screen_y >= 0 && screen_y < screen_height) {
                for (int x = 0; x < safe_w; x++) {
                    int screen_x = win->x + x;
                    if (screen_x >= 0 && screen_x < screen_width) {
                        uint32 pixel = g_back_buffer[screen_y * screen_width + screen_x];
                        win->frame_cache[y * safe_w + x] = pixel;
                    }
                }
            }
        }
        win->frame_cache_dirty = FALSE;
        win->last_w = win->width;
        win->last_h = win->height;
    }

    return TRUE;
}

void focus_win(Window* win) {
    if (!win) return;

    if (!isvalidwin(win->pid, win)) {
        kprint("W: Cannot bring invalid window to front\n");
        return;
    }

    int i, j;
    Window* temp = NULL;

    for (i = 0; i < wincount; i++) {
        if (window_list[i] && window_list[i]->id == win->id) {
            temp = window_list[i];
            for (j = i; j < wincount - 1; j++) {
                window_list[j] = window_list[j+1];
            }
            wincount--;
            break;
        }
    }

    if (temp) {
        window_list[wincount++] = temp;
    }

    for (i = 0; i < wincount; i++) {
        if (window_list[i]) {
            BOOL was_active = window_list[i]->active;
            window_list[i]->active = (window_list[i]->id == win->id);

            if (was_active != window_list[i]->active) {
                window_list[i]->frame_cache_dirty = TRUE;
            }
        }
    }

    is_bg_dirty(TRUE);
}

static int g_cascade_x = 50;
static int g_cascade_y = 50;

Window* window_r(int pid, const char* title, int x, int y, int width, int height, uint8 r, uint8 g, uint8 b) {
    if (!isvalidproc(pid)) {
        kprint("ERR: Cannot create window for invalid process PID %d\n", pid);
        return NULL;
    }
    
    if (wincount >= MAX_WINDOWS) {
        kprint("ERROR: Max windows reached!\n");
        return NULL;
    }
    
    if (width <= 0 || height <= 0) {
        kprint("ERROR: Invalid window dimensions!\n");
        return NULL;
    }

    BOOL do_cascade = (x == -1 || y == -1);
    
    if (!do_cascade) {
        for (int i = 0; i < wincount; i++) {
            if (window_list[i] && window_list[i]->x == x && window_list[i]->y == y) {
                do_cascade = TRUE;
                break;
            }
        }
    }

    if (do_cascade) {
        if (x == -1 || y == -1) {
            x = g_cascade_x;
            y = g_cascade_y;
        } else {
            x += 20;
            y += 20;
        }
        
        g_cascade_x += 25;
        g_cascade_y += 25;
        
        //wrap
        if (g_cascade_x > SCREEN_WIDTH / 2) g_cascade_x = 50;
        if (g_cascade_y > SCREEN_HEIGHT / 2) g_cascade_y = 50;
    }
    
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + width > SCREEN_WIDTH) x = SCREEN_WIDTH - width;
    if (y + height > SCREEN_HEIGHT) y = SCREEN_HEIGHT - height;

    if (width < 50) width = 50;
    if (height < 50) height = 50;


    Window* new_win = (Window*)kmalloc(sizeof(Window));
    if (!new_win) {
        printf("ERROR: Failed to allocate memory for new window!\n");
        return NULL;
    }
    
    memset(new_win, 0, sizeof(Window));
    
    new_win->id = next_window_id++;
    new_win->pid = pid;
    new_win->x = x;
    new_win->y = y;
    new_win->width = width;
    new_win->height = height;
    new_win->avg_r = r;
    new_win->avg_g = g;
    new_win->avg_b = b;

    new_win->frame_cache = NULL;
    new_win->frame_cache_dirty = TRUE;
    new_win->last_x = x;
    new_win->last_y = y;
    new_win->last_w = width;
    new_win->last_h = height;

    if (title) {
        new_win->title = (char*)kmalloc(strlen(title) + 1);
        if (new_win->title) {
            strcpy(new_win->title, title);
        } else {
            kprint("ERROR: failed to allocate memory for window title\n");
            kfree(new_win);
            return NULL;
        }
    } else {
        new_win->title = (char*)kmalloc(strlen("Untitled") + 1);
        if (new_win->title) {
            strcpy(new_win->title, "Untitled");
        } else {
            kprint("ERROR: failed to allocate memory for window title\n");
            kfree(new_win);
            return NULL;
        }
    }
    
    new_win->active = TRUE;
    new_win->content_renderer = NULL;

    window_list[wincount++] = new_win;
    register_for_process(pid, new_win);
    
    printf("a window has been created: %s (ID: %d, PID: %d)\n", new_win->title, new_win->id, new_win->pid);

    is_bg_dirty(TRUE);
    
    return new_win;
}

Window* window(int pid, const char* title, int x, int y, int width, int height) {
    return window_r(pid, title, x, y, width, height, g_curr_r, g_curr_g, g_curr_b);
}

static BOOL is_window_occluded(Window* win, int win_index) {
    for (int i = win_index + 1; i < wincount; i++) {
        Window* above = window_list[i];
        if (!above) continue;
        
        if (above->x <= win->x && 
            above->y <= win->y &&
            above->x + above->width >= win->x + win->width &&
            above->y + above->height >= win->y + win->height) {
            return TRUE;
        }
    }
    return FALSE;
}

static void _comp_split_content(Window* win, int content_x, int content_y, int content_w, int content_h,
                                            int nx, int ny, const int* offsets, int k, const int* pane_offset, int pane_count,
                                            int screen_width, int screen_height) {
    if (!win || !win->content_renderer) return;
    if (content_w <= 0 || content_h <= 0) return;

    uint32* before_buf = (uint32*)kmalloc(content_w * content_h * sizeof(uint32));
    if (!before_buf) {
        win->content_renderer(win);
        return;
    }

    for (int y = 0; y < content_h; y++) {
        int sy = content_y + y;
        if (sy < 0 || sy >= screen_height) continue;
        int sx0 = content_x;
        int copy_w = content_w;
        if (sx0 < 0) { copy_w += sx0; sx0 = 0; }
        if (sx0 + copy_w > screen_width) copy_w = screen_width - sx0;
        if (copy_w > 0) {
            memcpy(&before_buf[y * content_w + (sx0 - content_x)], &g_back_buffer[sy * screen_width + sx0], copy_w * sizeof(uint32));
        }
    }

    win->content_renderer(win);

    uint32* after_buf = (uint32*)kmalloc(content_w * content_h * sizeof(uint32));
    if (!after_buf) {
        for (int y = 0; y < content_h; y++) {
            int sy = content_y + y;
            if (sy < 0 || sy >= screen_height) continue;
            int sx0 = content_x;
            int copy_w = content_w;
            if (sx0 < 0) { copy_w += sx0; sx0 = 0; }
            if (sx0 + copy_w > screen_width) copy_w = screen_width - sx0;
            if (copy_w > 0) {
                memcpy(&g_back_buffer[sy * screen_width + sx0], &before_buf[y * content_w + (sx0 - content_x)], copy_w * sizeof(uint32));
            }
        }
        kfree(before_buf);
        return;
    }

    for (int y = 0; y < content_h; y++) {
        int sy = content_y + y;
        if (sy < 0 || sy >= screen_height) continue;
        int sx0 = content_x;
        int copy_w = content_w;
        if (sx0 < 0) { copy_w += sx0; sx0 = 0; }
        if (sx0 + copy_w > screen_width) copy_w = screen_width - sx0;
        if (copy_w > 0) {
            memcpy(&after_buf[y * content_w + (sx0 - content_x)], &g_back_buffer[sy * screen_width + sx0], copy_w * sizeof(uint32));
        }
    }

    for (int y = 0; y < content_h; y++) {
        int sy = content_y + y;
        if (sy < 0 || sy >= screen_height) continue;

        int crossing_px[MAX_DIVIDERS];
        int num_crossing = 0;
        int base_pane = 0;

        for (int i = 0; i < k; i++) {
            if (nx != 0) {
                int numer = offsets[i] - ny * y;
                int px_i = numer / nx;
                if ((numer % nx != 0) && ((numer < 0) != (nx < 0))) px_i--; //floordiv

                if (px_i < 0) { base_pane++; continue; }
                if (px_i >= content_w) continue;
                crossing_px[num_crossing++] = px_i;
            } else {
                if (ny * y >= offsets[i]) base_pane++;
            }
        }

        int x_start = 0;
        int pane = base_pane;
        for (int seg = 0; seg <= num_crossing; seg++) {
            int x_end = (seg < num_crossing) ? crossing_px[seg] : content_w;
            if (x_end > x_start) {
                int p = pane;
                if (p >= pane_count) p = pane_count - 1;
                int offset = pane_offset[p];
                int src_y = y - offset;

                int sx0 = x_start;
                int w_seg = x_end - x_start;
                int screen_x0 = content_x + sx0;
                if (screen_x0 < 0) { w_seg += screen_x0; sx0 -= screen_x0; screen_x0 = 0; }
                if (screen_x0 + w_seg > screen_width) w_seg = screen_width - screen_x0;

                if (w_seg > 0) {
                    const uint32* src = (src_y >= 0 && src_y < content_h)
                        ? &after_buf[src_y * content_w + sx0]
                        : &before_buf[y * content_w + sx0];
                    memcpy(&g_back_buffer[sy * screen_width + screen_x0], src, w_seg * sizeof(uint32));
                }
            }
            x_start = x_end;
            pane++;
        }

        //divider thing
        for (int i = 0; i < num_crossing; i++) {
            int sx = content_x + crossing_px[i];
            if (sx >= 0 && sx < screen_width) g_back_buffer[sy * screen_width + sx] = RGB(110, 90, 150);
            if (sx + 1 >= 0 && sx + 1 < screen_width) g_back_buffer[sy * screen_width + sx + 1] = RGB(220, 180, 255);
        }
    }

    kfree(before_buf);
    kfree(after_buf);

    //draw one full line if horiz
    if (nx == 0 && ny != 0) {
        for (int i = 0; i < k; i++) {
            int y0 = offsets[i] / ny;
            if ((offsets[i] % ny != 0) && (offsets[i] < 0)) y0--;
            rect(content_x, content_y + y0, content_w, 1, RGB(110, 90, 150));
            rect(content_x, content_y + y0 + 1, content_w, 1, RGB(220, 180, 255));
        }
    }
}

void draw_all_win() {
    valid_refs();
    
    int err_cnt = 0; //for avoiding spam
    
    if (wincount < 0 || wincount > MAX_WINDOWS) {
        kprint("!CRITICAL!: wincount is corrupted (%d). Resetting.\n", wincount);
        wincount = 0;
        for (int i = 0; i < MAX_WINDOWS; i++) {
            window_list[i] = NULL;
        }
        return;
    }
    
    for (int i = 0; i < wincount; i++) {
        Window* win = window_list[i];
        
        if (!win) continue;

        if (is_window_occluded(win, i)) continue;
        
        if (!isvalidwin(win->pid, win)) {
            kprint("WARNING: Window at index %d has invalid process reference\n", i);
            continue;
        }
        
        if (win->width <= 0 || win->height <= 0) {
            kprint("WARNING: Window %d has invalid dimensions (%d,%d)\n", 
                   win->id, win->width, win->height);
            continue;
        }
        
        int screen_width = vbe_get_width();
        int screen_height = vbe_get_height();
        
        if (win->x >= screen_width || win->y >= screen_height || 
            win->x + win->width <= 0 || win->y + win->height <= 0) {
            continue;
        }
        
        BOOL render_error = FALSE;
        
        _draw_winframe(win);

        int content_x = win->x + WIN_BORDER;
        int content_y = win->y + TITLEBAR_H + WIN_BORDER;
        int content_w = win->width - (2 * WIN_BORDER);
        int content_h = win->height - TITLEBAR_H - WIN_BORDER;
        
        if (win->split_panes_is_active) {
            int nx, ny;
            _get_split_normal(win, &nx, &ny);
            int offsets[MAX_DIVIDERS];
            int k = _get_split_ofs(win, content_w, content_h, nx, ny, offsets);
            int pane_count = k + 1;

            if (!win->split_anim_active) {
                win->split_anim_ticks = timer_ticks;
                win->split_anim_last_ofs = 0x7fffffff; //redraw the first frame
            }
            win->split_anim_active = TRUE;
            win->split_is_active = TRUE;

            uint32_t elapsed = timer_ticks - win->split_anim_ticks;
            int phase_deg = (int)((elapsed * 360) / SPLIT_FREQ) % 360;
            int bob = (SPLIT_AMP * _sinx1000(phase_deg)) / 1000;

            if (bob != win->split_anim_last_ofs) {
                win->split_anim_last_ofs = bob;
                is_dirty(TRUE); //endless
            }

            int pane_offset[MAX_PANES];
            for (int p = 0; p < pane_count; p++) {
                pane_offset[p] = (p % 2 == 0) ? bob : -bob;
            }

            _comp_split_content(win, content_x, content_y, content_w, content_h,
                                          nx, ny, offsets, k, pane_offset, pane_count,
                                          screen_width, screen_height);
        } else {
            win->split_anim_active = FALSE;
            win->split_is_active = FALSE;

            if (win->content_renderer) {
                win->content_renderer(win);
            }
        }

        if (win->pixelate_is_active) {
            _apply_pixelate(content_x, content_y, content_w, content_h,
                              win->pixelate_block_size, screen_width, screen_height);
        }
        
        if (render_error && err_cnt < 5) {
            kprint("WARNING: Error rendering content for window %d\n", win->id);
            err_cnt++;
            
            if (err_cnt == 5) {
                kprint("(...)\n");
            }
        }
    }
}

void m_win_event_handler(int mouse_x, int mouse_y, BOOL mouse_down) {
    static BOOL mouse_was_down = FALSE;

    valid_refs();

    Window* win_under_mouse = NULL;
    for (int i = wincount - 1; i >= 0; i--) {
        Window* win = window_list[i];
        if (!win) continue;
        if (mouse_x >= win->x && mouse_x < win->x + win->width &&
            mouse_y >= win->y && mouse_y < win->y + win->height) {
            win_under_mouse = win;
            break;
        }
    }

    if (g_mouse_capture_window) {
        if (g_mouse_capture_window->split_panes_is_active && g_mouse_capture_window->split_dragging) {
            Window* w = g_mouse_capture_window;
            int content_x = w->x + WIN_BORDER;
            int content_y = w->y + TITLEBAR_H + WIN_BORDER;
            int content_w = w->width - (2 * WIN_BORDER);
            int content_h = w->height - TITLEBAR_H - WIN_BORDER;

            int nx, ny;
            _get_split_normal(w, &nx, &ny);
            int s = _split_project(nx, ny, mouse_x - content_x, mouse_y - content_y);

            if (w->split_count <= 0) {
                int min_split = SPLIT_MIN_THICC;
                int max_split = content_w - SPLIT_MIN_THICC;

                if (s < min_split) s = min_split;
                if (s > max_split) s = max_split;
                w->split_pos = s;
            } else {
                int offsets[MAX_DIVIDERS];
                int k = _get_split_ofs(w, content_w, content_h, nx, ny, offsets);
                int idx = w->dragging_div_idx;

                if (idx >= 0 && idx < k) {
                    int min_s, max_s;
                    _get_split_bounds(content_w, content_h, nx, ny, &min_s, &max_s);
                    int lo = (idx == 0) ? min_s + SPLIT_MIN_THICC : offsets[idx - 1] + SPLIT_MIN_THICC;
                    int hi = (idx == k - 1) ? max_s - SPLIT_MIN_THICC : offsets[idx + 1] - SPLIT_MIN_THICC;
                    if (hi < lo) hi = lo;
                    if (s < lo) s = lo;
                    if (s > hi) s = hi;
                    w->split_ofs[idx] = s;
                }
            }

            is_dirty(TRUE);
        } else if (g_mouse_capture_window->on_mouse_move) {
            int content_x = mouse_x - (g_mouse_capture_window->x + WIN_BORDER);
            int content_y = mouse_y - (g_mouse_capture_window->y + TITLEBAR_H + WIN_BORDER);
            g_mouse_capture_window->on_mouse_move(g_mouse_capture_window, content_x, content_y);
        }
    } else if (win_under_mouse && win_under_mouse->on_mouse_move) {
        int content_x = mouse_x - (win_under_mouse->x + WIN_BORDER);
        int content_y = mouse_y - (win_under_mouse->y + TITLEBAR_H + WIN_BORDER);

        if (win_under_mouse->split_panes_is_active) {
            int pane_index = -1;
            int rel_x = 0;
            int rel_y = 0;

            BOOL on_divider = FALSE;
            if (_get_pane_for_point(win_under_mouse, mouse_x, mouse_y, &pane_index, &rel_x, &rel_y, &on_divider)) {
                if (pane_index >= 0) {
                    win_under_mouse->active_pane = pane_index;
                }
            }
        }
        win_under_mouse->on_mouse_move(win_under_mouse, content_x, content_y);
    }


    if (mouse_down && !mouse_was_down) {
        if (win_under_mouse) {
            focus_win(win_under_mouse);

            int content_x_abs = win_under_mouse->x + WIN_BORDER;
            int content_y_abs = win_under_mouse->y + TITLEBAR_H + WIN_BORDER;

            if (mouse_y < content_y_abs) {
                int close_button_x1 = win_under_mouse->x + win_under_mouse->width - TITLEBAR_H - WIN_BORDER;
                int close_button_y1 = win_under_mouse->y + WIN_BORDER + 2;
                int close_button_x2 = close_button_x1 + TITLEBAR_H - 4;
                int close_button_y2 = close_button_y1 + TITLEBAR_H - 4;

                if (mouse_x >= close_button_x1 - 5 && mouse_x <= close_button_x2 + 5 &&
                    mouse_y >= close_button_y1 - 5 && mouse_y <= close_button_y2 + 5) {
                    close_win(win_under_mouse->id);
                    is_bg_dirty(TRUE);
                } else {
                    g_dragging_window = win_under_mouse;
                    g_drag_offset_x = mouse_x - win_under_mouse->x;
                    g_drag_offset_y = mouse_y - win_under_mouse->y;
                }
            } else {
                g_mouse_capture_window = win_under_mouse;
                g_last_mouse_down_win = win_under_mouse;
                g_last_mouse_down_x = mouse_x;
                g_last_mouse_down_y = mouse_y;

                BOOL started_divider_drag = FALSE;
                if (win_under_mouse->split_panes_is_active) {
                    int pane_index = -1;
                    int rel_x = 0;
                    int rel_y = 0;
                    BOOL on_divider = FALSE;
                    if (_get_pane_for_point(win_under_mouse, mouse_x, mouse_y, &pane_index, &rel_x, &rel_y, &on_divider)) {
                        if (on_divider) {
                            win_under_mouse->split_dragging = TRUE;
                            win_under_mouse->dragging_div_idx = pane_index; //div idx while on
                            started_divider_drag = TRUE;
                        } else if (pane_index >= 0) {
                            win_under_mouse->active_pane = pane_index;
                        }
                    }
                }
                if (!started_divider_drag && win_under_mouse->on_mouse_down) {
                    int content_x = mouse_x - content_x_abs;
                    int content_y = mouse_y - content_y_abs;
                    win_under_mouse->on_mouse_down(win_under_mouse, content_x, content_y);
                }
            }
        }
    }
    //released
    else if (!mouse_down && mouse_was_down) {
        //drag will wait for mouse up (release)
        if (g_mouse_capture_window) {
            if (g_mouse_capture_window->split_panes_is_active && g_mouse_capture_window->split_dragging) {
                g_mouse_capture_window->split_dragging = FALSE;
            } else if (g_mouse_capture_window->on_mouse_up) {
                int content_x = mouse_x - (g_mouse_capture_window->x + WIN_BORDER);
                int content_y = mouse_y - (g_mouse_capture_window->y + TITLEBAR_H + WIN_BORDER);
                g_mouse_capture_window->on_mouse_up(g_mouse_capture_window, content_x, content_y);
            }
            
            if (g_last_mouse_down_win == win_under_mouse) {
                int dx = mouse_x - g_last_mouse_down_x;
                int dy = mouse_y - g_last_mouse_down_y;
                if (dx*dx + dy*dy < 25) { //5px
                    if (win_under_mouse && win_under_mouse->on_click) {
                        int content_x = mouse_x - (win_under_mouse->x + WIN_BORDER);
                        int content_y = mouse_y - (win_under_mouse->y + TITLEBAR_H + WIN_BORDER);
                        if (win_under_mouse->split_panes_is_active) {
                            int pane_index = -1;
                            int rel_x = 0;
                            int rel_y = 0;
                            BOOL on_divider = FALSE;
                            if (_get_pane_for_point(win_under_mouse, mouse_x, mouse_y, &pane_index, &rel_x, &rel_y, &on_divider)) {
                                if (pane_index >= 0) {
                                    win_under_mouse->active_pane = pane_index;
                                }
                            }
                        }
                        win_under_mouse->on_click(win_under_mouse, content_x, content_y);
                    }
                }
            }
        }

        g_dragging_window = NULL;
        g_mouse_capture_window = NULL;
        g_last_mouse_down_win = NULL;
    }

    if (g_dragging_window) {
        int new_x = mouse_x - g_drag_offset_x;
        int new_y = mouse_y - g_drag_offset_y;

        int min_x = 0;
        int max_x = SCREEN_WIDTH - g_dragging_window->width;

        int min_y = 36;
        int max_y = SCREEN_HEIGHT - g_dragging_window->height;

        if (new_x < min_x) new_x = min_x;
        if (new_x > max_x) new_x = max_x;

        if (new_y < min_y) new_y = min_y;
        if (new_y > max_y) new_y = max_y;

        if (g_dragging_window->x != new_x || g_dragging_window->y != new_y) {
            g_dragging_window->x = new_x;
            g_dragging_window->y = new_y;
            
            is_bg_dirty(TRUE);
        }
    }
        
    mouse_was_down = mouse_down;
}

void m_update() {
    extern int g_mouse_x_pos, g_mouse_y_pos;
    extern MOUSE_STATUS g_status;
    
    m_win_event_handler(g_mouse_x_pos, g_mouse_y_pos, g_status.left_button);

    if (_kb_split()) _toggle_split();
    if (_kb_pixelate()) _toggle_pixelate();

    switchstate(g_mouse_x_pos, g_mouse_y_pos, g_status.left_button);
}

void close_win(int window_id) {
    int i, j;
    Window* win = NULL;
    
    for (i = 0; i < wincount; i++) {
        if (window_list[i] && window_list[i]->id == window_id) {
            win = window_list[i];
            break;
        }
    }
    
    if (!win) {
        return;
    }
    
    kprint("Closing: %s (ID: %d)\n", win->title, win->id);
    
    if (g_dragging_window == win) {
        g_dragging_window = NULL;
        g_mouse_down_on_title_bar = FALSE;
    }
    
    // store pid b4 freeze
    int pid = win->pid;
    
    if (win->on_close) {
        win->on_close(win);
    }

    unregister_for_process(pid);
    
    if (win->title) {
        kfree(win->title);
        win->title = NULL;
    }

    if (win->frame_cache) {
        kfree(win->frame_cache);
        win->frame_cache = NULL;
    }
    
    win->frame_cache_dirty = TRUE;
    win->last_x = 0;
    win->last_y = 0;
    
    kfree(win);
    
    if (i >= 0 && i < wincount) {
        for (j = i; j < wincount - 1; j++) {
            window_list[j] = window_list[j+1];
        }
        window_list[wincount - 1] = NULL;
        wincount--;
    }
    
    cleanup_process(pid);
    is_bg_dirty(TRUE);
}

void switchstate(int mouse_x, int mouse_y, BOOL mouse_down) {
    for (int i = 0; i < button_count; i++) {
        icon_button_t* btn = button_list[i];
        BUTTON_STATE old_state = btn->state;
        BOOL is_hovering = (mouse_x >= btn->x && mouse_x <= btn->x + btn->width &&
                            mouse_y >= btn->y && mouse_y <= btn->y + btn->height);

        if (is_hovering) {
            if (mouse_down) {
                btn->state = BUTTON_STATE_CLICKED;
            } else {
                btn->state = BUTTON_STATE_HOVER;
                if (old_state == BUTTON_STATE_CLICKED && btn->action) {
                    g_curr_r = btn->avg_r;
                    g_curr_g = btn->avg_g;
                    g_curr_b = btn->avg_b;
                    btn->action();
                }
            }
        } else {
            btn->state = BUTTON_STATE_NORMAL;
        }
        if (btn->state != old_state) is_dirty(TRUE);
    }
}

static uint32 g_mouse_save_buffer[32 * 32];
static int g_saved_mouse_x = -1;
static int g_saved_mouse_y = -1;
static int g_saved_mouse_w = 0;
static int g_saved_mouse_h = 0;

static void _restore_mouse_bg() {
    if (g_saved_mouse_x == -1) return;

    int screen_width = vbe_get_width();
    int screen_height = vbe_get_height();

    for (int y = 0; y < g_saved_mouse_h; y++) {
        int sy = g_saved_mouse_y + y;
        if (sy >= 0 && sy < screen_height) {
            for (int x = 0; x < g_saved_mouse_w; x++) {
                int sx = g_saved_mouse_x + x;
                if (sx >= 0 && sx < screen_width) {
                    g_back_buffer[sy * screen_width + sx] = g_mouse_save_buffer[y * 32 + x];
                }
            }
        }
    }
    g_saved_mouse_x = -1;
}

static void _save_mouse_bg(int x, int y, int w, int h) {
    if (w > 32) w = 32;
    if (h > 32) h = 32;

    int screen_width = vbe_get_width();
    int screen_height = vbe_get_height();

    g_saved_mouse_x = x;
    g_saved_mouse_y = y;
    g_saved_mouse_w = w;
    g_saved_mouse_h = h;

    for (int sy = 0; sy < h; sy++) {
        int py = y + sy;
        if (py >= 0 && py < screen_height) {
            for (int sx = 0; sx < w; sx++) {
                int px = x + sx;
                if (px >= 0 && px < screen_width) {
                    g_mouse_save_buffer[sy * 32 + sx] = g_back_buffer[py * screen_width + px];
                }
            }
        }
    }
}

void redraw_all() {
    extern int g_mouse_x_pos, g_mouse_y_pos;
    static int last_gui_mouse_x = -1;
    static int last_gui_mouse_y = -1;
    extern int cursor_width, cursor_height;

    BOOL mouse_moved = (g_mouse_x_pos != last_gui_mouse_x || g_mouse_y_pos != last_gui_mouse_y);
    
    if (!g_screen_dirty && !mouse_moved) {
        return;
    }

    int old_mouse_x = g_saved_mouse_x;
    int old_mouse_y = g_saved_mouse_y;
    int old_mouse_w = g_saved_mouse_w;
    int old_mouse_h = g_saved_mouse_h;

    _restore_mouse_bg();

    if (g_screen_dirty) {
        if (g_bg_dirty) {
            wllp_rendcache();
            g_bg_dirty = FALSE;
        }

        rect_grad(0, 0, SCREEN_WIDTH, 36, RGB(110, 80, 140), RGB(160, 120, 185));
        rect_grad(0, 0, SCREEN_WIDTH, 16, RGB(230, 210, 245), RGB(190, 160, 220));
        rect(0, 36, SCREEN_WIDTH, 1, RGB(225, 215, 250));

        static time_t t;
        static uint32_t last_clock_tick = 0;
        if (timer_ticks - last_clock_tick >= 18) {
            last_clock_tick = timer_ticks;
            get_time(&t);
        }
        draw_clock(&t, CLOCK_X, CLOCK_Y);

        for (int i = 0; i < button_count; i++) {
            icon_button_t* btn = button_list[i];
            button_v(btn->x, btn->y, btn->width, btn->height, btn->state, btn->avg_r, btn->avg_g, btn->avg_b);
            if (btn->icon_pixel_data) {
                predraw_bmp_icn(btn->icon_pixel_data, btn->icon_width, btn->icon_height, 
                                     btn->icon_row_padded,
                                     btn->x + (btn->width / 2) - 16, 
                                     btn->y + (btn->height / 2) - 16, 32, 32);
            }
        }

        notif();
        
        draw_all_win();
        
        // capture area below the new mouse
        int mw = (cursor_width > 0) ? cursor_width : 32; 
        int mh = (cursor_height > 0) ? cursor_height : 32;
        _save_mouse_bg(g_mouse_x_pos, g_mouse_y_pos, mw, mh);

        extern void m_render(int x, int y);
        m_render(g_mouse_x_pos, g_mouse_y_pos);

        swapbuf();
        g_screen_dirty = FALSE;
    } else if (mouse_moved) {
        int mw = (cursor_width > 0) ? cursor_width : 32; 
        int mh = (cursor_height > 0) ? cursor_height : 32;
        _save_mouse_bg(g_mouse_x_pos, g_mouse_y_pos, mw, mh);

        extern void m_render(int x, int y);
        m_render(g_mouse_x_pos, g_mouse_y_pos);

        if (old_mouse_x != -1) {
            swapbuf_region(old_mouse_x, old_mouse_y, old_mouse_w, old_mouse_h);
        }
        swapbuf_region(g_mouse_x_pos, g_mouse_y_pos, mw, mh);
    }

    last_gui_mouse_x = g_mouse_x_pos;
    last_gui_mouse_y = g_mouse_y_pos;
}

BOOL is_drag_win(void) {
    return g_dragging_window != NULL;
}

void free_icn(icon_button_t* btn) {
    if (btn) {
        if (btn->icon_pixel_data) {
            kfree(btn->icon_pixel_data);
            btn->icon_pixel_data = NULL;
        }
        if (btn->icon_path) {
            kfree(btn->icon_path);
            btn->icon_path = NULL;
        }
        kfree(btn);
    }
}

BOOL sample_icn(const char* icon_path, uint8* r, uint8* g, uint8* b) {
    if (!icon_path) return FALSE;
    for (int i = 0; i < button_count; i++) {
        if (button_list[i]->icon_path && strcmp(button_list[i]->icon_path, icon_path) == 0) {
            if (r) *r = button_list[i]->avg_r;
            if (g) *g = button_list[i]->avg_g;
            if (b) *b = button_list[i]->avg_b;
            return TRUE;
        }
    }
    return FALSE;
}

void handle_scroll_event(int mouse_x, int mouse_y, int scroll_delta) {
    valid_refs();

    Window* win_under_mouse = NULL;
    for (int i = wincount - 1; i >= 0; i--) {
        Window* win = window_list[i];
        if (!win) continue;
        if (mouse_x >= win->x && mouse_x < win->x + win->width &&
            mouse_y >= win->y && mouse_y < win->y + win->height) {
            win_under_mouse = win;
            break;
        }
    }

    if (win_under_mouse && win_under_mouse->on_scroll) {
        win_under_mouse->on_scroll(win_under_mouse, scroll_delta);
        is_dirty(TRUE);
    }
}

void gui_cleanup() {
    for (int i = 0; i < button_count; i++) {
        if (button_list[i]) {
            free_icn(button_list[i]);
            button_list[i] = NULL;
        }
    }
    button_count = 0;

    if (g_close_icon) {
        free_preloaded_bmp(g_close_icon);
        g_close_icon = NULL;
    }

    g_dragging_window = NULL;
    g_mouse_down_on_title_bar = FALSE;

    for (int i = 0; i < wincount; i++) {
        if (window_list[i]) {
            close_win(window_list[i]->id);
        }
    }
}

Window* get_active_win() {
    if (wincount > 0) {
        return window_list[wincount - 1];
    }
    return NULL;
}

Window* get_win_by_pid(int pid) {
    for (int i = 0; i < wincount; i++) {
        if (window_list[i] && window_list[i]->pid == pid) {
            return window_list[i];
        }
    }
    return NULL;
}

#define CLAMP_U8(v) ((uint8)((int)(v) > 255 ? 255 : ((int)(v) < 0 ? 0 : (int)(v))))

void button_v(int x, int y, int width, int height, BUTTON_STATE state, uint8 r, uint8 g, uint8 b) {
    int ri = r, gi = g, bi = b;

    int gray = (ri + gi + bi) / 3;
    ri = (ri * 7 / 10) + (gray * 3 / 10) + 12;
    gi = (gi * 7 / 10) + (gray * 3 / 10);
    bi = (bi * 7 / 10) + (gray * 3 / 10);

    ri = ri > 255 ? 255 : ri;

    uint32 face = RGB(ri, gi, bi);
    uint32 light_shad = RGB(CLAMP_U8(ri + 20), CLAMP_U8(gi + 20), CLAMP_U8(bi + 10));
    uint32 dark_shad = RGB(ri * 7 / 10, gi * 7 / 10, bi * 7 / 10);

    uint32 face_color = face;
    uint32 top_left_shadow = light_shad;
    uint32 bottom_right_shadow = dark_shad;

    if (state == BUTTON_STATE_HOVER) {
        face_color = RGB(ri * 9 / 10, gi * 9 / 10, bi * 9 / 10);
    } else if (state == BUTTON_STATE_CLICKED) {
        face_color = RGB(CLAMP_U8(ri - 15), CLAMP_U8(gi - 15), CLAMP_U8(bi - 10));
        top_left_shadow = dark_shad;
        bottom_right_shadow = light_shad;
    }

    rect(x, y, width, height, light_shad);
    rect(x + 2, y + 2, width - 2, height - 2, bottom_right_shadow);
    rect(x, y, width - 2, height - 2, top_left_shadow);
    rect(x + 2, y + 2, width - 4, height - 4, face_color);
}

void button(int x, int y, int width, int height, BUTTON_STATE state) {
    button_v(x, y, width, height, state, 195, 180, 225);
}

void icon(int x, int y, int width, int height, const char* icon_path, button_action_t action) {
    if (button_count >= MAX_BUTTONS) return;

    icon_button_t* btn = (icon_button_t*)kmalloc(sizeof(icon_button_t));
    btn->x = x;
    btn->y = y;
    btn->width = width;
    btn->height = height;
    btn->action = action;
    btn->state = BUTTON_STATE_NORMAL;
    
    if (icon_path) {
        btn->icon_path = (char*)kmalloc(strlen(icon_path) + 1);
        strcpy(btn->icon_path, icon_path);
    } else {
        btn->icon_path = NULL;
    }

    char* bmp_data = fat_read_file((char*)icon_path);
    if (bmp_data) {
        BMP_FILE_H *fh = (BMP_FILE_H *)bmp_data;
        BMP_INFO_H *ih = (BMP_INFO_H *)(bmp_data + sizeof(*fh));

        if (fh->type == 0x4D42 && ih->bpp == 24 && ih->width > 0 && ih->height > 0) {
            btn->icon_width = ih->width;
            btn->icon_height = ih->height;
            btn->icon_row_padded = (btn->icon_width * 3 + 3) & ~3;
            uint32 alloc_size = btn->icon_row_padded * btn->icon_height;
            btn->icon_pixel_data = (char*)kmalloc(alloc_size);
            memcpy(btn->icon_pixel_data, bmp_data + fh->offset, alloc_size);

            uint32 r_sum = 0, g_sum = 0, b_sum = 0;
            int samples = 0;

            int row_stride = (btn->icon_height > 5) ? btn->icon_height / 5 : 1;
            int col_stride = (btn->icon_width > 5) ? (btn->icon_width / 5) * 3 : 3;

            for (int row = 0; row < btn->icon_height; row += row_stride) {
                char *row_ptr = btn->icon_pixel_data
                            + (btn->icon_height - 1 - row) * btn->icon_row_padded;
                for (int col = 0; col < btn->icon_width * 3; col += col_stride) {
                    uint8 pb = (uint8)row_ptr[col];
                    uint8 pg = (uint8)row_ptr[col + 1];
                    uint8 pr = (uint8)row_ptr[col + 2];

                    int lo = pr < pg ? pr : pg; lo = lo < pb ? lo : pb;
                    int hi = pr > pg ? pr : pg; hi = hi > pb ? hi : pb;
                    if (hi - lo < 20) continue;

                    r_sum += pr; g_sum += pg; b_sum += pb;
                    samples++;
                }
            }

            if (samples > 0) {
                btn->avg_r = (uint8)(r_sum / samples);
                btn->avg_g = (uint8)(g_sum / samples);
                btn->avg_b = (uint8)(b_sum / samples);
                // kprint("avg: r=%d g=%d b=%d samples=%d\n", btn->avg_r, btn->avg_g, btn->avg_b, samples);
            } else {
                btn->avg_r = 195; btn->avg_g = 180; btn->avg_b = 225;
            }
        } else {
            btn->icon_pixel_data = NULL;
        }
        kfree(bmp_data);
    } else btn->icon_pixel_data = NULL;

    button_list[button_count++] = btn;
}

#define NOTIFICATION_WIDTH 250
#define NOTIFICATION_HEIGHT 80
#define NOTIFICATION_DURATION 50

typedef struct {
    char title[64];
    char message[128];
    uint32 start_ticks;
    BOOL active;
} notification_t;

static notification_t g_notification;

void notif_handler(const char* title, const char* message) {
    // coooldown
    static uint32 last_notify_ticks = 0;
    if (timer_ticks - last_notify_ticks < 500 && g_notification.active) {
        return;
    }

    memset(g_notification.title, 0, sizeof(g_notification.title));
    memset(g_notification.message, 0, sizeof(g_notification.message));
    
    strncpy(g_notification.title, title, 63);
    g_notification.title[63] = '\0';
    strncpy(g_notification.message, message, 127);
    g_notification.message[127] = '\0';
    
    g_notification.start_ticks = timer_ticks;
    last_notify_ticks = timer_ticks;
    g_notification.active = TRUE;
    is_bg_dirty(TRUE);
}

#define MAX_LINES 10
#define MAX_LINE_LEN 128

#define TITLEBAR_H 20
#define PADDING 10
#define B_PAD 12
#define LINE_SPACING 4

static int wrap_text(const char* input, char lines[MAX_LINES][MAX_LINE_LEN], int max_chars) {
    int line = 0;

    while (*input && line < MAX_LINES) {
        int len = 0;
        int last_space = -1;

        while (input[len] && len < max_chars) {
            if (input[len] == ' ') last_space = len;
            len++;
        }

        int split = len;

        if (input[len]) {
            if (last_space != -1) {
                split = last_space; // break at word
            } else {
                split = max_chars; // force break long word
            }
        }

        memcpy(lines[line], input, split);
        lines[line][split] = '\0';

        input += split;

        while (*input == ' ') input++;

        line++;
    }

    return line;
}

static void notif() {
    if (!g_notification.active) return;

    if (timer_ticks - g_notification.start_ticks > NOTIFICATION_DURATION) {
        g_notification.active = FALSE;
        is_bg_dirty(TRUE);
        return;
    }

    int screen_w = vbe_get_width();
    int screen_h = vbe_get_height();

    int max_chars = (NOTIFICATION_WIDTH - 20) / 10;

    char lines[MAX_LINES][MAX_LINE_LEN];
    int lc = wrap_text(g_notification.message, lines, max_chars);

    int lh = 8 + LINE_SPACING;

    int content_height = lc * lh +
                         (lc - 1) * LINE_SPACING;

    int notif_height = TITLEBAR_H +
                       content_height +
                       (PADDING * 2) + B_PAD;

    int x = screen_w - NOTIFICATION_WIDTH - 10;
    int y = screen_h - notif_height - 10;

    rect(x, y, NOTIFICATION_WIDTH, notif_height,
         RGB(230, 200, 250));
    
    rect(x + 1, y + 1, NOTIFICATION_WIDTH - 2, notif_height - 2,
         RGB(140, 100, 180));
    
    rect(x + 2, y + 2, NOTIFICATION_WIDTH - 4, notif_height - 4,
         RGB(240, 230, 255));
    
    rect_grad(x + 2, y + 2,
              NOTIFICATION_WIDTH - 4,
              TITLEBAR_H,
              RGB(140, 90, 200),
              RGB(100, 60, 160));

    text(g_notification.title,
         x + 10,
         y + 4,
         RGB(255, 255, 255),
         FONT_KALNIA,
         FALSE);

    int ty = y + TITLEBAR_H + PADDING;
    
    for (int i = 0; i < lc; i++) {
        text(lines[i],
             x + 10,
             ty,
             RGB(60, 40, 80),
             FONT_KALNIA,
             FALSE);

        ty += lh + LINE_SPACING;
        if (lc == MAX_LINES && g_notification.message[0] != '\0') {
            int len = strlen(lines[MAX_LINES - 1]);
            if (len > 3) {
                lines[MAX_LINES - 1][len - 3] = '.';
                lines[MAX_LINES - 1][len - 2] = '.';
                lines[MAX_LINES - 1][len - 1] = '.';
            }
        }
    }
}

void desktop() {
    wllp_rendcache();

    rect_grad(0, 0, SCREEN_WIDTH, 36, RGB(192, 165, 202), RGB(197, 151, 208));

    time_t t;
    get_time(&t);
    draw_clock(&t, CLOCK_X, CLOCK_Y);

    for (int i = 0; i < button_count; i++) {
        icon_button_t* btn = button_list[i];
        button_v(btn->x, btn->y, btn->width, btn->height, btn->state,
                btn->avg_r, btn->avg_g, btn->avg_b);
        if (btn->icon_pixel_data) {
            predraw_bmp_icn(btn->icon_pixel_data, btn->icon_width, btn->icon_height,
                            btn->icon_row_padded,
                            btn->x + (btn->width / 2) - 16,
                            btn->y + (btn->height / 2) - 16, 32, 32);
        }
    }

    notif();
}