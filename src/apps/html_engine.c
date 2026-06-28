#include "apps/html_engine.h"
#include "string.h"
#include "kheap.h"
#include "graphics.h"
#include "vesa.h"

typedef struct {
    uint32 color;
    BOOL bold;
    BOOL in_ignored_tag; // script, style, head, select will be ignored, add in v3.1
    int scale;
    char* link_url;
    int current_form_id;
} ParserState;

static uint32 parse_hex_color(const char* hex)
{
    if (hex[0] == '#') hex++;
    uint32 c = 0;
    for (int i = 0; i < 6; i++) {
        c <<= 4;
        if (hex[i] >= '0' && hex[i] <= '9') c |= (hex[i] - '0');
        else if (hex[i] >= 'a' && hex[i] <= 'f') c |= (hex[i] - 'a' + 10);
        else if (hex[i] >= 'A' && hex[i] <= 'F') c |= (hex[i] - 'A' + 10);
    }
    return RGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}

static void parse_style(const char* style, ParserState* state)
{
    char* p = strstr(style, "color:");
    if (!p) return;
    while (*p != '#' && !isalpha(*p) && *p != '\0') p++;
    if (*p == '#') state->color = parse_hex_color(p);
}

static int decode_entity(const char* p, char* out, int* src_advance)
{
    int len = 0;
    while (p[len] && p[len] != ';' && p[len] != '<' && p[len] != '&' && len < 16)
        len++;

    int has_semi = (p[len] == ';');
    *src_advance = len + (has_semi ? 1 : 0);

    if (strncmp(p, "nbsp", 4) == 0 && (has_semi || len == 4)) { out[0] = ' '; return 1; }
    else if (strncmp(p, "amp", 3) == 0 && (has_semi || len == 3)) { out[0] = '&'; return 1; }
    else if (strncmp(p, "lt", 2) == 0 && (has_semi || len == 2)) { out[0] = '<'; return 1; }
    else if (strncmp(p, "gt", 2) == 0 && (has_semi || len == 2)) { out[0] = '>'; return 1; }
    else if (strncmp(p, "quot", 4) == 0 && (has_semi || len == 4)) { out[0] = '"'; return 1; }
    else if (strncmp(p, "apos", 4) == 0 && (has_semi || len == 4)) { out[0] = '\''; return 1; }
    else if (strncmp(p, "copy", 4) == 0 && (has_semi || len == 4)) { out[0] = '('; out[1] = 'c'; out[2] = ')'; return 3; }
    else if (strncmp(p, "reg", 3) == 0 && (has_semi || len == 3)) { out[0] = '('; out[1] = 'R'; out[2] = ')'; return 3; }
    else if (strncmp(p, "mdash", 5) == 0 && (has_semi || len == 5)) { out[0] = '-'; out[1] = '-'; return 2; }
    else if (strncmp(p, "ndash", 5) == 0 && (has_semi || len == 5)) { out[0] = '-'; return 1; }
    else if (strncmp(p, "laquo", 5) == 0 && (has_semi || len == 5)) { out[0] = '<'; out[1] = '<'; return 2; }
    else if (strncmp(p, "raquo", 5) == 0 && (has_semi || len == 5)) { out[0] = '>'; out[1] = '>'; return 2; }
    else if (strncmp(p, "ldquo", 5) == 0 && (has_semi || len == 5)) { out[0] = '"'; return 1; }
    else if (strncmp(p, "rdquo", 5) == 0 && (has_semi || len == 5)) { out[0] = '"'; return 1; }
    else if (strncmp(p, "lsquo", 5) == 0 && (has_semi || len == 5)) { out[0] = '\''; return 1; }
    else if (strncmp(p, "rsquo", 5) == 0 && (has_semi || len == 5)) { out[0] = '\''; return 1; }
    else if (strncmp(p, "hellip",6) == 0 && (has_semi || len == 6)) { out[0] = '.'; out[1] = '.'; out[2] = '.'; return 3; }
    else if (strncmp(p, "bull", 4) == 0 && (has_semi || len == 4)) { out[0] = '*'; return 1; }
    else if (strncmp(p, "middot",6) == 0 && (has_semi || len == 6)) { out[0] = '.'; return 1; }
    else if (strncmp(p, "trade", 5) == 0 && (has_semi || len == 5)) { out[0] = 'T'; out[1] = 'M'; return 2; }

    // numerics
    if (p[0] == '#') {
        int val = 0;
        if (p[1] == 'x' || p[1] == 'X') {
            const char* h = p + 2;
            while (*h && *h != ';') {
                val <<= 4;
                if (*h >= '0' && *h <= '9') val |= (*h - '0');
                else if (*h >= 'a' && *h <= 'f') val |= (*h - 'a' + 10);
                else if (*h >= 'A' && *h <= 'F') val |= (*h - 'A' + 10);
                h++;
            }
        } else {
            const char* d = p + 1;
            while (*d >= '0' && *d <= '9') { val = val * 10 + (*d - '0'); d++; }
        }

        if (val == 160) { out[0] = ' '; return 1; }
        if (val == 8211 || val == 8212) { out[0] = '-'; return 1; }
        if (val == 8216 || val == 8217) { out[0] = '\''; return 1; }
        if (val == 8220 || val == 8221) { out[0] = '"'; return 1; }
        if (val == 8230) { out[0] = '.'; out[1] = '.'; out[2] = '.'; return 3; }
        if (val >= 32 && val <= 126) { out[0] = (char)val; return 1; }
        
        return 0;
    }

    *src_advance = 0;
    out[0] = '&';
    return 1;
}

static char* extract_attribute(const char* tag, const char* attr)
{
    char* p = strstr(tag, attr);
    if (!p) return NULL;
    p += strlen(attr);
    while (*p == ' ' || *p == '=') p++;
    char quote = 0;
    if (*p == '"' || *p == '\'') quote = *p++;
    const char* start = p;
    while (*p && (quote ? (*p != quote) : (*p != ' ' && *p != '>'))) p++;
    int len = p - start;
    char* res = (char*)kmalloc(len + 1);
    memcpy(res, start, len);
    res[len] = '\0';
    return res;
}

static void extract_attr_buf(const char* tag, const char* attr,
                              char* out, int out_len)
{
    char* val = extract_attribute(tag, attr);
    if (val) { strncpy(out, val, out_len - 1); out[out_len - 1] = '\0'; kfree(val); }
    else out[0] = '\0';
}

static void percent_encode(const char* src, char* dst, int dst_len)
{
    static const char hex[] = "0123456789ABCDEF";
    int di = 0;
    for (; *src && di < dst_len - 3; src++) {
        unsigned char c = (unsigned char)*src;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
            dst[di++] = (char)c;
        else if (c == ' ') dst[di++] = '+';
        else { dst[di++] = '%'; dst[di++] = hex[c >> 4]; dst[di++] = hex[c & 0xF]; }
    }
    dst[di] = '\0';
}

int html_form_encode(const RenderList* list, int form_id,
                     char* out_buf, int out_len)
{
    int written = 0;
    BOOL first = TRUE;
    for (int i = 0; i < list->count && written < out_len - 1; i++) {
        const RenderElement* el = &list->elements[i];
        if (el->type != ELEM_INPUT_TEXT && el->type != ELEM_INPUT_HIDDEN && el->type != ELEM_INPUT_SUBMIT) continue;
        if (el->form_id != form_id) continue;
        if (el->field_name[0] == '\0') continue;
        if (el->type == ELEM_INPUT_SUBMIT && el->field_name[0] == '\0') continue;
        if (!first && written < out_len - 1) out_buf[written++] = '&';
        first = FALSE;
        char enc[512];
        percent_encode(el->field_name, enc, sizeof(enc));
        int nlen = strlen(enc);
        if (written + nlen >= out_len - 1) break;
        memcpy(out_buf + written, enc, nlen); written += nlen;
        if (written < out_len - 1) out_buf[written++] = '=';
        percent_encode(el->field_value, enc, sizeof(enc));
        int vlen = strlen(enc);
        if (written + vlen >= out_len - 1) break;
        memcpy(out_buf + written, enc, vlen); written += vlen;
    }
    out_buf[written] = '\0';
    return written;
}

static void emit_newline(RenderList* list)
{
    if (list->count >= MAX_RENDER_ELEMENTS) return;
    RenderElement* el = &list->elements[list->count++];
    memset(el, 0, sizeof(*el));
    el->type = ELEM_NEWLINE;
    el->newline = TRUE;
    el->scale = 1;
}

static void decode_entities(char* s)
{
    if (!s) return;
    char* src = s;
    char* dst = s;
    while (*src) {
        if (*src == '&') {
            src++;
            char decoded[8];
            int advance = 0;
            int nout = decode_entity(src, decoded, &advance);
            src += advance;
            for (int i = 0; i < nout; i++) *dst++ = decoded[i];
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static void tag_tolower(const char* tag, char* out, int out_len) {
    int i = 0;
    while (tag[i] && tag[i] != ' ' && tag[i] != '\t' && tag[i] != '/' && i < out_len - 1) {
        char c = tag[i];
        if (c >= 'A' && c <= 'Z') c += 32;
        out[i] = c;
        i++;
    }
    out[i] = '\0';
}

void html_parse(const char* html, RenderList* list)
{
    list->count = 0;
    list->form_count = 0;
    list->focused_element = -1;

    ParserState state = {
        .color = RGB(20, 20, 20),
        .bold = FALSE,
        .in_ignored_tag = FALSE,
        .scale = 1,
        .link_url = NULL,
        .current_form_id = -1,
    };

    const char* p = html;
    char buffer[2048];
    int buf_idx = 0;
    // wanna collapse the whitespaces
    BOOL last_was_space = TRUE;

#define FLUSH_TEXT() do { \
    while (buf_idx > 0 && buffer[buf_idx - 1] == ' ') { \
        buf_idx--; \
        last_was_space = TRUE; \
    } \
    if (buf_idx > 0 && !state.in_ignored_tag && \
        list->count < MAX_RENDER_ELEMENTS) { \
        RenderElement* _el = &list->elements[list->count++]; \
        memset(_el, 0, sizeof(*_el)); \
        _el->type = ELEM_TEXT; \
        _el->text = (char*)kmalloc(buf_idx + 1); \
        memcpy(_el->text, buffer, buf_idx); \
        _el->text[buf_idx] = '\0'; \
        _el->color = state.link_url ? RGB(0, 0, 238) : state.color; \
        _el->bold = state.bold; \
        _el->newline = FALSE; \
        _el->scale = state.scale; \
        _el->link_url = state.link_url ? strdup(state.link_url) : NULL; \
        _el->form_id = -1; \
        buf_idx = 0; \
    } else { \
        buf_idx = 0; \
    }\
    last_was_space = TRUE; \
} while (0) 
    while (*p && list->count < MAX_RENDER_ELEMENTS) {
        if (*p == '&' && !state.in_ignored_tag) {
            p++;
            char decoded[8];
            int advance = 0;
            int nout = decode_entity(p, decoded, &advance);
            p += advance;
            for (int i = 0; i < nout && buf_idx < 2046; i++) {
                char c = decoded[i];
                buffer[buf_idx++] = c;
                last_was_space = (c == ' ' || c == '\t');
            }
            if (buf_idx >= 2047) FLUSH_TEXT();
            continue;
        }

        if (*p == '<') {
            FLUSH_TEXT();
            p++;

            char tag[512]; int tag_idx = 0;
            while (*p && *p != '>') { if (tag_idx < 511) tag[tag_idx++] = *p; p++; }
            tag[tag_idx] = '\0';
            if (*p == '>') p++;

            char* tp = tag;
            while (*tp == ' ' || *tp == '\t') tp++;

            BOOL is_close = (*tp == '/');
            if (is_close) tp++;

            char tname[32];
            tag_tolower(tp, tname, sizeof(tname));

            if (strncmp(tp, "!--", 3) == 0) {
                //rewind, scan forward
                while (*p && !(p[0] == '-' && p[1] == '-' && p[2] == '>')) p++;
                if (*p) p += 3;
                continue;
            }

            if (!is_close && (strcmp(tname, "script") == 0 ||
                            strcmp(tname, "style") == 0 ||
                            strcmp(tname, "head") == 0 ||
                            strcmp(tname, "select") == 0 ||
                            strcmp(tname, "noscript") == 0 ||
                            strcmp(tname, "iframe") == 0 ||
                            strcmp(tname, "svg") == 0 ||
                            strcmp(tname, "math") == 0 ||
                            strcmp(tname, "canvas") == 0 ||
                            strcmp(tname, "video") == 0 ||
                            strcmp(tname, "audio") == 0 ||
                            strcmp(tname, "object") == 0 ||
                            strcmp(tname, "embed") == 0 ||
                            strcmp(tname, "template") == 0 ||
                            strcmp(tname, "textarea") == 0 ||
                            strcmp(tname, "title") == 0)) {
                char closing[40];
                closing[0] = '<'; closing[1] = '/';
                int ci = 2;
                for (int ti = 0; tname[ti] && ci < 38; ti++) closing[ci++] = tname[ti];
                closing[ci] = '\0';
                while (*p) {
                    if (*p == '<') {
                        const char* q = p;
                        int match = 1;
                        for (int ci2 = 0; closing[ci2]; ci2++) {
                            if (*q == '\0') { match = 0; break; }
                            char c1 = closing[ci2];
                            char c2 = *q;
                            if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
                            if (c1 != c2) { match = 0; break; }
                            q++;
                        }
                        if (match) {
                            while (*p && *p != '>') p++;
                            if (*p == '>') p++;
                            break;
                        }
                    }
                    p++;
                }
                last_was_space = TRUE;

            } else if (!is_close && strcmp(tname, "option") == 0) {
                while (*p && *p != '<') p++;

            } else if (strcmp(tname, "br") == 0 ||
                    strcmp(tname, "p") == 0 ||
                    strcmp(tname, "div") == 0 ||
                    strcmp(tname, "ul") == 0 ||
                    strcmp(tname, "ol") == 0 ||
                    strcmp(tname, "dl") == 0 ||
                    strcmp(tname, "table") == 0 ||
                    strcmp(tname, "tr") == 0 ||
                    strcmp(tname, "td") == 0 ||
                    strcmp(tname, "th") == 0 ||
                    strcmp(tname, "hr") == 0 ||
                    strcmp(tname, "nav") == 0 ||
                    strcmp(tname, "header") == 0 ||
                    strcmp(tname, "footer") == 0 ||
                    strcmp(tname, "main") == 0 ||
                    strcmp(tname, "section") == 0 ||
                    strcmp(tname, "article") == 0 ||
                    strcmp(tname, "aside") == 0 ||
                    strcmp(tname, "pre") == 0 ||
                    strcmp(tname, "blockquote") == 0 ||
                    strcmp(tname, "figure") == 0 ||
                    strcmp(tname, "details") == 0 ||
                    strcmp(tname, "summary") == 0 ||
                    strcmp(tname, "dialog") == 0 ||
                    strcmp(tname, "address") == 0 ||
                    strcmp(tname, "menu") == 0) {
                emit_newline(list);
                last_was_space = TRUE;

            } else if (strcmp(tname, "li") == 0) {
                emit_newline(list);
                if (list->count < MAX_RENDER_ELEMENTS) {
                    RenderElement* el = &list->elements[list->count++];
                    memset(el, 0, sizeof(*el));
                    el->type = ELEM_TEXT;
                    el->text = (char*)kmalloc(3);
                    strcpy(el->text, "* ");
                    el->color = state.color;
                    el->bold = state.bold;
                    el->scale = state.scale;
                    el->form_id = -1;
                }
                last_was_space = TRUE;

            } else if (strcmp(tname, "dt") == 0) {
                emit_newline(list);
                last_was_space = TRUE;

            } else if (strcmp(tname, "dd") == 0) {
                emit_newline(list);
                if (list->count < MAX_RENDER_ELEMENTS) {
                    RenderElement* el = &list->elements[list->count++];
                    memset(el, 0, sizeof(*el));
                    el->type = ELEM_TEXT;
                    el->text = (char*)kmalloc(3);
                    strcpy(el->text, " ");
                    el->color = state.color;
                    el->bold = state.bold;
                    el->scale = state.scale;
                    el->form_id = -1;
                }
                last_was_space = TRUE;

            } else if (!is_close && tname[0] == 'h' &&
                    tname[1] >= '1' && tname[1] <= '6' && tname[2] == '\0') {
                emit_newline(list);
                state.bold = TRUE;
                int level = tname[1] - '0';
                state.scale = (level <= 2) ? 2 : 1;

            } else if (is_close && tname[0] == 'h' &&
                    tname[1] >= '1' && tname[1] <= '6' && tname[2] == '\0') {
                state.bold = FALSE;
                state.scale = 1;
                emit_newline(list);
                last_was_space = TRUE;
            } else if (!is_close && (strcmp(tname, "b") == 0 ||
                                    strcmp(tname, "strong") == 0 ||
                                    strcmp(tname, "em") == 0 ||
                                    strcmp(tname, "i") == 0 ||
                                    strcmp(tname, "cite") == 0 ||
                                    strcmp(tname, "dfn") == 0 ||
                                    strcmp(tname, "mark") == 0 ||
                                    strcmp(tname, "var") == 0)) {
                state.bold = TRUE;
            } else if ( is_close && (strcmp(tname, "b") == 0 ||
                                    strcmp(tname, "strong") == 0 ||
                                    strcmp(tname, "em") == 0 ||
                                    strcmp(tname, "i") == 0 ||
                                    strcmp(tname, "cite") == 0 ||
                                    strcmp(tname, "dfn") == 0 ||
                                    strcmp(tname, "mark") == 0 ||
                                    strcmp(tname, "var") == 0)) {
                state.bold = FALSE;

            // treating inline code as text rn, add in v3.1
            } else if (!is_close && (strcmp(tname, "code") == 0 ||
                                    strcmp(tname, "kbd") == 0 ||
                                    strcmp(tname, "samp") == 0 ||
                                    strcmp(tname, "tt") == 0)) {//stubs
            } else if (is_close && (strcmp(tname, "code") == 0 ||
                                    strcmp(tname, "kbd") == 0 ||
                                    strcmp(tname, "samp") == 0 ||
                                    strcmp(tname, "tt") == 0)) { //stubs
            } else if (!is_close && strcmp(tname, "span") == 0) {
                parse_style(tp, &state);
            } else if ( is_close && strcmp(tname, "span") == 0) {
                state.color = RGB(20, 20, 20);
            } else if (!is_close && strcmp(tname, "a") == 0) {
                if (state.link_url) kfree(state.link_url);
                state.link_url = extract_attribute(tp, "href");
                decode_entities(state.link_url);
            } else if ( is_close && strcmp(tname, "a") == 0) {
                if (state.link_url) kfree(state.link_url);
                state.link_url = NULL;

            } else if (!is_close && strcmp(tname, "form") == 0) {
                if (list->form_count < MAX_FORM_FIELDS) {
                    FormDescriptor* fd = &list->forms[list->form_count];
                    fd->id = list->form_count;
                    extract_attr_buf(tp, "action", fd->action, sizeof(fd->action));
                    decode_entities(fd->action);
                    extract_attr_buf(tp, "method", fd->method, sizeof(fd->method));
                    for (char* m = fd->method; *m; m++) if (*m >= 'a' && *m <= 'z') *m -= 32;
                    if (fd->method[0] == '\0') strcpy(fd->method, "GET");
                    state.current_form_id = list->form_count;
                    list->form_count++;
                }
                emit_newline(list);
            } else if (is_close && strcmp(tname, "form") == 0) {
                state.current_form_id = -1;
                emit_newline(list);

            } else if (!is_close && strcmp(tname, "input") == 0) {
                if (list->count < MAX_RENDER_ELEMENTS) {
                    RenderElement* el = &list->elements[list->count++];
                    memset(el, 0, sizeof(*el));
                    el->form_id = state.current_form_id;
                    extract_attr_buf(tp, "name", el->field_name, sizeof(el->field_name));
                    extract_attr_buf(tp, "value", el->field_value, sizeof(el->field_value));
                    extract_attr_buf(tp, "placeholder", el->placeholder, sizeof(el->placeholder));
                    char size_str[16]; extract_attr_buf(tp, "size", size_str, sizeof(size_str));
                    int size_chars = size_str[0] ? (int)strtol(size_str, NULL, 10) : 20;
                    el->input_width = size_chars * 8 + 10;
                    if (el->input_width < 60) el->input_width = 60;
                    if (el->input_width > 400) el->input_width = 400;
                    char type_str[32]; extract_attr_buf(tp, "type", type_str, sizeof(type_str));
                    for (char* c = type_str; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32;
                    if (strcmp(type_str, "submit") == 0) { el->type = ELEM_INPUT_SUBMIT; if (el->field_value[0] == '\0') strcpy(el->field_value, "Submit"); }
                    else if (strcmp(type_str, "hidden") == 0) el->type = ELEM_INPUT_HIDDEN;
                    else if (strcmp(type_str, "checkbox") == 0) el->type = ELEM_INPUT_HIDDEN;
                    else if (strcmp(type_str, "radio") == 0) el->type = ELEM_INPUT_HIDDEN;
                    else { el->type = ELEM_INPUT_TEXT; el->is_password = (strcmp(type_str, "password") == 0); }
                }

            } else if (!is_close && strcmp(tname, "button") == 0) {
                if (list->count < MAX_RENDER_ELEMENTS) {
                    RenderElement* el = &list->elements[list->count++];
                    memset(el, 0, sizeof(*el));
                    el->type = ELEM_INPUT_SUBMIT;
                    el->form_id = state.current_form_id;
                    extract_attr_buf(tp, "name", el->field_name, sizeof(el->field_name));
                    extract_attr_buf(tp, "value", el->field_value, sizeof(el->field_value));
                    if (el->field_value[0] == '\0') strcpy(el->field_value, "Submit");
                    el->input_width = 80;
                }

            } else if (!is_close && strcmp(tname, "img") == 0) {
                if (list->count < MAX_RENDER_ELEMENTS) {
                    RenderElement* el = &list->elements[list->count++];
                    memset(el, 0, sizeof(*el));
                    el->type = ELEM_IMAGE;
                    el->form_id = -1;
                    extract_attr_buf(tp, "src", el->src_url, sizeof(el->src_url));
                    decode_entities(el->src_url);
                    extract_attr_buf(tp, "alt", el->alt_text, sizeof(el->alt_text));

                    char dim_str[16];
                    extract_attr_buf(tp, "width", dim_str, sizeof(dim_str));
                    el->img_width = dim_str[0] ? (int)strtol(dim_str, NULL, 10) : 0;
                    extract_attr_buf(tp, "height", dim_str, sizeof(dim_str));
                    el->img_height = dim_str[0] ? (int)strtol(dim_str, NULL, 10) : 0;
                    el->pixels = NULL;
                }

            }

            continue;
        }

        if (!state.in_ignored_tag) {
            char c = *p;

            // collapse ws
            if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
                if (!last_was_space) {
                    buffer[buf_idx++] = ' ';
                    last_was_space = TRUE;
                }
            } else {
                buffer[buf_idx++] = c;
                last_was_space = FALSE;
            }

            if (buf_idx >= 2047) FLUSH_TEXT();
        }
        p++;
    }

    FLUSH_TEXT();
    if (state.link_url) kfree(state.link_url);

#undef FLUSH_TEXT
}

void html_free_list(RenderList* list)
{
    for (int i = 0; i < list->count; i++) {
        RenderElement* el = &list->elements[i];
        if (el->text) { kfree(el->text); el->text = NULL; }
        if (el->link_url) { kfree(el->link_url); el->link_url = NULL; }
        if (el->pixels) { kfree(el->pixels); el->pixels = NULL; }
        if (el->frames) {
            for (int f = 0; f < el->frame_count; f++) {
                if (el->frames[f]) kfree(el->frames[f]);
            }
            kfree(el->frames);
            el->frames = NULL;
        }
        if (el->delays) { kfree(el->delays); el->delays = NULL; }
    }
    list->count = 0;
    list->form_count = 0;
    list->focused_element = -1;
}