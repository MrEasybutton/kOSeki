#include "apps/Reaper.h"
#include "apps/html_engine.h"
#include "procsys.h"
#include "gui.h"
#include "string.h"
#include "graphics.h"
#include "bmp.h"
#include "kheap.h"
#include "vesa.h"
#include "serial.h"
#include "keyboard.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "lwip/altcp.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "mbedtls/ssl.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_MALLOC(sz) kmalloc(sz)
#define STBI_REALLOC(p,sz) krealloc(p,sz)
#define STBI_FREE(p) kfree(p)
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_GIF
#define STBI_ONLY_BMP
#define STBI_SUPPORT_ZLIB
#define STBI_ASSERT(x)
#define NDEBUG
#include "apps/stb_image.h"

extern uint32 timer_ticks;

#define WIN_BORDER 2
#define TITLEBAR_H 20
#define TAB_BAR_HEIGHT 24
#define URL_BAR_HEIGHT 30
#define STATUS_BAR_HEIGHT 18
#define BUTTON_WIDTH 40
#define BUTTON_HEIGHT 20
#define BACK_BUTTON_WIDTH 30
#define MAX_TABS 8
#define MAX_HISTORY 32
#define TAB_MAX_WIDTH 120
#define TAB_MIN_WIDTH 60
#define INPUT_HEIGHT 22
#define SUBMIT_HEIGHT 22
#define INPUT_PADDING_X 6
#define INPUT_PADDING_Y 3

#define COLOR_BG_DARK RGB(210, 160, 175)
#define COLOR_BG_LIGHT RGB(122, 13, 32)
#define COLOR_ACCENT_PRIMARY RGB(180, 80, 110)
#define COLOR_TEXT_PRIMARY RGB( 60, 20, 35)
#define COLOR_TEXT_SECONDARY RGB(130, 70, 90)
#define COLOR_BORDER RGB(195, 140, 158)
#define COLOR_PAGE_BG RGB(255, 245, 248)
#define COLOR_SCROLLBAR RGB(220, 170, 185)
#define COLOR_THUMB RGB(180, 110, 135)
#define COLOR_THUMB_HOV RGB(150, 80, 110)
#define COLOR_BTN_HOV RGB(225, 175, 190)
#define COLOR_TAB_ACTIVE RGB(255, 240, 245)
#define COLOR_TAB_INACTIVE RGB(200, 155, 170)
#define COLOR_TAB_HOV RGB(230, 195, 210)
#define COLOR_STATUS_BG RGB(200, 155, 170)
#define COLOR_STATUS_TEXT RGB( 60, 20, 35)
#define COLOR_BACK_DISABLED RGB(180, 145, 158)
#define COLOR_INPUT_BG RGB(255, 255, 255)
#define COLOR_INPUT_BORDER RGB(160, 120, 140)
#define COLOR_INPUT_FOCUSED RGB(100, 60, 180)
#define COLOR_SUBMIT_BG RGB(180, 80, 110)
#define COLOR_SUBMIT_HOV RGB(210, 100, 130)
#define COLOR_SUBMIT_TEXT RGB(255, 255, 255)

static const char HOMEPAGE_HTML[] =
    "<h1>REAPER BROWSER</h1>"
    "<p>>>>>>>></p>"
    "<form action=\"https://lite.duckduckgo.com/lite/\" method=\"get\">"
    "<input type=\"text\" name=\"q\" placeholder=\"Cobblin D\" size=\"40\">"
    "<input type=\"submit\" value=\"Search\">"
    "</form>"
    "<h1>\n</h1>"
    "<img src=\"/SYSTEM/moreap.bmp\" width=\"150\" height=\"150\" alt=\"[MORI]\">";

typedef struct {
    char entries[MAX_HISTORY][1024];
    int count, current;
} BrowserHistory;

static void text_clip_b(const char* str, int x, int y, int right_margin, int view_y, int view_h, uint32 color, BOOL bold);
static void rect_clipped(int x, int y, int w, int h, int right_margin, int view_y, int view_h, uint32 color);
static void draw_rect_outline_clipped(int x, int y, int w, int h, int right_margin, int view_y, int view_h, uint32 color);
static void draw_rect_outline(int x, int y, int w, int h, uint32 color);
static int eval_visible(RenderList* list, int left_margin, int right_margin);

static void history_init(BrowserHistory* h) { memset(h, 0, sizeof(*h)); h->current = -1; }

static void history_push(BrowserHistory* h, const char* url) {
    h->count = h->current + 1;
    if (h->count >= MAX_HISTORY) {
        for (int i = 0; i < MAX_HISTORY - 1; i++)
            memcpy(h->entries[i], h->entries[i + 1], 1024);
        h->count = MAX_HISTORY - 1;
        h->current = MAX_HISTORY - 2;
    }
    strncpy(h->entries[++h->current], url, 1023);
    h->entries[h->current][1023] = '\0';
    h->count++;
}


static BOOL history_can_go_back(BrowserHistory* h) { return h->current > 0; }
static const char* history_go_back(BrowserHistory* h) {
    if (!history_can_go_back(h)) return NULL;
    return h->entries[--h->current];
}

// tab states n shi
typedef struct {
    char url[1024];
    BOOL is_loading;
    int scroll_offset, content_height, view_height;
    char* html_buffer;
    int html_buffer_len, html_buffer_size;
    char pending_url[1024];
    int redirect_ticks;
    RenderList render_list;
    BrowserHistory history;
    char title[64];
    char status[128];
    int id;
} BrowserTab;

typedef struct {
    BrowserTab tabs[MAX_TABS];
    int tab_count, active_tab;
    int mouse_hover_go, mouse_hover_back, mouse_hover_tab;
    int mouse_hover_newtab, link_hover_idx, hover_submit_idx;
    int mouse_hover_close;
    int next_tab_id;
    BOOL sb_is_dragging, sb_is_hovered;
    int drag_start_y, drag_start_offset;
    int bar_x, bar_y, bar_width, bar_height;
    int thumb_x, thumb_y, thumb_width, thumb_height;
    BOOL url_bar_focused;
    int url_cursor_pos;
    int url_view_offset;
} BrowserMeta;

typedef struct { char host[128]; int port; char path[1024]; } ParsedURL;

typedef struct {
    int pid, tab_id, port;
    char* host;
    char* path;
    BOOL is_https;
    char* post_body;
    struct altcp_tls_config* tls_config;
} BrowserFetchCtx;

typedef struct {
    int pid, tab_id, elem_index, port;
    char* host;
    char* path;
    BOOL is_https;
    // for raw response
    char* buf;
    int buf_len, buf_size;
    struct altcp_tls_config* tls_config;
    int redirect_count;
} ImageFetchCtx;

static void imgctx_free(ImageFetchCtx* ctx)
{
    if (!ctx) return;
    if (ctx->tls_config) { altcp_tls_free_config(ctx->tls_config); ctx->tls_config = NULL; }
    if (ctx->buf) kfree(ctx->buf);
    if (ctx->host) kfree(ctx->host);
    if (ctx->path) kfree(ctx->path);
    kfree(ctx);
}

static void draw_image(RenderElement* el, int x, int y, int right_margin,
                       int view_y, int view_h)
{
    if (!el->pixels || el->img_width <= 0 || el->img_height <= 0) {
        int bw = el->img_width > 0 ? el->img_width : 64;
        int bh = el->img_height > 0 ? el->img_height : 64;
        if (y + bh <= view_y || y >= view_y + view_h) return;
        rect_clipped(x, y, bw, bh, right_margin, view_y, view_h, RGB(200, 200, 200));
        draw_rect_outline_clipped(x, y, bw, bh, right_margin, view_y, view_h, RGB(160, 160, 160));
        if (el->alt_text[0])
            text_clip_b(el->alt_text, x + 4, y + 4,
                      right_margin, view_y, view_h, RGB(80, 80, 80), FALSE);
        return;
    }

    if (el->frame_count > 1 && el->frames && el->delays) {
        uint32 now = timer_ticks;
        int delay_ticks = (el->delays[el->current_frame] * 18) / 100;
        if (delay_ticks < 1) delay_ticks = 1;
        
        if (now - el->last_frame_ticks >= (uint32)delay_ticks) {
            el->current_frame = (el->current_frame + 1) % el->frame_count;
            el->pixels = el->frames[el->current_frame];
            el->last_frame_ticks = now;
            is_dirty(TRUE);
        }
    }

    int sw = vbe_get_width(), sh = vbe_get_height();
    int iw = el->img_width, ih = el->img_height;

    for (int row = 0; row < ih; row++) {
        int py = y + row;
        if (py < view_y || py >= view_y + view_h) continue;
        if (py < 0 || py >= sh) continue;
        for (int col = 0; col < iw; col++) {
            int px = x + col;
            if (px < 0 || px >= sw || px >= right_margin) break;
            uint32 argb = el->pixels[row * iw + col];
            uint8 a = (argb >> 24) & 0xFF;
            if (a == 0) continue;
            uint8 r = (argb >> 16) & 0xFF;
            uint8 g = (argb >> 8) & 0xFF;
            uint8 b = argb & 0xFF;
            if (a == 0xFF) {
                rect(px, py, 1, 1, RGB(r, g, b));
            } else {
                uint8 inv = 255 - a;
                r = (uint8)((r * a + 255 * inv) >> 8);
                g = (uint8)((g * a + 255 * inv) >> 8);
                b = (uint8)((b * a + 255 * inv) >> 8);
                rect(px, py, 1, 1, RGB(r, g, b));
            }
        }
    }
}

void search(Window* win);
static void browser_fetch(Window* win, const char* url, const char* post_body);
static void browser_fetch_images(Window* win, BrowserTab* t);
static void resp_form(Window* win, int form_id);

static BrowserTab* active_tab(BrowserMeta* d) { return &d->tabs[d->active_tab]; }

static void tab_init(BrowserTab* t, int id) {
    memset(t, 0, sizeof(*t));
    t->id = id;
    t->html_buffer_size = 4096;
    t->html_buffer = (char*)kmalloc(t->html_buffer_size);
    t->html_buffer[0] = '\0';
    history_init(&t->history);
    strcpy(t->title, "Home");
}

static void tab_free(BrowserTab* t) {
    html_free_list(&t->render_list);
    if (t->html_buffer) { kfree(t->html_buffer); t->html_buffer = NULL; }
}

static int find_tab_by_id(BrowserMeta* bd, int tab_id) {
    if (!bd) return -1;
    for (int i = 0; i < bd->tab_count; i++) {
        if (bd->tabs[i].id == tab_id) return i;
    }
    return -1;
}

static void close_tab(Window* win, int index) {
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    BrowserMeta* bd = (BrowserMeta*)p->data;
    if (index < 0 || index >= bd->tab_count) return;

    tab_free(&bd->tabs[index]);

    for (int i = index; i < bd->tab_count - 1; i++) {
        bd->tabs[i] = bd->tabs[i + 1];
    }
    bd->tab_count--;

    bd->mouse_hover_tab = -1;
    bd->mouse_hover_close = -1;

    if (bd->tab_count == 0) {
        close_win(win->id);
        return;
    }

    if (bd->active_tab >= bd->tab_count) {
        bd->active_tab = bd->tab_count - 1;
    }

    is_dirty(TRUE);
}

static void derive_title(BrowserTab* t, const char* url) {
    const char* p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    if (strncmp(p, "https://", 8) == 0) p += 8;
    int i = 0;
    while (*p && *p != '/' && *p != ':' && i < 62) t->title[i++] = *p++;
    t->title[i] = '\0';
    if (i == 0) strcpy(t->title, "New Tab");
}

static void tab_load_homepage(BrowserTab* t) {
    t->scroll_offset = 0;
    t->is_loading = FALSE;
    html_free_list(&t->render_list);
    html_parse(HOMEPAGE_HTML, &t->render_list);
    strcpy(t->title, "Home");
    strcpy(t->status, "guh~");
    strcpy(t->url, "");
}

static void parse_url(const char* url, ParsedURL* out) {
    memset(out, 0, sizeof(*out));
    out->port = 80;
    strcpy(out->path, "/");
    const char* p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    else if (strncmp(p, "https://", 8) == 0) { p += 8; out->port = 443; }
    int i = 0;
    while (*p && *p != ':' && *p != '/' && i < 127) out->host[i++] = *p++;
    out->host[i] = '\0';
    if (*p == ':') { p++; out->port = 0; while (*p >= '0' && *p <= '9') out->port = out->port * 10 + (*p++ - '0'); }
    if (*p == '/') { strncpy(out->path, p, 1023); out->path[1023] = '\0'; }
}

static void resolve_url(const char* base_url, const char* href, char* out, int out_len) {
    if (!href || !href[0]) {
        out[0]='\0';
        return;
    }

    //alr absolute
    if (!strncmp(href,"http://",7) ||
        !strncmp(href,"https://",8))
    {
        strncpy(out,href,out_len-1);
        out[out_len-1]='\0';
        return;
    }

    ParsedURL base;
    parse_url(base_url,&base);

    const char* proto =
        !strncmp(base_url,"https://",8)
        ? "https"
        : "http";

    //relative to protocol
    if (!strncmp(href,"//",2)) {
        snprintf(out,out_len, "%s:%s", proto,href);
        return;
    }

    // relative to root
    if (href[0]=='/') {
        snprintf(out,out_len, "%s://%s%s", proto, base.host, href);
        return;
    }

    char dir[1024];
    strncpy(dir,base.path,sizeof(dir)-1);
    dir[sizeof(dir)-1]='\0';

    char* last=strrchr(dir,'/');

    if (last)
        *(last+1)='\0';
    else
        strcpy(dir,"/");

    snprintf(out,out_len,
             "%s://%s%s%s",
             proto,
             base.host,
             dir,
             href);

    kprint(
        "resolve: %s + %s -> %s\n",
        base_url,
        href,
        out
    );
}

static void rect_clipped(int x, int y, int w, int h, int right_margin, int view_y, int view_h, uint32 color) {
    if (y + h <= view_y || y >= view_y + view_h) return;
    int dy = y, dh = h;
    if (dy < view_y) { dh -= (view_y - dy); dy = view_y; }
    if (dy + dh > view_y + view_h) { dh = (view_y + view_h) - dy; }
    if (dh <= 0) return;

    int dx = x, dw = w;
    if (dx + dw > right_margin) { dw = right_margin - dx; }
    if (dx < 0) { dw += dx; dx = 0; }
    if (dw <= 0) return;
    
    rect(dx, dy, dw, dh, color);
}

static void draw_rect_outline_clipped(int x, int y, int w, int h, int right_margin, int view_y, int view_h, uint32 color) {
    rect_clipped(x, y, w, 1, right_margin, view_y, view_h, color);
    rect_clipped(x, y + h - 1, w, 1, right_margin, view_y, view_h, color);
    rect_clipped(x, y, 1, h, right_margin, view_y, view_h, color);
    rect_clipped(x + w - 1, y, 1, h, right_margin, view_y, view_h, color);
}

static void draw_rect_outline(int x, int y, int w, int h, uint32 color) {
    rect(x, y, w, 1, color);
    rect(x, y + h - 1, w, 1, color);
    rect(x, y, 1, h, color);
    rect(x + w - 1, y, 1, h, color);
}

static void ctx_free(BrowserFetchCtx* ctx) {
    if (ctx->tls_config) { altcp_tls_free_config(ctx->tls_config); ctx->tls_config = NULL; }
    if (ctx->post_body) { kfree(ctx->post_body); ctx->post_body = NULL; }
    kfree(ctx->host); kfree(ctx->path); kfree(ctx);
}

static int hex_to_int(const char* s, char** end) {
    int res = 0;
    while (*s) {
        char c = *s;
        if (c >= '0' && c <= '9') res = res * 16 + (c - '0');
        else if (c >= 'a' && c <= 'f') res = res * 16 + (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') res = res * 16 + (c - 'A' + 10);
        else break;
        s++;
    }
    if (end) *end = (char*)s;
    return res;
}

static int decode_chunked_bin(unsigned char* body, int len) {
    unsigned char *src = body, *dst = body;
    unsigned char *end = body + len;
    int total = 0;
    while (src < end) {
        char* hex_end;
        int size = hex_to_int((char*)src, &hex_end);
        if (src == (unsigned char*)hex_end) break;
        src = (unsigned char*)hex_end;
        // skip chunk extensions e.g. ;... then find crlf
        while (src < end && *src != '\r' && *src != '\n') src++;
        while (src < end && (*src == '\r' || *src == '\n')) src++;
        if (size <= 0) break;
        if (src + size > end) size = (int)(end - src);
        memmove(dst, src, size);
        dst += size; src += size; total += size;
        while (src < end && (*src == '\r' || *src == '\n')) src++;
    }
    return total;
}

static void decode_chunked(char* body) {
    char *src = body, *dst = body;
    while (*src) {
        char* end; int size = hex_to_int(src, &end);
        if (src == end) break;
        src = end;
        while (*src == '\r' || *src == '\n') src++;
        if (size == 0) break;
        for (int i = 0; i < size && *src; i++) *dst++ = *src++;
        while (*src == '\r' || *src == '\n') src++;
    }
    *dst = '\0';
}

void reaper_check_redirect(void) {
    Process* bp = get_process_by_name("Reaper Browser");
    if (!bp || !bp->data) return;
    BrowserMeta* bd = (BrowserMeta*)bp->data;
    for (int i = 0; i < bd->tab_count; i++) {
        BrowserTab* t = &bd->tabs[i];
        if (t->redirect_ticks > 0 && --t->redirect_ticks == 0 && i == bd->active_tab)
            search(bp->window);
    }
}

static BOOL browser_process_response(BrowserFetchCtx* ctx, struct altcp_pcb* tpcb,
                                     BrowserMeta* bd, int tab_index)
{
    BrowserTab* t = &bd->tabs[tab_index];
    if (!t || !t->html_buffer) return FALSE;

    char* body = strstr(t->html_buffer, "\r\n\r\n");
    if (!body) { html_parse(t->html_buffer, &t->render_list); return FALSE; }
    *body = '\0'; body += 4;

    kprint("Browser[tab%d]: Headers:\n%s\n", tab_index, t->html_buffer);

    // 3xx redirect
    BOOL is_redirect = FALSE;
    char* status_end = strstr(t->html_buffer, "\r\n");
    if (status_end) { char* sp = strchr(t->html_buffer, ' '); if (sp && sp < status_end) { while (*sp == ' ') sp++; if (sp[0] == '3') is_redirect = TRUE; } }

    if (is_redirect) {
        char* loc = strcasestr(t->html_buffer, "Location: ");
        if (loc) {
            loc += 10;
            char* end = strstr(loc, "\r\n"); if (end) *end = '\0';
            while (*loc == ' ') loc++;
            char new_url[1024];
            if (strncmp(loc, "http", 4) == 0) strncpy(new_url, loc, 1023);
            else if (loc[0] == '/') {
                const char* proto = ctx->is_https ? "https" : "http";
                if (ctx->port != 80 && ctx->port != 443)
                    snprintf(new_url, sizeof(new_url), "%s://%s:%d%s", proto, ctx->host, ctx->port, loc);
                else
                    snprintf(new_url, sizeof(new_url), "%s://%s%s", proto, ctx->host, loc);
            } else strncpy(new_url, loc, 1023);
            new_url[1023] = '\0';
            strncpy(t->url, new_url, 1023);
            kprint("Browser[tab%d]: Redirect -> %s\n", tab_index, t->url);
            altcp_close(tpcb); ctx_free(ctx); t->redirect_ticks = 2;
            return TRUE;
        }
    }

    if (strcasestr(t->html_buffer, "Transfer-Encoding: chunked")) decode_chunked(body);
    html_parse(body, &t->render_list);

    for (int i = 0; i < t->render_list.count; i++) {
        RenderElement* el = &t->render_list.elements[i];

        if (el->type != ELEM_IMAGE)
            continue;

        if (!el->src_url[0])
            continue;

        char resolved[256];

        resolve_url(t->url, el->src_url, resolved, sizeof(resolved));

        strncpy(el->src_url, resolved, sizeof(el->src_url)-1);
        el->src_url[sizeof(el->src_url)-1] = '\0';
    }
    
    derive_title(t, t->url);
    snprintf(t->status, sizeof(t->status), "> %s", t->url);
    return FALSE;
}

static err_t browser_recv_callback(void* arg, struct altcp_pcb* tpcb,
                                   struct pbuf* p, err_t err)
{
    BrowserFetchCtx* ctx = (BrowserFetchCtx*)arg;
    BrowserMeta* bd = get_process(ctx->pid) ? (BrowserMeta*)get_process(ctx->pid)->data : NULL;
    int tab_index = find_tab_by_id(bd, ctx->tab_id);
    BrowserTab* t = (bd && tab_index >= 0)
                                ? &bd->tabs[tab_index] : NULL;

    if (p == NULL) {
        kprint("Browser[tab%d]: Connection closed (err=%d)\n", tab_index, err);
        if (t && t->is_loading) {
            t->is_loading = FALSE; t->scroll_offset = 0;
            html_free_list(&t->render_list);
            if (browser_process_response(ctx, tpcb, bd, tab_index)) return ERR_OK;
        }
        altcp_close(tpcb);

        //img fetches start here
        if (t) { Window* _w = get_active_win();
                  if (_w && _w->pid == ctx->pid) browser_fetch_images(_w, t); }
        ctx_free(ctx); is_dirty(TRUE);
        return ERR_OK;
    }

    if (t) {
        int new_len = t->html_buffer_len + p->tot_len;
        if (new_len + 1 > t->html_buffer_size) {
            int new_size = t->html_buffer_size * 2;
            if (new_size < new_len + 1) new_size = new_len + 16384;
            char* nb = (char*)krealloc(t->html_buffer, new_size);
            if (nb) { t->html_buffer = nb; t->html_buffer_size = new_size; }
        }
        if (t->html_buffer && new_len + 1 <= t->html_buffer_size) {
            pbuf_copy_partial(p, t->html_buffer + t->html_buffer_len, p->tot_len, 0);
            t->html_buffer_len = new_len;
            t->html_buffer[t->html_buffer_len] = '\0';
            snprintf(t->status, sizeof(t->status), "Receiving... %d bytes", t->html_buffer_len);
            altcp_recved(tpcb, p->tot_len);
            is_dirty(TRUE);
        } else {
            kprint("Browser[tab%d]: Buffer limit!\n", tab_index);
            altcp_recved(tpcb, p->tot_len);
        }

        char* hdr_end = strstr(t->html_buffer, "\r\n\r\n");
        if (hdr_end && t->is_loading) {
            BOOL done = FALSE;
            char* cl = strcasestr(t->html_buffer, "Content-Length: ");
            if (cl) {
                cl += 16; int clen = 0;
                while (*cl >= '0' && *cl <= '9') clen = clen * 10 + (*cl++ - '0');
                if (t->html_buffer_len - (int)(hdr_end - t->html_buffer) - 4 >= clen) done = TRUE;
            } else if (strcasestr(t->html_buffer, "Transfer-Encoding: chunked")) {
                if (t->html_buffer_len >= 5 &&
                    strcmp(t->html_buffer + t->html_buffer_len - 5, "0\r\n\r\n") == 0)
                    done = TRUE;
            }
            if (done) {
                t->is_loading = FALSE; t->scroll_offset = 0;
                html_free_list(&t->render_list);
                pbuf_free(p);
                if (browser_process_response(ctx, tpcb, bd, tab_index)) return ERR_OK;
                altcp_close(tpcb);

                //fetch img starts here
                { Window* _w = get_active_win();
                  if (_w && _w->pid == ctx->pid) browser_fetch_images(_w, t); }
                ctx_free(ctx); is_dirty(TRUE);
                return ERR_OK;
            }
        }
    }
    pbuf_free(p);
    return ERR_OK;
}

static void browser_err_callback(void* arg, err_t err) {
    if (!arg) return;
    BrowserFetchCtx* ctx = (BrowserFetchCtx*)arg;
    Process* proc = get_process(ctx->pid);
    (void)proc;
    BrowserMeta* bd = get_process(ctx->pid) ? (BrowserMeta*)get_process(ctx->pid)->data : NULL;
    int tab_index = find_tab_by_id(bd, ctx->tab_id);
    BrowserTab* t = (bd && tab_index >= 0)
                                ? &bd->tabs[tab_index] : NULL;
    kprint("Browser[tab%d]: Connection error %d\n", tab_index, err);
    if (t && t->is_loading) {
        t->is_loading = FALSE; t->scroll_offset = 0;
        html_free_list(&t->render_list);
        snprintf(t->status, sizeof(t->status), "Error: connection failed (%d)", err);
        if (t->html_buffer && t->html_buffer_len > 0) {
            char* body = strstr(t->html_buffer, "\r\n\r\n");
            if (body) { *body = '\0'; body += 4; if (strcasestr(t->html_buffer, "Transfer-Encoding: chunked")) decode_chunked(body); html_parse(body, &t->render_list); }
            else html_parse(t->html_buffer, &t->render_list);
        }
        is_dirty(TRUE);
    }
    ctx_free(ctx);
}

static err_t browser_sent_callback(void* arg, struct altcp_pcb* tpcb, u16_t len) {
    (void)arg; (void)tpcb; (void)len; return ERR_OK;
}

static err_t browser_connected_callback(void* arg, struct altcp_pcb* tpcb, err_t err) {
    BrowserFetchCtx* ctx = (BrowserFetchCtx*)arg;
    BrowserMeta* bd = get_process(ctx->pid) ? (BrowserMeta*)get_process(ctx->pid)->data : NULL;
    int tab_index = find_tab_by_id(bd, ctx->tab_id);
    if (err != ERR_OK) { kprint("Browser[tab%d]: Connect error %d\n", tab_index, err); ctx_free(ctx); return err; }

    char request[4096];
    if (ctx->post_body)
        snprintf(request, sizeof(request),
            "POST %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: kOSeki-Browser/1.0\r\n"
            "Accept: text/html, */*\r\nAccept-Encoding: identity\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\n"
            "Connection: close\r\n\r\n%s",
            ctx->path, ctx->host, (int)strlen(ctx->post_body), ctx->post_body);
    else
        snprintf(request, sizeof(request),
            "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: kOSeki-Browser/1.0\r\n"
            "Accept: text/html, */*\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n",
            ctx->path, ctx->host);

    altcp_recv(tpcb, browser_recv_callback);
    altcp_sent(tpcb, browser_sent_callback);
    err_t w = altcp_write(tpcb, request, strlen(request), TCP_WRITE_FLAG_COPY);
    if (w != ERR_OK) { kprint("Browser[tab%d]: write failed %d\n", tab_index, w); altcp_close(tpcb); ctx_free(ctx); return ERR_OK; }
    altcp_output(tpcb);
    return ERR_OK;
}

static void browser_dns_callback(const char* name, const ip_addr_t* ipaddr, void* callback_arg)
{
    (void)name;
    BrowserFetchCtx* ctx = (BrowserFetchCtx*)callback_arg;
    if (ipaddr) {
        struct altcp_pcb* pcb = NULL;
        if (ctx->is_https) {
            struct altcp_tls_config* cfg = altcp_tls_create_config_client(NULL, 0);
            if (!cfg) { ctx->tls_config = NULL; ctx_free(ctx); return; }
            ctx->tls_config = cfg;
            pcb = altcp_tls_new(cfg, IPADDR_TYPE_V4);
            if (!pcb) { ctx_free(ctx); return; }
            mbedtls_ssl_context* ssl = (mbedtls_ssl_context*)altcp_tls_context(pcb);
            if (ssl) mbedtls_ssl_set_hostname(ssl, ctx->host);
        } else {
            ctx->tls_config = NULL;
            pcb = altcp_tcp_new_ip_type(IPADDR_TYPE_V4);
            if (!pcb) { ctx_free(ctx); return; }
        }
        altcp_arg(pcb, ctx);
        altcp_err(pcb, browser_err_callback);
        err_t err = altcp_connect(pcb, ipaddr, ctx->port, browser_connected_callback);
        if (err != ERR_OK && err != ERR_INPROGRESS) { browser_err_callback(ctx, err); altcp_close(pcb); }
    } else {
        Process* proc = get_process(ctx->pid);
        BrowserMeta* bd = proc ? (BrowserMeta*)proc->data : NULL;
        int tab_index = find_tab_by_id(bd, ctx->tab_id);
        BrowserTab* t = (bd && tab_index >= 0) ? &bd->tabs[tab_index] : NULL;
        if (t) {
            if (t->html_buffer) { strcpy(t->html_buffer, "Error: DNS Resolution Failed\n"); t->html_buffer_len = strlen(t->html_buffer); }
            t->is_loading = FALSE;
            snprintf(t->status, sizeof(t->status), "Error: DNS failed for %s", ctx->host);
        }
        ctx_free(ctx); is_dirty(TRUE);
    }
}

static void img_ctx_close(struct altcp_pcb* pcb, ImageFetchCtx* ctx)
{
    if (pcb) altcp_close(pcb);
    imgctx_free(ctx);
}

static void browser_img_start_fetch(ImageFetchCtx* ctx);

static err_t img_recv_callback(void* arg, struct altcp_pcb* pcb,
                               struct pbuf* p, err_t err)
{
    ImageFetchCtx* ctx = (ImageFetchCtx*)arg;
    if (!p || err != ERR_OK) {
        goto decode;
    }
    altcp_recved(pcb, p->tot_len);
    int needed = ctx->buf_len + (int)p->tot_len;
    if (needed >= ctx->buf_size) {
        int ns = needed + 131072;
        char* nb = (char*)kmalloc(ns);
        if (!nb) { 
            kprint("Browser[img]: Failed to grow buffer to %d bytes for %s\n", ns, ctx->host);
            pbuf_free(p); 
            img_ctx_close(pcb, ctx); 
            return ERR_OK; 
        }
        if (ctx->buf) { memcpy(nb, ctx->buf, ctx->buf_len); kfree(ctx->buf); }
        ctx->buf = nb; ctx->buf_size = ns;
    }
    pbuf_copy_partial(p, ctx->buf + ctx->buf_len, p->tot_len, 0);
    ctx->buf_len += p->tot_len;
    pbuf_free(p);
    return ERR_OK;

decode:;
    if (ctx->buf) ctx->buf[ctx->buf_len] = '\0';
    // strip headers
    Process* proc = get_process(ctx->pid);
    BrowserMeta* bd = proc ? (BrowserMeta*)proc->data : NULL;
    int tab_index = find_tab_by_id(bd, ctx->tab_id);
    BrowserTab* t = (bd && tab_index >= 0)
                               ? &bd->tabs[tab_index] : NULL;
    if (!t || ctx->elem_index < 0 || ctx->elem_index >= t->render_list.count)
        { img_ctx_close(pcb, ctx); return ERR_OK; }

    RenderElement* el = &t->render_list.elements[ctx->elem_index];

    char* body = ctx->buf ? strstr(ctx->buf, "\r\n\r\n") : NULL;
    if (!body) { img_ctx_close(pcb, ctx); return ERR_OK; }
    *body = '\0'; body += 4;

    int status = 0;
    char* sp = strchr(ctx->buf, ' ');
    if (sp) {
        while (*sp == ' ') sp++;
        while (*sp >= '0' && *sp <= '9') status = status * 10 + (*sp++ - '0');
    }

    kprint("Browser[img]: Host %s, Status %d\n", ctx->host, status);

    if (status >= 300 && status < 400 && ctx->redirect_count < 5) {
        char* loc = strcasestr(ctx->buf, "Location: ");
        if (loc) {
            loc += 10;
            char* end = strstr(loc, "\r\n"); if (end) *end = '\0';
            while (*loc == ' ') loc++;
            
            char base_url[1024];
            snprintf(base_url, sizeof(base_url), "%s://%s:%d%s", 
                     ctx->is_https ? "https" : "http", ctx->host, ctx->port, ctx->path);
            
            char new_url[1024];
            resolve_url(base_url, loc, new_url, sizeof(new_url));
            
            kprint("Browser[img]: Redirect -> %s\n", new_url);
            
            ParsedURL pu; parse_url(new_url, &pu);
            kfree(ctx->host); ctx->host = strdup(pu.host);
            kfree(ctx->path); ctx->path = strdup(pu.path);
            ctx->port = pu.port;
            ctx->is_https = (pu.port == 443 || strncmp(new_url, "https://", 8) == 0);
            ctx->buf_len = 0;
            ctx->redirect_count++;
            
            if (pcb) altcp_close(pcb);
            if (ctx->tls_config) { altcp_tls_free_config(ctx->tls_config); ctx->tls_config = NULL; }
            
            browser_img_start_fetch(ctx);
            return ERR_OK;
        }
    }

    if (status != 200) {
        kprint("Browser[img]: Non-200 status for %s\n", ctx->path);
        img_ctx_close(pcb, ctx);
        return ERR_OK;
    }

    int body_len = ctx->buf_len - (int)(body - ctx->buf);
    if (body_len <= 0) { img_ctx_close(pcb, ctx); return ERR_OK; }

    if (strcasestr(ctx->buf, "Transfer-Encoding: chunked")) {
        kprint("Browser[img]: Decoding chunked data...\n");
        body_len = decode_chunked_bin((unsigned char*)body, body_len);
        kprint("Browser[img]: Decoded len %d\n", body_len);
    }

    int w = 0, h = 0, ch = 0;

    if (body_len > 10 && body[0] == 'G' && body[1] == 'I' && body[2] == 'F') {
        int frames = 0;
        int* delays = NULL;
        unsigned char* decoded = stbi_load_gif_from_memory(
            (const unsigned char*)body, body_len, &delays, &w, &h, &frames, &ch, 4);
        
        if (decoded && frames > 1) {
            kprint("Browser[img]: Decoded GIF %dx%d, %d frames\n", w, h, frames);
            el->img_width = w; el->img_height = h;
            el->frame_count = frames;
            el->frames = (uint32**)kmalloc(sizeof(uint32*) * frames);
            el->delays = (int*)kmalloc(sizeof(int) * frames);
            for (int f = 0; f < frames; f++) {
                int d = delays[f];
                if (d < 3) d = 1;
                d = d / 2;
                el->delays[f] = d;
                uint32* fpix = (uint32*)kmalloc(w * h * sizeof(uint32));
                if (!fpix) {
                    kprint("Browser[img]: Failed to allocate frame %d\n", f);
                    el->frame_count = f; // stop
                    break;
                }
                unsigned char* fsrc = decoded + (f * w * h * 4);
                for (int i = 0; i < w * h; i++) {
                    int row = i / w;
                    int col = i % w;
                    int src_row = h - 1 - row;
                    int si = (src_row * w + col) * 4;
                    uint8 r = fsrc[si+0], g = fsrc[si+1], b = fsrc[si+2], a = fsrc[si+3];
                    fpix[i] = ((uint32)a << 24) | ((uint32)r << 16) | ((uint32)g << 8) | b;
                }
                el->frames[f] = fpix;
            }
            el->pixels = el->frames[0];
            el->current_frame = 0;
            el->last_frame_ticks = timer_ticks;
            stbi_image_free(decoded);
            kfree(delays);
            goto done_decode;
        }
        if (decoded) stbi_image_free(decoded);
        if (delays) kfree(delays);
    }

    unsigned char* decoded = stbi_load_from_memory(
        (const unsigned char*)body, body_len, &w, &h, &ch, 4);
    if (decoded && w > 0 && h > 0) {
        kprint("Browser[img]: Decoded %dx%d (%d channels)\n", w, h, ch);
        if (el->img_width <= 0) el->img_width = w;
        if (el->img_height <= 0) el->img_height = h;
        int dw = el->img_width, dh = el->img_height;

        if (dw > 500) { dh = (dh * 500) / dw; dw = 500; }
        if (dh > 375) { dw = (dw * 375) / dh; dh = 375; }

        uint32* pixels = (uint32*)kmalloc(dw * dh * sizeof(uint32));
        if (pixels) {
            if (dw == w && dh == h) {
                for (int i = 0; i < w * h; i++) {
                    int row = i / w;
                    int col = i % w;
                    int src_row = h - 1 - row;
                    int src_idx = (src_row * w + col) * 4;
                    uint8 r = decoded[src_idx+0], g = decoded[src_idx+1],
                        b = decoded[src_idx+2], a = decoded[src_idx+3];
                    pixels[i] = ((uint32)a << 24) | ((uint32)r << 16)
                            | ((uint32)g << 8) | (uint32)b;
                }
            } else {
                for (int dy2 = 0; dy2 < dh; dy2++) {
                    int sy = (dy2 * h) / dh;
                    int src_row = h - 1 - sy;
                    for (int dx2 = 0; dx2 < dw; dx2++) {
                        int sx = (dx2 * w) / dw;
                        int si = (src_row * w + sx) * 4;
                        uint8 r = decoded[si+0], g = decoded[si+1],
                              b = decoded[si+2], a = decoded[si+3];
                        pixels[dy2*dw+dx2] = ((uint32)a << 24) | ((uint32)r << 16)
                                           | ((uint32)g << 8) | (uint32)b;
                    }
                }
            }
            if (el->pixels) kfree(el->pixels);
            el->pixels = pixels;
            el->img_width = dw;
            el->img_height = dh;
        } else {
            kprint("Browser[img]: Failed to allocate pixel buffer\n");
        }
        stbi_image_free(decoded);

        done_decode:;
        Window* win = get_active_win();
        if (win && win->pid == ctx->pid) {
            win->frame_cache_dirty = TRUE;
            is_dirty(TRUE);
        }
        if (t) {
            int lm = 8, rm = 500 - 24;
            t->content_height = eval_visible(&t->render_list, lm, rm);
        }
    } else {
        kprint("Browser[img]: stbi_load_from_memory failed: %s\n", stbi_failure_reason());
    }
    img_ctx_close(pcb, ctx);
    return ERR_OK;
}

static void browser_img_start_fetch(ImageFetchCtx* ctx);

static err_t img_connected_callback(void* arg, struct altcp_pcb* pcb, err_t err)
{
    ImageFetchCtx* ctx = (ImageFetchCtx*)arg;
    if (err != ERR_OK) { img_ctx_close(pcb, ctx); return err; }

    char req[4096];
    snprintf(req, sizeof(req),
             "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nUser-Agent: kOSeki-Browser/1.0\r\n\r\n",
             ctx->path, ctx->host);
    altcp_write(pcb, req, strlen(req), TCP_WRITE_FLAG_COPY);
    altcp_output(pcb);
    return ERR_OK;
}

static void img_dns_callback(const char* name, const ip_addr_t* ipaddr, void* arg)
{
    (void)name;
    ImageFetchCtx* ctx = (ImageFetchCtx*)arg;
    if (!ipaddr) { imgctx_free(ctx); return; }

    struct altcp_pcb* pcb;
    if (ctx->is_https) {
        struct altcp_tls_config* cfg = altcp_tls_create_config_client(NULL, 0);
        if (!cfg) { imgctx_free(ctx); return; }
        ctx->tls_config = cfg;
        pcb = altcp_tls_new(cfg, IPADDR_TYPE_V4);
        if (!pcb) { imgctx_free(ctx); return; }
        mbedtls_ssl_context* ssl = (mbedtls_ssl_context*)altcp_tls_context(pcb);
        if (ssl) mbedtls_ssl_set_hostname(ssl, ctx->host);
    } else {
        pcb = altcp_tcp_new_ip_type(IPADDR_TYPE_ANY);
        if (!pcb) { imgctx_free(ctx); return; }
    }

    altcp_recv(pcb, img_recv_callback);
    altcp_arg(pcb, ctx);

    ip_addr_t addr = *ipaddr;
    err_t e = altcp_connect(pcb, &addr, ctx->port, img_connected_callback);
    if (e != ERR_OK) { altcp_close(pcb); imgctx_free(ctx); }
}

static void browser_img_start_fetch(ImageFetchCtx* ctx)
{
    ip_addr_t addr;
    err_t e = dns_gethostbyname(ctx->host, &addr, img_dns_callback, ctx);
    if (e == ERR_OK) {
        img_dns_callback(ctx->host, &addr, ctx);
    } else if (e != ERR_INPROGRESS) {
        imgctx_free(ctx);
    }
}

//async fetch if no px but src
static void browser_fetch_images(Window* win, BrowserTab* t) {
    (void)win;
    
    for (int i = 0; i < t->render_list.count; i++) {
        RenderElement* el = &t->render_list.elements[i];
        if (el->type != ELEM_IMAGE) continue;
        if (el->src_url[0] == '\0') continue;
        if (el->pixels) continue;

        const char* src_ptr = el->src_url;
        if (src_ptr[0] == '/') src_ptr++;

        if (strncmp(src_ptr, "SYSTEM/", 7) == 0) {
            Bitmap* bmp = load_bmp((char*)src_ptr);
            if (!bmp) continue;

            int dw = el->img_width > 0 ? el->img_width : bmp->width;
            int dh = el->img_height > 0 ? el->img_height : bmp->height;
            if (dw > 500) { dh = (dh * 500) / dw; dw = 500; }
            if (dh > 375) { dw = (dw * 375) / dh; dh = 375; }

            uint32* pixels = (uint32*)kmalloc(dw * dh * sizeof(uint32));
            if (pixels) {
                for (int dy2 = 0; dy2 < dh; dy2++) {
                    int sy = (dy2 * bmp->height) / dh;
                    for (int dx2 = 0; dx2 < dw; dx2++) {
                        int sx = (dx2 * bmp->width) / dw;

                        //expecting argb
                        uint32 c = bmp->data[sy * bmp->width + sx];
                        uint8 b = (c >> 0) & 0xFF,
                            g = (c >> 8) & 0xFF,
                            r = (c >> 16) & 0xFF;
                        pixels[dy2 * dw + dx2] =
                            (0xFFu << 24) | ((uint32)r << 16) | ((uint32)g << 8) | b;
                    }
                }
                if (el->pixels) kfree(el->pixels);
                el->pixels = pixels;
                el->img_width = dw;
                el->img_height = dh;
            }
            free_bmp(bmp);
            is_dirty(TRUE);
            continue;
        }
        
        char abs_url[1024];
        resolve_url(t->url, el->src_url, abs_url, sizeof(abs_url));

        ParsedURL pu; parse_url(abs_url, &pu);

        ImageFetchCtx* ctx = (ImageFetchCtx*)kmalloc(sizeof(ImageFetchCtx));
        if (!ctx) continue;
        memset(ctx, 0, sizeof(*ctx));
        ctx->pid = win->pid;
        ctx->tab_id = t->id;
        ctx->elem_index = i;
        ctx->port = pu.port;
        ctx->host = strdup(pu.host);
        ctx->path = strdup(pu.path);
        ctx->buf_size = 1048576;
        ctx->buf = (char*)kmalloc(ctx->buf_size);
        ctx->buf_len = 0;
        ctx->is_https = (pu.port == 443 || strncmp(abs_url, "https://", 8) == 0);

        browser_img_start_fetch(ctx);
    }
}

static void browser_fetch(Window* win, const char* url, const char* post_body) {
    Process* p = get_process(win->pid);
    BrowserMeta* bd = (BrowserMeta*)p->data;
    BrowserTab* t = active_tab(bd);

    if (url[0] == '\0' || strcmp(url, "about:home") == 0) {
        tab_load_homepage(t);
        browser_fetch_images(win, t);
        is_dirty(TRUE);
        return;
    }

    if (t->redirect_ticks == 0) history_push(&t->history, url);
    strncpy(t->url, url, 1023); t->url[1023] = '\0';
    t->is_loading = TRUE; t->html_buffer_len = 0;
    if (t->html_buffer) t->html_buffer[0] = '\0';
    html_free_list(&t->render_list);
    snprintf(t->status, sizeof(t->status), "Connecting to %s...", t->url);
    derive_title(t, t->url);

    ParsedURL parsed; parse_url(t->url, &parsed);
    BrowserFetchCtx* ctx = (BrowserFetchCtx*)kmalloc(sizeof(BrowserFetchCtx));
    memset(ctx, 0, sizeof(BrowserFetchCtx));
    ctx->pid = win->pid;
    ctx->tab_id = t->id;
    ctx->host = (char*)kmalloc(strlen(parsed.host) + 1); strcpy(ctx->host, parsed.host);
    ctx->port = parsed.port;
    ctx->path = (char*)kmalloc(strlen(parsed.path) + 1); strcpy(ctx->path, parsed.path);
    ctx->is_https = (parsed.port == 443 || strncmp(t->url, "https://", 8) == 0);
    ctx->post_body = NULL;
    if (post_body && post_body[0]) { ctx->post_body = (char*)kmalloc(strlen(post_body) + 1); strcpy(ctx->post_body, post_body); }

    kprint("Browser[tab%d]: %s %s (Host: %s Port: %d Path: %s)\n",
                  bd->active_tab, post_body ? "POST" : "GET", t->url, parsed.host, parsed.port, parsed.path);

    ip_addr_t addr;
    err_t err = dns_gethostbyname(parsed.host, &addr, browser_dns_callback, ctx);
    if (err == ERR_OK) browser_dns_callback(parsed.host, &addr, ctx);
    else if (err != ERR_INPROGRESS) {
        if (t->html_buffer) { strcpy(t->html_buffer, "Error: Network unreachable\n"); t->html_buffer_len = strlen(t->html_buffer); }
        t->is_loading = FALSE;
        snprintf(t->status, sizeof(t->status), "Error: Network unreachable");
        ctx_free(ctx);
    }
    is_dirty(TRUE);
}

void search(Window* win) {
    Process* p = get_process(win->pid);
    BrowserMeta* bd = (BrowserMeta*)p->data;
    BrowserTab* t = active_tab(bd);
    bd->url_bar_focused = FALSE;
    t->render_list.focused_element = -1;
    browser_fetch(win, t->url, NULL);
}

static void resp_form(Window* win, int form_id) {
    Process* p = get_process(win->pid);
    BrowserMeta* bd = (BrowserMeta*)p->data;
    BrowserTab* t = active_tab(bd);
    if (form_id < 0 || form_id >= t->render_list.form_count) return;
    FormDescriptor* fd = &t->render_list.forms[form_id];
    char encoded[1024];
    html_form_encode(&t->render_list, form_id, encoded, sizeof(encoded));
    char action_url[512];
    if (fd->action[0] == '\0') strncpy(action_url, t->url, sizeof(action_url) - 1);
    else resolve_url(t->url, fd->action, action_url, sizeof(action_url));
    kprint("Browser: form submit method=%s action=%s body=%s\n", fd->method, action_url, encoded);
    if (strcmp(fd->method, "POST") == 0) {
        browser_fetch(win, action_url, encoded);
    } else {
        char get_url[768];
        if (encoded[0]) snprintf(get_url, sizeof(get_url), "%s?%s", action_url, encoded);
        else strncpy(get_url, action_url, sizeof(get_url) - 1);
        get_url[sizeof(get_url) - 1] = '\0';
        browser_fetch(win, get_url, NULL);
    }
}

static int eval_visible(RenderList* list, int left_margin, int right_margin) {
    const int base_lh = 18, base_cw = 11, base_sw = 4;
    int draw_x = left_margin, draw_y = 0;
    for (int i = 0; i < list->count; i++) {
        RenderElement* el = &list->elements[i];
        int scale = (el->scale > 0) ? el->scale : 1;
        int lh = base_lh * scale, cw = (base_cw - 1) * scale + (el->bold ? scale : 0), sw = base_sw * scale;
        if (el->type == ELEM_NEWLINE || el->newline) { draw_x = left_margin; draw_y += lh; continue; }
        if (el->type == ELEM_INPUT_TEXT || el->type == ELEM_INPUT_SUBMIT) { draw_x += el->input_width + 11; if (draw_x > right_margin) { draw_x = left_margin; draw_y += INPUT_HEIGHT + 6; } continue; }
        if (el->type == ELEM_IMAGE) {
            int iw = el->img_width > 0 ? el->img_width : 64;
            int ih = el->img_height > 0 ? el->img_height : 64;
            if (draw_x + iw > right_margin && draw_x > left_margin) { draw_x = left_margin; draw_y += lh; }
            draw_x += iw + 4;
            if (ih > lh) draw_y += ih - lh; //tall images
            continue;
        }
        if (el->type == ELEM_INPUT_HIDDEN || !el->text) continue;
        const char* str = el->text;
        while (*str) {
            if (*str == '\n') { draw_x = left_margin; draw_y += lh; str++; continue; }
            int w = 0; while (*str && *str != ' ' && *str != '\n' && w < 127) { w++; str++; }
            if (*str == ' ') str++;
            if (w == 0) continue;
            int wpx = w * cw;
            if (draw_x + wpx > right_margin && draw_x > left_margin) { draw_x = left_margin; draw_y += lh; }
            draw_x += wpx + sw;
        }
    }
    return draw_y + base_lh;
}

static void char_clip(int ch, int x, int y, int right_margin, int view_y, int view_h, uint32 color, BOOL bold, int scale, const FontInfo* fi) {
    int sw = vbe_get_width(), sh = vbe_get_height();
    if (ch < 32 || ch > 126) ch = 32;
    for (int row = 0; row < fi->height; row++) {
        int py = y + row * scale;
        if (py + scale <= view_y || py >= view_y + view_h || py + scale <= 0 || py >= sh) { if (py >= view_y + view_h) break; continue; }
        unsigned int rd = fi->get_char(ch, row);
        for (int col = 0; col < fi->width; col++) {
            if (!((rd >> (fi->width - 1 - col)) & 1)) continue;
            int px = x + col * scale;
            if (px >= right_margin || px >= sw) break;
            if (px + scale <= 0 || px < 0) continue;
            rect(px, py, scale, scale, color);
            if (bold) { int pbx = px + 1; if (pbx < right_margin && pbx < sw) rect(pbx, py, scale, scale, color); }
        }
    }
}

static void text_clip_b(const char* str, int x, int y, int right_margin, int view_y, int view_h, uint32 color, BOOL bold)
{
    const FontInfo* fi = get_font_info(FONT_DEFAULT);
    if (!fi) return;
    int sw = vbe_get_width();
    int cw = 11;

    while (*str) {
        if (x >= right_margin || x >= sw) break;
        if (x + cw > 0) {
            char_clip((int)*str, x, y, right_margin, view_y, view_h, color, bold, 1, fi);
        }
        x += cw;
        str++;
    }
}

static void draw_wrapped_text(const char* str, int* draw_x, int* draw_y, int left_margin, int right_margin, int view_y, int view_h, uint32 color, BOOL bold, int scale) {
    const int base_lh = 18, base_cw = 11, base_sw = 4;
    int lh = base_lh * scale, cw = (base_cw - 1) * scale + (bold ? scale : 0), sw = base_sw * scale;
    const FontInfo* fi = get_font_info(FONT_DEFAULT);
    if (!fi) return;
    while (*str) {
        if (*str == '\n') { *draw_x = left_margin; *draw_y += lh; str++; continue; }
        const char* ws = str; int w = 0;
        while (*str && *str != ' ' && *str != '\n' && w < 127) { w++; str++; }
        if (*str == ' ') str++;
        if (w == 0) continue;
        int wpx = w * cw;
        if (*draw_x + wpx > right_margin && *draw_x > left_margin) { *draw_x = left_margin; *draw_y += lh; }
        if (*draw_y >= view_y + view_h) return;
        if (*draw_y + lh <= view_y) { *draw_x += wpx + sw; continue; }
        int cx = *draw_x;
        for (int i = 0; i < w; i++) {
            if (cx >= right_margin) break;
            char_clip((int)ws[i], cx, *draw_y, right_margin, view_y, view_h, color, bold, scale, fi);
            cx += cw;
        }
        *draw_x += wpx + sw;
    }
}

static void draw_input_text(RenderElement* el, int x, int y, int right_margin, int view_y, int view_h, BOOL focused) {
    if (y + INPUT_HEIGHT <= view_y || y >= view_y + view_h) return;
    int w = el->input_width;

    uint32 bc = focused ? COLOR_INPUT_FOCUSED : COLOR_INPUT_BORDER;
    rect_clipped(x, y, w, INPUT_HEIGHT, right_margin, view_y, view_h, COLOR_INPUT_BG);
    draw_rect_outline_clipped(x, y, w, INPUT_HEIGHT, right_margin, view_y, view_h, bc);
    if (focused) draw_rect_outline_clipped(x - 1, y - 1, w + 2, INPUT_HEIGHT + 2, right_margin, view_y, view_h, bc);

    const char* display = el->field_value;
    BOOL use_ph = (display[0] == '\0' && el->placeholder[0] != '\0');
    if (use_ph) display = el->placeholder;

    char masked[256];
    if (el->is_password && !use_ph) {
        int len = strlen(el->field_value); if (len > 255) len = 255;
        for (int i = 0; i < len; i++) masked[i] = 0xF8;
        masked[len] = '\0'; display = masked;
    }
    text_clip_b(display, x + INPUT_PADDING_X, y + INPUT_PADDING_Y,
              x + w - 4 < right_margin ? x + w - 4 : right_margin, view_y, view_h,
              use_ph ? RGB(180, 150, 165) : COLOR_TEXT_PRIMARY, FALSE);
    if (focused) {
        int cx = x + INPUT_PADDING_X + (strlen(el->field_value) * 11);
        if (cx < x + w - 4 && cx < right_margin) 
            rect_clipped(cx, y + INPUT_PADDING_Y + 1, 2, INPUT_HEIGHT - 8, right_margin, view_y, view_h, COLOR_ACCENT_PRIMARY);
    }
}

static void draw_submit_button(RenderElement* el, int x, int y, int right_margin, int view_y, int view_h, BOOL hovered) {
    if (y + SUBMIT_HEIGHT <= view_y || y >= view_y + view_h) return;
    const char* label = el->field_value[0] ? el->field_value : "enter";
    int btn_w = strlen(label) * 11 + 20; if (btn_w < 60) btn_w = 60;
    rect_clipped(x, y, btn_w, SUBMIT_HEIGHT, right_margin, view_y, view_h, hovered ? COLOR_SUBMIT_HOV : COLOR_SUBMIT_BG);
    draw_rect_outline_clipped(x, y, btn_w, SUBMIT_HEIGHT, right_margin, view_y, view_h, RGB(140, 50, 80));
    text_clip_b(label, x + 10, y + INPUT_PADDING_Y, right_margin, view_y, view_h, COLOR_SUBMIT_TEXT, TRUE);
    el->input_width = btn_w;
}

void browser_renderer(Window* win) {
    Process* p = get_process(win->pid);
    BrowserMeta* bd = (BrowserMeta*)p->data;
    BrowserTab* t = active_tab(bd);
    int sh = vbe_get_height();

    int cx = win->x + WIN_BORDER;
    int cy = win->y + TITLEBAR_H + WIN_BORDER;
    int cw = win->width - (2 * WIN_BORDER);
    int ch = win->height - TITLEBAR_H - (2 * WIN_BORDER);

    rect(cx, cy, cw, TAB_BAR_HEIGHT, COLOR_BG_LIGHT);
    rect(cx, cy + TAB_BAR_HEIGHT - 1, cw, 1, COLOR_BORDER);
    int tab_area_w = cw - 26;
    int tab_w = (bd->tab_count > 0) ? tab_area_w / bd->tab_count : tab_area_w;
    if (tab_w > TAB_MAX_WIDTH) tab_w = TAB_MAX_WIDTH;
    if (tab_w < TAB_MIN_WIDTH) tab_w = TAB_MIN_WIDTH;
    for (int i = 0; i < bd->tab_count; i++) {
        int tx = cx + i * tab_w, ty = cy;
        BOOL ia = (i == bd->active_tab), ih = (i == bd->mouse_hover_tab);
        uint32 tc = ia ? COLOR_TAB_ACTIVE : (ih ? COLOR_TAB_HOV : COLOR_TAB_INACTIVE);
        rect(tx, ty, tab_w, TAB_BAR_HEIGHT, tc);
        draw_rect_outline(tx, ty, tab_w, TAB_BAR_HEIGHT, COLOR_BORDER);
        
        int mc = (tab_w - 24) / 7; if (mc < 1) mc = 1; if (mc > 19) mc = 19;
        char st[21]; strncpy(st, bd->tabs[i].title, mc); st[mc] = '\0';
        if ((int)strlen(bd->tabs[i].title) > mc && mc > 2) st[mc - 1] = '.';
        text_clip_b(st, tx + 4, ty + 5, tx + tab_w - 20, 0, sh, ia ? COLOR_TEXT_PRIMARY : COLOR_TEXT_SECONDARY, ia);

        int close_x = tx + tab_w - 18;
        int close_y = ty + 6;
        BOOL hc = (i == bd->mouse_hover_close);
        if (hc) {
            rect(close_x, close_y - 2, 14, 16, RGB(240, 150, 150));
        }
        text("x", close_x + 4, close_y - 4, hc ? RGB(120, 10, 10) : (ia ? COLOR_TEXT_PRIMARY : COLOR_TEXT_SECONDARY), FONT_KALNIA, FALSE);
    }
    int plus_x = cx + bd->tab_count * tab_w;
    rect(plus_x, cy, 24, TAB_BAR_HEIGHT - 1, bd->mouse_hover_newtab ? COLOR_BTN_HOV : COLOR_BG_DARK);
    draw_rect_outline(plus_x, cy, 24, TAB_BAR_HEIGHT - 1, COLOR_BORDER);
    text("+", plus_x + 6, cy + 5, COLOR_TEXT_PRIMARY, FONT_DEFAULT, TRUE);

    int uby = cy + TAB_BAR_HEIGHT;
    rect(cx, uby, cw, URL_BAR_HEIGHT, COLOR_BG_LIGHT);
    rect(cx, uby + URL_BAR_HEIGHT - 1, cw, 1, COLOR_BORDER);
    BOOL can_back = history_can_go_back(&t->history);
    uint32 back_col = bd->mouse_hover_back ? COLOR_BTN_HOV : COLOR_BG_DARK;
    if (!can_back) back_col = COLOR_BACK_DISABLED;
    rect(cx + 4, uby + 5, BACK_BUTTON_WIDTH, BUTTON_HEIGHT, back_col);
    draw_rect_outline(cx + 4, uby + 5, BACK_BUTTON_WIDTH, BUTTON_HEIGHT, COLOR_BORDER);
    text("<", cx + 11, uby + 8, can_back ? COLOR_TEXT_PRIMARY : COLOR_TEXT_SECONDARY, FONT_DEFAULT, TRUE);
    text("URL", cx + 38, uby + 8, COLOR_ACCENT_PRIMARY, FONT_DEFAULT, TRUE);
    
    int input_x = cx + 78, input_w = cw - 138;
    rect(input_x, uby + 5, input_w, 20, COLOR_BG_DARK);
    draw_rect_outline(input_x, uby + 5, input_w, 20, bd->url_bar_focused ? COLOR_INPUT_FOCUSED : COLOR_BORDER);
    
    int max_chars_visible = (input_w - 10) / 11;
    int url_len = strlen(t->url);

    // keep cursor visible
    if (bd->url_cursor_pos < bd->url_view_offset) {
        bd->url_view_offset = bd->url_cursor_pos;
    } else if (bd->url_cursor_pos >= bd->url_view_offset + max_chars_visible) {
        bd->url_view_offset = bd->url_cursor_pos - max_chars_visible + 1;
    }
    
    if (url_len < max_chars_visible) {
        bd->url_view_offset = 0;
    } else if (bd->url_view_offset + max_chars_visible > url_len) {
        bd->url_view_offset = url_len - max_chars_visible;
    }

    if (bd->url_view_offset < 0) bd->url_view_offset = 0;
    if (bd->url_view_offset > url_len) bd->url_view_offset = url_len;

    text_clip_b(t->url + bd->url_view_offset, input_x + 5, uby + 8, input_x + input_w - 5, 0, sh, COLOR_TEXT_PRIMARY, FALSE);
    
    if (bd->url_bar_focused) {
        int cursor_x = input_x + 5 + (int)((bd->url_cursor_pos - bd->url_view_offset) * 11);
        if (cursor_x < input_x + input_w - 5) {
            rect(cursor_x, uby + 7, 2, 16, COLOR_ACCENT_PRIMARY);
        }
    }
    
    int btn_x = cx + cw - 50;
    rect(btn_x, uby + 5, BUTTON_WIDTH, BUTTON_HEIGHT, bd->mouse_hover_go ? COLOR_BTN_HOV : COLOR_BG_DARK);
    draw_rect_outline(btn_x, uby + 5, BUTTON_WIDTH, BUTTON_HEIGHT, COLOR_BORDER);
    text("GO", btn_x + 12, uby + 8, COLOR_TEXT_PRIMARY, FONT_DEFAULT, TRUE);

    int view_y = uby + URL_BAR_HEIGHT;
    int view_h = ch - TAB_BAR_HEIGHT - URL_BAR_HEIGHT - STATUS_BAR_HEIGHT;
    rect(cx, view_y, cw, view_h, COLOR_PAGE_BG);

    if (t->is_loading && t->html_buffer_len == 0) {
        text("Loading...", cx + 10, view_y + 10, COLOR_TEXT_PRIMARY, FONT_DEFAULT, FALSE);
    } else {
        int left_m = cx + 8, right_m = cx + cw - 24;
        const int base_lh = 18, base_cw = 11, base_sw = 4;
        t->content_height = eval_visible(&t->render_list, left_m, right_m);
        t->view_height = view_h;
        int max_scroll = t->content_height - t->view_height;
        if (max_scroll < 0) max_scroll = 0;
        if (t->scroll_offset > max_scroll) t->scroll_offset = max_scroll;
        if (t->scroll_offset < 0) t->scroll_offset = 0;

        int draw_x = left_m, draw_y = view_y + 2 - t->scroll_offset;
        for (int i = 0; i < t->render_list.count; i++) {
            RenderElement* el = &t->render_list.elements[i];
            int scale = (el->scale > 0) ? el->scale : 1;
            int lh = base_lh * scale;
            if (el->type == ELEM_NEWLINE || el->newline) { draw_x = left_m; draw_y += lh; continue; }
            if (el->type == ELEM_INPUT_HIDDEN) continue;
            if (el->type == ELEM_INPUT_TEXT) {
                draw_input_text(el, draw_x, draw_y, right_m, view_y, view_h, t->render_list.focused_element == i);
                draw_x += el->input_width + 11; continue;
            }
            if (el->type == ELEM_INPUT_SUBMIT) {
                draw_submit_button(el, draw_x, draw_y, right_m, view_y, view_h, bd->hover_submit_idx == i);
                draw_x += el->input_width + 11; continue;
            }
            if (el->type == ELEM_IMAGE) {
                int iw = el->img_width > 0 ? el->img_width : 64;
                int ih = el->img_height > 0 ? el->img_height : 64;
                if (draw_x + iw > right_m && draw_x > left_m) { draw_x = left_m; draw_y += lh; }
                draw_image(el, draw_x, draw_y, right_m, view_y, view_h);
                draw_x += iw + 4;
                if (ih > lh) draw_y += ih - lh;
                continue;
            }
            if (draw_y >= view_y + view_h) break;
            if (el->text) draw_wrapped_text(el->text, &draw_x, &draw_y, left_m, right_m, view_y, view_h, el->color, el->bold, scale);
        }

        if (t->content_height > t->view_height) {
            int sb_rx = cw - 14 - WIN_BORDER, sb_ry = TAB_BAR_HEIGHT + URL_BAR_HEIGHT + WIN_BORDER;
            bd->bar_x = sb_rx; bd->bar_y = sb_ry; bd->bar_width = 18; bd->bar_height = view_h;
            int thumb_h = (bd->bar_height * t->view_height) / t->content_height;
            if (thumb_h < 20) thumb_h = 20;
            if (thumb_h > bd->bar_height) thumb_h = bd->bar_height;
            int thumb_y = bd->bar_y + (max_scroll > 0 ? (t->scroll_offset * (bd->bar_height - thumb_h)) / max_scroll : 0);
            bd->thumb_x = sb_rx - 2; bd->thumb_y = thumb_y; bd->thumb_width = bd->bar_width + 4; bd->thumb_height = thumb_h;
            int abs_bx = cx + cw - 18;
            rect(abs_bx, view_y, bd->bar_width, bd->bar_height, COLOR_SCROLLBAR);
            rect(abs_bx + 2, view_y + (thumb_y - sb_ry) + 2, bd->bar_width - 4, thumb_h - 4,
                 bd->sb_is_hovered ? COLOR_THUMB_HOV : COLOR_THUMB);
        }
    }

    int status_y = view_y + view_h;
    rect(cx, status_y, cw, STATUS_BAR_HEIGHT, COLOR_STATUS_BG);
    rect(cx, status_y, cw, 1, COLOR_BORDER);
    if (bd->link_hover_idx != -1) {
        RenderElement* el = &t->render_list.elements[bd->link_hover_idx];
        if (el->link_url) {
            char sl[256]; snprintf(sl, sizeof(sl), "Link: %s", el->link_url);
            text_clip_b(sl, cx + 6, status_y + 3, cx + cw - 6, 0, sh, COLOR_STATUS_TEXT, FALSE);
            goto status_done;
        }
    }
    text_clip_b(t->status, cx + 6, status_y + 3, cx + cw - 6, 0, sh, COLOR_STATUS_TEXT, FALSE);
    status_done:;
}

void browser_on_mouse_move(Window* win, int x, int y)
{
    Process* p = get_process(win->pid);
    BrowserMeta* bd = (BrowserMeta*)p->data;
    BrowserTab* t = active_tab(bd);
    int cw = win->width - (2 * WIN_BORDER);
    int url_bar_rel_y = TAB_BAR_HEIGHT + WIN_BORDER;

    int old_go = bd->mouse_hover_go, btn_x = cw - 50;
    bd->mouse_hover_go = (x >= btn_x && x < btn_x + BUTTON_WIDTH && y >= url_bar_rel_y + 5 && y < url_bar_rel_y + 5 + BUTTON_HEIGHT);
    int old_back = bd->mouse_hover_back;
    bd->mouse_hover_back = (x >= 4 && x < 4 + BACK_BUTTON_WIDTH && y >= url_bar_rel_y + 5 && y < url_bar_rel_y + 5 + BUTTON_HEIGHT);

    int tab_w = (bd->tab_count > 0) ? (cw - 26) / bd->tab_count : (cw - 26);
    if (tab_w > TAB_MAX_WIDTH) tab_w = TAB_MAX_WIDTH;
    if (tab_w < TAB_MIN_WIDTH) tab_w = TAB_MIN_WIDTH;
    int old_tab = bd->mouse_hover_tab;
    bd->mouse_hover_tab = -1;
    int old_close = bd->mouse_hover_close;
    bd->mouse_hover_close = -1;
    if (y >= 0 && y < TAB_BAR_HEIGHT) {
        for (int i = 0; i < bd->tab_count; i++) {
            if (x >= i * tab_w && x < (i + 1) * tab_w) {
                bd->mouse_hover_tab = i;
                int close_x_min = (i + 1) * tab_w - 18;
                int close_x_max = (i + 1) * tab_w - 4;
                int close_y_min = 6;
                int close_y_max = 18;
                if (x >= close_x_min && x < close_x_max && y >= close_y_min && y < close_y_max) {
                    bd->mouse_hover_close = i;
                }
                break;
            }
        }
    }

    int old_newtab = bd->mouse_hover_newtab, plus_x_rel = bd->tab_count * tab_w;
    bd->mouse_hover_newtab = (y >= 0 && y < TAB_BAR_HEIGHT && x >= plus_x_rel && x < plus_x_rel + 24);
    BOOL was_hov = bd->sb_is_hovered;
    bd->sb_is_hovered = (x >= bd->thumb_x && x < bd->thumb_x + bd->thumb_width && y >= bd->thumb_y && y < bd->thumb_y + bd->thumb_height);

    int old_link = bd->link_hover_idx; bd->link_hover_idx = -1;
    int old_sub = bd->hover_submit_idx; bd->hover_submit_idx = -1;

    int page_rel_y = TAB_BAR_HEIGHT + URL_BAR_HEIGHT + WIN_BORDER;
    if (y >= page_rel_y && y < win->height - STATUS_BAR_HEIGHT - TITLEBAR_H) {
        const int base_lh = 18, base_cw = 11, base_sw = 4;
        int left_m = 8, right_m = cw - 24;
        int draw_x = left_m, draw_y = page_rel_y + 2 - t->scroll_offset;
        for (int i = 0; i < t->render_list.count; i++) {
            RenderElement* el = &t->render_list.elements[i];
            int scale = (el->scale > 0) ? el->scale : 1;
            int lh = base_lh * scale, cw2 = (base_cw - 1) * scale + (el->bold ? scale : 0), sw = base_sw * scale;
            if (el->type == ELEM_NEWLINE || el->newline) { draw_x = left_m; draw_y += lh; continue; }
            if (el->type == ELEM_INPUT_HIDDEN) continue;
            if (el->type == ELEM_INPUT_TEXT || el->type == ELEM_INPUT_SUBMIT) {
                if (x >= draw_x && x < draw_x + el->input_width && y >= draw_y && y < draw_y + INPUT_HEIGHT)
                    if (el->type == ELEM_INPUT_SUBMIT) bd->hover_submit_idx = i;
                draw_x += el->input_width + 8; continue;
            }
            if (el->type == ELEM_IMAGE) {
                int iw = el->img_width > 0 ? el->img_width : 64;
                int ih = el->img_height > 0 ? el->img_height : 64;
                if (draw_x + iw > right_m && draw_x > left_m) { draw_x = left_m; draw_y += lh; }
                draw_x += iw + 4;
                if (ih > lh) draw_y += ih - lh;
                continue;
            }
            if (!el->text) continue;
            const char* str = el->text;
            while (*str) {
                if (*str == '\n') { draw_x = left_m; draw_y += lh; str++; continue; }
                int w = 0; while (*str && *str != ' ' && *str != '\n' && w < 127) { w++; str++; }
                if (*str == ' ') str++;
                if (w == 0) continue;
                int wpx = w * cw2;
                if (draw_x + wpx > right_m && draw_x > left_m) { draw_x = left_m; draw_y += lh; }
                if (el->link_url && x >= draw_x && x < draw_x + wpx && y >= draw_y && y < draw_y + lh) { bd->link_hover_idx = i; goto end_hover; }
                draw_x += wpx + sw;
            }
        }
    }
    end_hover:
    if (bd->sb_is_dragging) {
        int max_scroll = t->content_height - t->view_height; if (max_scroll < 0) max_scroll = 0;
        int travel = bd->bar_height - bd->thumb_height;
        int dy = y - bd->drag_start_y;
        int no = bd->drag_start_offset + (travel > 0 ? (dy * max_scroll) / travel : 0);
        if (no < 0) no = 0;
        if (no > max_scroll) no = max_scroll;
        t->scroll_offset = no; is_dirty(TRUE);
    }
    if (old_go != bd->mouse_hover_go || old_back != bd->mouse_hover_back ||
        old_tab != bd->mouse_hover_tab || old_newtab != bd->mouse_hover_newtab ||
        old_close != bd->mouse_hover_close ||
        was_hov != bd->sb_is_hovered || old_link != bd->link_hover_idx ||
        old_sub != bd->hover_submit_idx) is_dirty(TRUE);
}

static char* unwrap_ddg_redirect(const char* url)
{
    const char* marker = "uddg=";
    const char* p = strstr(url, marker);
    if (!p) return NULL;
    p += strlen(marker);

    int len = strlen(p);
    char* out = (char*)kmalloc(len + 1);
    char* dst = out;
    while (*p && *p != '&') {
        if (*p == '%' &&
            ((p[1] >= '0' && p[1] <= '9') || (p[1] >= 'a' && p[1] <= 'f') || (p[1] >= 'A' && p[1] <= 'F')) &&
            ((p[2] >= '0' && p[2] <= '9') || (p[2] >= 'a' && p[2] <= 'f') || (p[2] >= 'A' && p[2] <= 'F')))
        {
            int hi = (p[1] >= 'a') ? p[1]-'a'+10 : (p[1] >= 'A') ? p[1]-'A'+10 : p[1]-'0';
            int lo = (p[2] >= 'a') ? p[2]-'a'+10 : (p[2] >= 'A') ? p[2]-'A'+10 : p[2]-'0';
            *dst++ = (char)((hi << 4) | lo);
            p += 3;
        } else if (*p == '+') {
            *dst++ = ' '; p++;
        } else {
            *dst++ = *p++;
        }
    }
    *dst = '\0';
    return out;
}
void browser_on_mouse_down(Window* win, int x, int y)
{
    Process* p = get_process(win->pid);
    BrowserMeta* bd = (BrowserMeta*)p->data;
    BrowserTab* t = active_tab(bd);
    int cw = win->width - (2 * WIN_BORDER);
    int url_bar_rel_y = TAB_BAR_HEIGHT + WIN_BORDER;

    if (y >= 0 && y < TAB_BAR_HEIGHT) {
        int tab_w = (bd->tab_count > 0) ? (cw - 26) / bd->tab_count : (cw - 26);
        if (tab_w > TAB_MAX_WIDTH) tab_w = TAB_MAX_WIDTH;
        if (tab_w < TAB_MIN_WIDTH) tab_w = TAB_MIN_WIDTH;
        for (int i = 0; i < bd->tab_count; i++) {
            if (x >= i * tab_w && x < (i + 1) * tab_w) {
                int close_x_min = (i + 1) * tab_w - 18;
                int close_x_max = (i + 1) * tab_w - 4;
                int close_y_min = 6;
                int close_y_max = 18;
                if (x >= close_x_min && x < close_x_max && y >= close_y_min && y < close_y_max) {
                    close_tab(win, i);
                } else {
                    bd->active_tab = i;
                    is_dirty(TRUE);
                }
                return;
            }
        }
        int plus_x_rel = bd->tab_count * tab_w;
        if (x >= plus_x_rel && x < plus_x_rel + 24 && bd->tab_count < MAX_TABS) {
            tab_init(&bd->tabs[bd->tab_count], bd->next_tab_id++);
            tab_load_homepage(&bd->tabs[bd->tab_count]);
            browser_fetch_images(win, &bd->tabs[bd->tab_count]);
            bd->active_tab = bd->tab_count++;
            is_dirty(TRUE);
        }
        return;
    }

    int ix = 78, iw = cw - 138;
    if (x >= ix && x < ix + iw && y >= url_bar_rel_y + 5 && y < url_bar_rel_y + 25) {
        bd->url_bar_focused = TRUE; 
        bd->url_cursor_pos = strlen(t->url);
        t->render_list.focused_element = -1; 
        is_dirty(TRUE); return;
    }

    int btn_x = cw - 50;
    if (x >= btn_x && x < btn_x + BUTTON_WIDTH && y >= url_bar_rel_y + 5 && y < url_bar_rel_y + 5 + BUTTON_HEIGHT) {
        search(win); return;
    }
    
    if (x >= 4 && x < 4 + BACK_BUTTON_WIDTH && y >= url_bar_rel_y + 5 && y < url_bar_rel_y + 5 + BUTTON_HEIGHT) {
        const char* prev = history_go_back(&t->history);
        if (prev) { strncpy(t->url, prev, 1023); t->url[1023] = '\0'; browser_fetch(win, t->url, NULL); }
        return;
    }

    //page area
    int page_rel_y = TAB_BAR_HEIGHT + URL_BAR_HEIGHT + WIN_BORDER;
    if (y < page_rel_y) return;
    int left_m = 8, right_m = cw - 24;
    const int base_lh = 18, base_cw = 11, base_sw = 4;
    int draw_x = left_m, draw_y = page_rel_y + 2 - t->scroll_offset;
    bd->url_bar_focused = FALSE;

    for (int i = 0; i < t->render_list.count; i++) {
        RenderElement* el = &t->render_list.elements[i];
        int scale = (el->scale > 0) ? el->scale : 1;
        int lh = base_lh * scale, cw2 = (base_cw - 1) * scale + (el->bold ? scale : 0), sw = base_sw * scale;
        if (el->type == ELEM_NEWLINE || el->newline) { draw_x = left_m; draw_y += lh; continue; }
        if (el->type == ELEM_INPUT_HIDDEN) continue;
        if (el->type == ELEM_INPUT_TEXT) {
            if (x >= draw_x && x < draw_x + el->input_width && y >= draw_y && y < draw_y + INPUT_HEIGHT)
                { t->render_list.focused_element = i; is_dirty(TRUE); return; }
            draw_x += el->input_width + 11; continue;
        }
        if (el->type == ELEM_INPUT_SUBMIT) {
            if (x >= draw_x && x < draw_x + el->input_width && y >= draw_y && y < draw_y + SUBMIT_HEIGHT)
                { resp_form(win, el->form_id); return; }
            draw_x += el->input_width + 11; continue;
        }
        if (el->type == ELEM_IMAGE) {
            int iw = el->img_width > 0 ? el->img_width : 64;
            int ih = el->img_height > 0 ? el->img_height : 64;
            if (draw_x + iw > right_m && draw_x > left_m) { draw_x = left_m; draw_y += lh; }
            if (el->link_url && x >= draw_x && x < draw_x + iw && y >= draw_y && y < draw_y + ih) {
                char nav_url[1024]; resolve_url(t->url, el->link_url, nav_url, sizeof(nav_url));
                char* unwrapped = unwrap_ddg_redirect(nav_url);
                if (unwrapped) { strncpy(nav_url, unwrapped, sizeof(nav_url) - 1); nav_url[sizeof(nav_url)-1] = '\0'; kfree(unwrapped); }
                strncpy(t->url, nav_url, 1023); t->url[1023] = '\0';
                browser_fetch(win, t->url, NULL); return;
            }
            draw_x += iw + 4;
            if (ih > lh) draw_y += ih - lh;
            continue;
        }
        if (!el->text) continue;
        const char* str = el->text;
        while (*str) {
            if (*str == '\n') { draw_x = left_m; draw_y += lh; str++; continue; }
            int w = 0; while (*str && *str != ' ' && *str != '\n' && w < 127) { w++; str++; }
            if (*str == ' ') str++;
            if (w == 0) continue;
            int wpx = w * cw2;
            if (draw_x + wpx > right_m && draw_x > left_m) { draw_x = left_m; draw_y += lh; }
            if (el->link_url && x >= draw_x && x < draw_x + wpx && y >= draw_y && y < draw_y + lh) {
                char nav_url[1024]; resolve_url(t->url, el->link_url, nav_url, sizeof(nav_url));
                char* unwrapped = unwrap_ddg_redirect(nav_url);
                if (unwrapped) { strncpy(nav_url, unwrapped, sizeof(nav_url) - 1); nav_url[sizeof(nav_url)-1] = '\0'; kfree(unwrapped); }
                strncpy(t->url, nav_url, 1023); t->url[1023] = '\0';
                browser_fetch(win, t->url, NULL); return;
            }
            draw_x += wpx + sw;
        }
    }

    if (x >= bd->thumb_x && x < bd->thumb_x + bd->thumb_width &&
        y >= bd->thumb_y && y < bd->thumb_y + bd->thumb_height) {
        bd->sb_is_dragging = TRUE; bd->drag_start_y = y; bd->drag_start_offset = t->scroll_offset;
    }
    is_dirty(TRUE);
}

void browser_on_mouse_up(Window* win, int x __attribute__((unused)), int y __attribute__((unused))) {
    ((BrowserMeta*)get_process(win->pid)->data)->sb_is_dragging = FALSE;
}

void browser_on_scroll(Window* win, int delta) {
    active_tab((BrowserMeta*)get_process(win->pid)->data)->scroll_offset += delta * 18;
    is_dirty(TRUE);
}

void browser_on_key_press(Window* win, unsigned int c)
{
    Process* p = get_process(win->pid);
    BrowserMeta* bd = (BrowserMeta*)p->data;
    BrowserTab* t = active_tab(bd);
    int focused = t->render_list.focused_element;

    if (focused >= 0 && focused < t->render_list.count) {
        RenderElement* el = &t->render_list.elements[focused];
        if (el->type == ELEM_INPUT_TEXT) {
            if (c == '\n' || c == '\r') { resp_form(win, el->form_id); }
            else if (c == '\b') { int len = strlen(el->field_value); if (len > 0) el->field_value[len - 1] = '\0'; }
            else if (c >= 32 && c <= 126) { int len = strlen(el->field_value); if (len < (int)sizeof(el->field_value) - 1) { el->field_value[len] = c; el->field_value[len + 1] = '\0'; } }
            else if (c == '\t') {
                for (int i = focused + 1; i < t->render_list.count; i++)
                    if (t->render_list.elements[i].type == ELEM_INPUT_TEXT) { t->render_list.focused_element = i; goto key_done; }
                for (int i = 0; i < focused; i++)
                    if (t->render_list.elements[i].type == ELEM_INPUT_TEXT) { t->render_list.focused_element = i; goto key_done; }
            }
            goto key_done;
        }
    }

    if (bd->url_bar_focused) {
        int len = strlen(t->url);
        if (c == '\n' || c == '\r') {
            search(win);
        } else if (c == '\b') {
            if (bd->url_cursor_pos > 0) {
                memmove(t->url + bd->url_cursor_pos - 1, t->url + bd->url_cursor_pos, len - bd->url_cursor_pos + 1);
                bd->url_cursor_pos--;
            }
        } else if (c == KEY_LEFT) {
            if (bd->url_cursor_pos > 0) bd->url_cursor_pos--;
        } else if (c == KEY_RIGHT) {
            if (bd->url_cursor_pos < len) bd->url_cursor_pos++;
        } else if (c == KEY_HOME) {
            bd->url_cursor_pos = 0;
        } else if (c == KEY_END) {
            bd->url_cursor_pos = len;
        } else if (c == KEY_DELETE) {
            if (bd->url_cursor_pos < len) {
                memmove(t->url + bd->url_cursor_pos, t->url + bd->url_cursor_pos + 1, len - bd->url_cursor_pos);
            }
        } else if ((unsigned char)c >= 32 && (unsigned char)c <= 126) {
            if (len < 254) {
                memmove(t->url + bd->url_cursor_pos + 1, t->url + bd->url_cursor_pos, len - bd->url_cursor_pos + 1);
                t->url[bd->url_cursor_pos] = c;
                bd->url_cursor_pos++;
            }
        }
    } else {
        if (c == '\n' || c == '\r') search(win);
        else if (c == '\b') { int len = strlen(t->url); if (len > 0) t->url[len - 1] = '\0'; }
        else if (c >= 32 && c <= 126) { int len = strlen(t->url); if (len < 254) { t->url[len] = c; t->url[len + 1] = '\0'; } }
    }

    key_done:
    is_dirty(TRUE);
}

void browser_cleanup(Window* win) {
    Process* p = get_process(win->pid);
    if (p && p->data) {
        BrowserMeta* bd = (BrowserMeta*)p->data;
        for (int i = 0; i < bd->tab_count; i++) tab_free(&bd->tabs[i]);
        kfree(p->data); p->data = NULL;
    }
}

void launch_reaper(void) {
    Process* p = create_process("Reaper Browser");
    if (!p) return;

    BrowserMeta* bd = (BrowserMeta*)kmalloc(sizeof(BrowserMeta));
    memset(bd, 0, sizeof(BrowserMeta));
    bd->mouse_hover_tab = -1;
    bd->mouse_hover_close = -1;
    bd->hover_submit_idx = -1;
    bd->link_hover_idx = -1;
    bd->url_bar_focused = FALSE;
    bd->next_tab_id = 1;

    tab_init(&bd->tabs[0], bd->next_tab_id++);
    tab_load_homepage(&bd->tabs[0]);
    bd->tab_count = 1;
    bd->active_tab = 0;
    bd->url_cursor_pos = 0;
    bd->url_view_offset = 0;
    p->data = (char*)bd;

    Window* w = window(p->pid, "Reaper Browser", -1, -1, 540, 440);
    if (!w) { tab_free(&bd->tabs[0]); kfree(bd); cleanup_process(p->pid); return; }

    w->content_renderer = browser_renderer;
    w->on_key_press = browser_on_key_press;
    w->on_mouse_move = browser_on_mouse_move;
    w->on_mouse_down = browser_on_mouse_down;
    w->on_mouse_up = browser_on_mouse_up;
    w->on_scroll = browser_on_scroll;
    w->on_close = browser_cleanup;
    browser_fetch_images(w, &bd->tabs[0]); 
}