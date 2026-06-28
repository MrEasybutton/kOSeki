#include "apps/CLStudio.h"
#include "procsys.h"
#include "gui.h"
#include "vesa.h"
#include "graphics.h"
#include "kheap.h"
#include "string.h"
#include "ac97.h"
#include "synth.h"
#include "kmath.h"
#include "serial.h"

//i was working on this ever since the sound update, forgive me for not using PON API yet
// pushing that forward to v3.1 (level 67 procrastinator)

extern uint32 timer_ticks;
static uint32 get_tick_ms() { return (timer_ticks * 1000) / 67; }

int atoi(const char* str) {
    int result = 0;
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] < '0' || str[i] > '9') break;
        result = result * 10 + (str[i] - '0');
    }
    return result;
}

extern void button_v(int x, int y, int width, int height, BUTTON_STATE state, uint8 r, uint8 g, uint8 b);

#define STEP_WIDTH 20
#define TABS_WIDTH 60
#define KEYS_WIDTH 30
#define DRAWER_WIDTH 140
#define MENU_BAR_HEIGHT 34
#define SCROLLBAR_H 16
#define PLAYHEAD_HEADER_H 12
#define BR_PARAM_LEN 9
#define BR_PANEL_W 172
#define BR_PANEL_H 240
#define BR_ROW_H 24
#define DROPDOWN_H 20
#define DROPDOWN_ITEM_H 16
#define DAW_DIALOG_W 300
#define DAW_DIALOG_H 160

typedef struct {
    int cx, cy, cw, ch;
    int keys_x, pr_x, pr_y_base, pr_y, drawer_x, pr_w_vis;

    int dr_x, drawer_ys, drawer_h;
    int dd_abs_x, dd_w;
    int preset_dd_abs_y;
    int edit_btn_y;
    int clr_btn_y;
} daw_layout_t;

static void init_layout(const Window* win, daw_layout_t* L) {
    L->cx = win->x + 4;
    L->cy = win->y + 24;
    L->cw = win->width - 8;
    L->ch = win->height - 28;

    L->keys_x = L->cx + TABS_WIDTH + 5;
    L->pr_x = L->keys_x + KEYS_WIDTH;
    L->drawer_x = L->cx + L->cw - DRAWER_WIDTH;
    L->pr_w_vis = L->drawer_x - L->pr_x;
    L->pr_y_base = L->cy + MENU_BAR_HEIGHT + 5;
    L->pr_y = L->pr_y_base + PLAYHEAD_HEADER_H;

    L->dr_x = L->cx + L->cw - DRAWER_WIDTH;
    L->drawer_ys = L->cy + MENU_BAR_HEIGHT;
    L->drawer_h = L->ch - MENU_BAR_HEIGHT;
    L->dd_abs_x = L->dr_x + 10;
    L->dd_w = DRAWER_WIDTH - 20;

    L->preset_dd_abs_y = L->drawer_ys + 60;
    L->edit_btn_y = L->preset_dd_abs_y + DROPDOWN_H + 42;
    L->clr_btn_y = L->drawer_ys + L->drawer_h - 34;
}

//i was gonna do a visualiser with dynamic anims and stuff but i cant draw larger pixel art
static const char* BR_PRESET_NAMES[2] = { "E.R.B", "RISSA" };

static void apply_preset_BR(br_params_t* br, int preset) {
    if (preset == 0) {
        br->vowel = VOWEL_O; br->volume = 96;
        br->oq_start = 0.82f; br->oq_target = 0.52f;
        br->vib_rate_hz = 7.8f; br->vib_depth_pct = 1.80f;
        br->vib_onset_ms = 180; br->breath_amt = 24; br->shimmer_amt = 16;
    } else if (preset == 1) {
        br->vowel = VOWEL_E; br->volume = 96;
        br->oq_start = 0.96f; br->oq_target = 0.45f;
        br->vib_rate_hz = 4.2f; br->vib_depth_pct = 4.20f;
        br->vib_onset_ms = 420; br->breath_amt = 42; br->shimmer_amt = 12;
    }
}

static const char* BR_PARAM_LABELS[BR_PARAM_LEN] = {
    "VOWEL", "VOLUME", "OQ ONSET", "OQ TARGET",
    "VIB RATE", "VIB DEPTH", "VIB ONSET", "BREATH", "SHIMMER",
};

#pragma pack(push, 1)
typedef struct {
    char riff[4]; uint32 overall_size; char wave[4];
    char fmt_chunk_marker[4]; uint32 length_of_fmt;
    uint16 format_type; uint16 channels;
    uint32 sample_rate; uint32 byterate;
    uint16 block_align; uint16 bits_per_sample;
    char data_chunk_header[4]; uint32 data_size;
} wav_header_t;
#pragma pack(pop)

static float get_note_freq(int index) {
    float n = (float)(index + 60 - 69) / 12.0f;
    return 440.0f * expf(n * 0.69314718f);
}

static void init_track(track_sound_t* ts, int track_idx, int win_x, int win_y, int dr_x_local, int drawer_y_local) {
    ts->profile = PROFILE_BLOODRAVEN;
    ts->wave = WAVE_SINE;

    ts->br = (br_params_t){
        .vowel = VOWEL_A, .volume = 75,
        .oq_start = 0.80f, .oq_target = 0.70f,
        .vib_rate_hz = 5.8f, .vib_depth_pct = 1.73f,
        .vib_onset_ms = 200, .breath_amt = 40, .shimmer_amt = 4,
    };

    int dd_w = DRAWER_WIDTH - 20;

    ts->preset_dd = (dropdown_state_t){
        .x = dr_x_local, .y = drawer_y_local + 30,
        .w = dd_w, .open = FALSE, .hovered_item = -1,
    };
    ts->wave_dd = (dropdown_state_t){
        .x = dr_x_local, .y = drawer_y_local + 22,
        .w = dd_w, .open = FALSE, .hovered_item = -1,
    };
    ts->selected_preset = -1;

    ts->br_panel = (br_panel_state_t){
        .open = FALSE,
        .x = 4 + (760 - DRAWER_WIDTH - 20),
        .y = 24 + MENU_BAR_HEIGHT + 10,
        .dragging = FALSE, .drag_off_x = 0, .drag_off_y = 0,
        .hovered_row = -1, .editing_row = -1,
        .edit_start_x = 0, .edit_start_val = 0.0f,
    };

    (void)track_idx; (void)win_x; (void)win_y;
}

static synth_buf_t* get_note_buf(daw_state_t* state, int note_idx, int len_steps) {
    if (note_idx < 0 || note_idx >= DAW_NOTES) return NULL;
    if (len_steps <= 0 || len_steps > DAW_STEPS) return NULL;

    int slot = len_steps - 1;
    if (!state->pregen_notes[note_idx][slot]) {
        track_sound_t* ts = &state->sound[state->active_track];
        uint32 dur_ms = (uint32)(len_steps * 250);
        if (ts->profile == PROFILE_BLOODRAVEN)
            state->pregen_notes[note_idx][slot] = ac97_synth_opera_note(
                get_note_freq(note_idx), ts->br.vowel, dur_ms, ts->br.volume, 48000);
        else
            state->pregen_notes[note_idx][slot] = ac97_synth_generate(
                ts->wave, (uint32)get_note_freq(note_idx), dur_ms, ts->br.volume, 48000);
    }
    return state->pregen_notes[note_idx][slot];
}

static void note_preview(daw_state_t* state, int note_index) {
    synth_buf_t* b = get_note_buf(state, note_index, 1);
    if (!b) return;
    uint32 n = (48000 * 180 / 1000) * 2;
    if (n > b->num_samples) n = b->num_samples;
    ac97_play_buffer(b->samples, n, (uint16)b->sample_rate, NULL);
}

static void free_trkcache(daw_state_t* state) {
    for (int n = 0; n < DAW_NOTES; n++)
        for (int l = 0; l < DAW_STEPS; l++)
            if (state->pregen_notes[n][l]) {
                ac97_synth_free(state->pregen_notes[n][l]);
                state->pregen_notes[n][l] = NULL;
            }
}

static void start_playback(daw_state_t* state) {
    if (state->is_playing) return;
    state->is_playing = TRUE;
    ac97_stop_all();
    if (state->play_sentinel_buf) { kfree(state->play_sentinel_buf); state->play_sentinel_buf = NULL; }

    int start_step = (int)state->playhead_step;
    int last_end = start_step + 1;
    for (int t = 0; t < DAW_TRACKS; t++)
        for (int i = 0; i < MAX_NOTES_PER_TRACK; i++) {
            if (!state->tracks[t].notes[i].active) continue;
            int end = state->tracks[t].notes[i].start_step + state->tracks[t].notes[i].length_steps;
            if (end > last_end) last_end = end;
        }
    state->play_start_step = start_step;
    state->play_total_ms = (uint32)((last_end - start_step) * 250);
    state->playhead_step = (float)start_step;

    ac97_mixer_batch_begin();

    uint32 sentinel_samples = (48000 * state->play_total_ms / 1000) * 2;
    state->play_sentinel_buf = (sint16*)kmalloc(sentinel_samples * sizeof(sint16));
    if (state->play_sentinel_buf) {
        memset(state->play_sentinel_buf, 0, sentinel_samples * sizeof(sint16));
        state->play_handle = ac97_play_buffer(
            state->play_sentinel_buf, sentinel_samples, 48000, state->play_sentinel_buf);
    } else {
        state->play_handle = -1;
    }

    for (int t = 0; t < DAW_TRACKS; t++) {
        for (int i = 0; i < MAX_NOTES_PER_TRACK; i++) {
            if (!state->tracks[t].notes[i].active) continue;
            int note_idx = state->tracks[t].notes[i].note_index;
            int note_start = state->tracks[t].notes[i].start_step;
            int len = state->tracks[t].notes[i].length_steps;
            if (note_start + len <= start_step) continue;

            int effective_start = note_start - start_step;
            int trim_samples = 0;
            if (effective_start < 0) {
                trim_samples = (-effective_start) * 250 * 48000 / 1000 * 2;
                effective_start = 0;
            }
            synth_buf_t* b = get_note_buf(state, note_idx, len);
            if (!b) continue;
            sint16* pcm_start = b->samples + trim_samples;
            uint32 play_samples = b->num_samples > (uint32)trim_samples
                                 ? b->num_samples - (uint32)trim_samples : 0;
            if (play_samples == 0) continue;
            ac97_play_buffer_delayed(pcm_start, play_samples,
                                     (uint16)b->sample_rate, NULL, (uint32)(effective_start * 250));
        }
    }
    state->play_start_tick = get_tick_ms();
    ac97_mixer_batch_end();
}

static void stop_playback(daw_state_t* state) {
    state->is_playing = FALSE;
    state->play_handle = -1;
    if (state->play_sentinel_buf) state->play_sentinel_buf = NULL;
    ac97_stop_all();
}

static void reset_playhead(daw_state_t* state) {
    stop_playback(state);
    state->playhead_step = 0.0f;
}

static void icn_play(int x, int y, int size, uint32 color) {
    for (int i = 0; i < size; i++) rect(x + (size - 1 - i), y + (size / 2) - (i / 2), 1, i + 1, color);
}

static void icn_stop(int x, int y, int size, uint32 color) {
    rect(x, y, size, size, color);
}
static void icn_reset(int x, int y, int size, uint32 color) {
    rect(x, y, 2, size, color);
    for (int i = 0; i < size; i++) {
        rect(x + 3 + (size/2 - 1) - i/2, y + i/2, 1, size - i, color);
        rect(x + 3 + (size - 1) - i/2, y + i/2, 1, size - i, color);
    }
}
static void icn_export(int x, int y, int size, uint32 color) {
    int ax = x + size/2;
    rect(x, y + size - 2, size, 2, color);
    rect(x, y + size/2, 1, size/2, color);
    rect(x + size - 1, y + size/2, 1, size/2, color);
    rect(ax - 1, y, 2, size - 6, color);
    rect(ax - 3, y + size - 8, 7, 1, color);
    rect(ax - 2, y + size - 7, 5, 1, color);
    rect(ax - 1, y + size - 6, 3, 1, color);
    rect(ax, y + size - 5, 1, 1, color);
}

static int eval_preset_BR(br_params_t* br) {
    for (int i = 0; i < 2; i++) {
        br_params_t t; apply_preset_BR(&t, i);
        if (t.vowel == br->vowel && t.volume == br->volume &&
            (int)(t.oq_start * 100) == (int)(br->oq_start * 100) &&
            (int)(t.oq_target * 100) == (int)(br->oq_target * 100) &&
            (int)(t.vib_rate_hz * 100) == (int)(br->vib_rate_hz * 100) &&
            (int)(t.vib_depth_pct* 100) == (int)(br->vib_depth_pct* 100) &&
            t.vib_onset_ms == br->vib_onset_ms &&
            t.breath_amt == br->breath_amt &&
            t.shimmer_amt == br->shimmer_amt)
            return i;
    }
    return -1;
}

static void save_RYS(daw_state_t* state, const char* filename) {
    char* buf = (char*)kmalloc(65536);
    if (!buf) return;
    int p = 0;
    p += sprintf(buf + p, "RYSA\n");
    for (int t = 0; t < DAW_TRACKS; t++) {
        p += sprintf(buf + p, "TRACK %d %s\n", t, state->tracks[t].name);
        track_sound_t* ts = &state->sound[t];
        if (ts->profile == PROFILE_BLOODRAVEN)
            p += sprintf(buf + p, "SYNTH BLOODRAVEN %d %d %d %d %d %d %d %d %d\n",
                (int)ts->br.vowel, (int)ts->br.volume,
                (int)(ts->br.oq_start * 100 + 0.5f), (int)(ts->br.oq_target * 100 + 0.5f),
                (int)(ts->br.vib_rate_hz * 100 + 0.5f), (int)(ts->br.vib_depth_pct * 100 + 0.5f),
                (int)ts->br.vib_onset_ms, (int)ts->br.breath_amt, (int)ts->br.shimmer_amt);
        for (int i = 0; i < MAX_NOTES_PER_TRACK; i++) {
            if (!state->tracks[t].notes[i].active) continue;
            daw_note_t* n = &state->tracks[t].notes[i];
            p += sprintf(buf + p, "NOTE %d %d %d\n", n->note_index, n->start_step, n->length_steps);
        }
    }
    extern int fat_create_file(const char* path);
    extern int fat_write_file(const char* path, const char* data, uint32 size);
    fat_create_file(filename);
    int res = fat_write_file(filename, buf, p);
    if (res == 0) notif_handler("[CLStudio] Saved", "Your project has been saved successfully.");
    else notif_handler("[CLStudio] Error", "Failed to save the project as a .RYS. Please try that again.");
    kfree(buf);
}

static void load_RYS(daw_state_t* state, const char* filename) {
    char* data = fat_read_file((char*)filename);
    if (!data) return;
    if (memcmp(data, "RYSA", 4) != 0) { kfree(data); return; }

    for (int t = 0; t < DAW_TRACKS; t++)
        memset(state->tracks[t].notes, 0, sizeof(state->tracks[t].notes));

    char* line = strtok(data, "\n");
    int current_track = -1;
    while (line) {
        if (strncmp(line, "TRACK ", 6) == 0) {
            char* p = line + 6;
            int t_idx = atoi(p);
            if (t_idx >= 0 && t_idx < DAW_TRACKS) {
                current_track = t_idx;
                char* name = strchr(p, ' ');
                if (name) { while (*name == ' ') name++; strcpy(state->tracks[t_idx].name, name); }
            }
        } else if (strncmp(line, "SYNTH ", 6) == 0 && current_track != -1) {
            char* p = line + 6;
            if (strncmp(p, "BLOODRAVEN ", 11) == 0) {
                p += 11;
                track_sound_t* ts = &state->sound[current_track];
                ts->profile = PROFILE_BLOODRAVEN;
                ts->br.vowel = (vowel_t)atoi(p);
                p = strchr(p, ' '); if (p) { while (*p == ' ') p++; ts->br.volume = (uint8)atoi(p); }
                p = strchr(p, ' '); if (p) { while (*p == ' ') p++; ts->br.oq_start = (float)atoi(p) / 100.0f; }
                p = strchr(p, ' '); if (p) { while (*p == ' ') p++; ts->br.oq_target = (float)atoi(p) / 100.0f; }
                p = strchr(p, ' '); if (p) { while (*p == ' ') p++; ts->br.vib_rate_hz = (float)atoi(p) / 100.0f; }
                p = strchr(p, ' '); if (p) { while (*p == ' ') p++; ts->br.vib_depth_pct = (float)atoi(p) / 100.0f; }
                p = strchr(p, ' '); if (p) { while (*p == ' ') p++; ts->br.vib_onset_ms = (uint32)atoi(p); }
                p = strchr(p, ' '); if (p) { while (*p == ' ') p++; ts->br.breath_amt = (uint8)atoi(p); }
                p = strchr(p, ' '); if (p) { while (*p == ' ') p++; ts->br.shimmer_amt = (uint8)atoi(p); }
                ts->selected_preset = eval_preset_BR(&ts->br);
            }
        } else if (strncmp(line, "NOTE ", 5) == 0 && current_track != -1) {
            char* p = line + 5;
            int n_idx = atoi(p);
            p = strchr(p, ' '); if (!p) { line = strtok(NULL, "\n"); continue; }
            while (*p == ' ') p++;
            int s_step = atoi(p);
            p = strchr(p, ' '); if (!p) { line = strtok(NULL, "\n"); continue; }
            while (*p == ' ') p++;
            int l_steps = atoi(p);
            for (int i = 0; i < MAX_NOTES_PER_TRACK; i++) {
                if (!state->tracks[current_track].notes[i].active) {
                    state->tracks[current_track].notes[i] = (daw_note_t){
                        .active = TRUE, .note_index = n_idx,
                        .start_step = s_step, .length_steps = l_steps,
                    };
                    break;
                }
            }
        }
        line = strtok(NULL, "\n");
    }
    kfree(data);
    free_trkcache(state);
    is_dirty(TRUE);
}

static void export(daw_state_t* state) {
    if (state->dialog_input_pos == 0) return;

    char path[MAX_FILENAME_LEN + 8] = "/";
    strcat(path, state->dialog_input_buffer);
    char* ext = strrchr(path, '.');
    if (!ext) strcat(path, state->export_type == EXPORT_MODE_WAV ? ".WAV" : ".RYS");

    if (state->export_type == EXPORT_MODE_WAV) {
        uint32 sample_rate = 48000;
        uint32 total_ms = DAW_STEPS * 250;
        uint32 total_frames = (sample_rate * total_ms) / 1000;
        uint32 data_size = total_frames * 2 * sizeof(sint16);

        sint16* pcm = (sint16*)kmalloc(data_size);
        if (!pcm) { notif_handler("Export Error", "Failed to allocate PCM buffer."); return; }
        memset(pcm, 0, data_size);

        for (int t = 0; t < DAW_TRACKS; t++) {
            for (int i = 0; i < MAX_NOTES_PER_TRACK; i++) {
                if (!state->tracks[t].notes[i].active) continue;
                daw_note_t* n = &state->tracks[t].notes[i];
                synth_buf_t* b = get_note_buf(state, n->note_index, n->length_steps);
                if (!b) continue;
                uint32 start_frame = (n->start_step * 250 * sample_rate) / 1000;
                for (uint32 s = 0; s < b->num_samples; s++) {
                    uint32 idx = (start_frame * 2) + s;
                    if (idx < total_frames * 2) {
                        sint32 mixed = (sint32)pcm[idx] + (sint32)b->samples[s];
                        if (mixed > 32767) mixed = 32767;
                        if (mixed < -32768) mixed = -32768;
                        pcm[idx] = (sint16)mixed;
                    }
                }
            }
        }

        wav_header_t head;
        memcpy(head.riff, "RIFF", 4);
        head.overall_size = sizeof(wav_header_t) + data_size - 8;
        memcpy(head.wave, "WAVE", 4);
        memcpy(head.fmt_chunk_marker, "fmt ", 4);
        head.length_of_fmt = 16; head.format_type = 1; head.channels = 2;
        head.sample_rate = sample_rate; head.bits_per_sample = 16;
        head.byterate = (sample_rate * 2 * 16) / 8;
        head.block_align = (2 * 16) / 8;
        memcpy(head.data_chunk_header, "data", 4);
        head.data_size = data_size;

        uint32 total_file_size = sizeof(wav_header_t) + data_size;
        char* file_buf = (char*)kmalloc(total_file_size);
        if (file_buf) {
            memcpy(file_buf, &head, sizeof(wav_header_t));
            memcpy(file_buf + sizeof(wav_header_t), pcm, data_size);
            extern int fat_create_file(const char* path);
            extern int fat_write_file(const char* path, const char* data, uint32 size);
            fat_create_file(path);
            int res = fat_write_file(path, file_buf, total_file_size);
            if (res == 0) notif_handler("Exported", "Exported song as WAV successfully!");
            else notif_handler("Error exporting!!", "Failed to write WAV");
            kfree(file_buf);
        }
        kfree(pcm);
    } else {
        save_RYS(state, path);
    }
}

static void on_keypress(Window* win, unsigned int key) { (void)win; (void)key; }

static void dlg_keys(Window* win, unsigned int key) {
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    daw_state_t* state = (daw_state_t*)p->data;
    if (key == '\n') {
        export(state);
        state->dialog_mode = EXPORT_MODE_NONE;
        win->on_key_press = on_keypress;
    } else if (key == 27) {
        state->dialog_mode = EXPORT_MODE_NONE;
        win->on_key_press = on_keypress;
    } else if (key == '\b') {
        if (state->dialog_input_pos > 0)
            state->dialog_input_buffer[--state->dialog_input_pos] = '\0';
    } else if (state->dialog_input_pos < MAX_FILENAME_LEN - 1 && key >= 32 && key <= 126) {
        state->dialog_input_buffer[state->dialog_input_pos++] = (char)key;
        state->dialog_input_buffer[state->dialog_input_pos] = '\0';
    }
    is_dirty(TRUE);
}

static void prep_dlg_export(Window* win) {
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    daw_state_t* state = (daw_state_t*)p->data;
    state->dialog_mode = state->export_type = EXPORT_MODE_WAV;
    state->dialog_input_pos = 0;
    memset(state->dialog_input_buffer, 0, MAX_FILENAME_LEN);
    win->on_key_press = dlg_keys;
}

static void dlg_renderer(Window* win, daw_state_t* state) {
    int dx = win->x + (win->width - DAW_DIALOG_W) / 2;
    int dy = win->y + (win->height - DAW_DIALOG_H) / 2;

    rect(dx + 5, dy + 5, DAW_DIALOG_W, DAW_DIALOG_H, RGB(3, 6, 14));
    rect(dx, dy, DAW_DIALOG_W, DAW_DIALOG_H, RGB(10, 12, 25));
    rect(dx, dy, DAW_DIALOG_W, 16, RGB(10, 40, 90));
    rect(dx, dy + 16, DAW_DIALOG_W, 14, RGB(5, 25, 60));
    text("EXPORT", dx + 10, dy + 9,
         win->active ? RGB(180, 240, 255) : RGB(120, 170, 200), FONT_KALNIA, FALSE);

    int x_pad = 20, y = dy + 44;
    text("FILENAME", dx + x_pad, y - 8, RGB(90, 170, 220), FONT_KALNIA, FALSE);
    y += 12;
    rect(dx + x_pad, y, DAW_DIALOG_W - 40, 22, RGB(5, 8, 18));
    rect(dx + x_pad, y, DAW_DIALOG_W - 40, 1, RGB(0, 200, 255));
    rect(dx + x_pad, y + 21, DAW_DIALOG_W - 40, 1, RGB(0, 60, 120));
    text(state->dialog_input_buffer, dx + x_pad + 6, y + 6,
         win->active ? RGB(200, 245, 255) : RGB(120, 160, 190), FONT_KALNIA, FALSE);
    rect(dx + x_pad + 6 + state->dialog_input_pos * 11, y + 6, 1, 14, RGB(0, 220, 255));

    y += 30;
    text("FORMAT", dx + x_pad, y - 8, RGB(90, 170, 220), FONT_KALNIA, FALSE);
    y += 14;

    int wav_x = dx + x_pad, rys_x = dx + 82;
    BOOL wav_on = (state->export_type == EXPORT_MODE_WAV);
    BOOL rys_on = (state->export_type == EXPORT_MODE_RYS);

    rect(wav_x, y, 52, 20, wav_on ? RGB(0, 70, 140) : RGB(8, 10, 20));
    rect(wav_x, y, 52, 1, RGB(0, 200, 255));
    rect(wav_x, y + 19, 52, 1, RGB(3, 10, 20));
    text("WAV", wav_x + 13, y + 6, wav_on ? RGB(200, 245, 255) : RGB(90, 120, 150), FONT_KALNIA, FALSE);

    rect(rys_x, y, 52, 20, rys_on ? RGB(0, 70, 140) : RGB(8, 10, 20));
    rect(rys_x, y, 52, 1, RGB(0, 200, 255));
    rect(rys_x, y + 19, 52, 1, RGB(3, 10, 20));
    text("RYS", rys_x + 13, y + 6, rys_on ? RGB(200, 245, 255) : RGB(90, 120, 150), FONT_KALNIA, FALSE);

    int btn_y = dy + DAW_DIALOG_H - 30;
    int ex_x = dx + DAW_DIALOG_W - 92;
    int can_x = dx + 20;

    rect(can_x, btn_y, 72, 1, RGB(0, 140, 255));
    rect(can_x, btn_y + 1, 72, 24, RGB(10, 30, 60));
    text("CANCEL", can_x + 8, btn_y + 8,
         win->active ? RGB(180, 230, 255) : RGB(100, 140, 170), FONT_KALNIA, FALSE);

    rect(ex_x, btn_y, 72, 1, RGB(0, 200, 255));
    rect(ex_x, btn_y + 1, 72, 24, RGB(0, 60, 120));
    text("EXPORT", ex_x + 8, btn_y + 8,
         win->active ? RGB(180, 245, 255) : RGB(110, 170, 200), FONT_KALNIA, FALSE);
}

static void export_wav(daw_state_t* state) {
    (void)state;
    prep_dlg_export(get_active_win());
}

static void dd_renderer(int abs_x, int abs_y, int w,
                          const char* label, const char* value_str,
                          BOOL open, BOOL is_br,
                          const char** items, int item_count, int hovered_item) {
    uint32 label_col = is_br ? RGB(180, 130, 230) : RGB(160, 160, 190);
    uint32 border_col = is_br ? RGB(120, 70, 175) : RGB(90, 90, 115);
    uint32 txt_col = is_br ? RGB(230, 200, 255) : RGB(220, 220, 235);

    text(label, abs_x, abs_y - 24, label_col, FONT_KALNIA, FALSE);

    rect(abs_x, abs_y, w, 1,
         is_br ? RGB(190, 150, 240) : RGB(180, 180, 210));
    rect(abs_x, abs_y + 1, w, DROPDOWN_H / 2,
         is_br ? RGB(90, 55, 130) : RGB(65, 65, 85));
    rect(abs_x, abs_y + 1 + DROPDOWN_H/2, w, DROPDOWN_H / 2,
         is_br ? RGB(68, 40, 105) : RGB(48, 48, 64));
    rect(abs_x, abs_y + DROPDOWN_H, w, 1, RGB(10, 10, 18));
    rect(abs_x, abs_y + 1, 1, DROPDOWN_H - 1, border_col);
    rect(abs_x + w - 1, abs_y + 1, 1, DROPDOWN_H - 1, border_col);
    text(value_str, abs_x + 5, abs_y + 2, txt_col, FONT_KALNIA, TRUE);

    int ax = abs_x + w - 11, ay = abs_y + (DROPDOWN_H / 2) - 1;
    uint32 arr_col = is_br ? RGB(200, 160, 240) : RGB(180, 180, 200);
    if (!open) {
        rect(ax, ay, 5, 1, arr_col);
        rect(ax + 1, ay + 1, 3, 1, arr_col);
        rect(ax + 2, ay + 2, 1, 1, arr_col);
    } else {
        rect(ax + 2, ay, 1, 1, arr_col);
        rect(ax + 1, ay + 1, 3, 1, arr_col);
        rect(ax, ay + 2, 5, 1, arr_col);
    }

    if (open && items && item_count > 0) {
        int list_y = abs_y + DROPDOWN_H + 1;
        int total_list_h = item_count * DROPDOWN_ITEM_H + 2;
        uint32 list_bg = is_br ? RGB(55, 30, 90) : RGB(40, 40, 55);
        uint32 list_bdr = is_br ? RGB(120, 70, 175) : RGB(90, 90, 115);

        rect(abs_x, list_y, w, total_list_h, list_bg);
        rect(abs_x, list_y, w, 1, list_bdr);
        rect(abs_x, list_y + total_list_h - 1, w, 1, RGB(10, 10, 18));
        rect(abs_x, list_y, 1, total_list_h, list_bdr);
        rect(abs_x + w - 1, list_y, 1, total_list_h, list_bdr);

        for (int i = 0; i < item_count; i++) {
            int iy = list_y + 1 + i * DROPDOWN_ITEM_H;
            if (i == hovered_item)
                rect(abs_x + 1, iy, w - 2, DROPDOWN_ITEM_H,
                     is_br ? RGB(100, 55, 155) : RGB(75, 75, 100));
            if (i > 0)
                rect(abs_x + 2, iy, w - 4, 1,
                     is_br ? RGB(90, 50, 130) : RGB(60, 60, 80));
            text(items[i], abs_x + 6, iy,
                 is_br ? RGB(220, 190, 250) : RGB(210, 210, 230),
                 FONT_KALNIA, FALSE);
        }
    }
}

static float br_param_get_float(const br_params_t* br, int row) {
    switch (row) {
        case BR_PARAM_VOWEL: return (float)br->vowel;
        case BR_PARAM_VOLUME: return (float)br->volume;
        case BR_PARAM_OQ_START: return br->oq_start;
        case BR_PARAM_OQ_TARGET: return br->oq_target;
        case BR_PARAM_VIB_RATE: return br->vib_rate_hz;
        case BR_PARAM_VIB_DEPTH: return br->vib_depth_pct;
        case BR_PARAM_VIB_ONSET: return (float)br->vib_onset_ms;
        case BR_PARAM_BREATH: return (float)br->breath_amt;
        case BR_PARAM_SHIMMER: return (float)br->shimmer_amt;
        default: return 0.0f;
    }
}

static void br_param_range(int row, float* out_min, float* out_max) {
    switch (row) {
        case BR_PARAM_VOWEL: *out_min = 0; *out_max = (float)(VOWEL_COUNT - 1); break;
        case BR_PARAM_VOLUME: *out_min = 0; *out_max = 100; break;
        case BR_PARAM_OQ_START: *out_min = 0.5f; *out_max = 1.0f; break;
        case BR_PARAM_OQ_TARGET: *out_min = 0.4f; *out_max = 0.9f; break;
        case BR_PARAM_VIB_RATE: *out_min = 3.0f; *out_max = 9.0f; break;
        case BR_PARAM_VIB_DEPTH: *out_min = 0.0f; *out_max = 5.0f; break;
        case BR_PARAM_VIB_ONSET: *out_min = 0; *out_max = 600; break;
        case BR_PARAM_BREATH: *out_min = 0; *out_max = 100; break;
        case BR_PARAM_SHIMMER: *out_min = 0; *out_max = 20; break;
        default: *out_min = 0; *out_max = 1; break;
    }
}

static void br_param_set(br_params_t* br, int row, float val) {
    float mn, mx; br_param_range(row, &mn, &mx);
    if (val < mn) val = mn; if (val > mx) val = mx;
    switch (row) {
        case BR_PARAM_VOWEL: br->vowel = (vowel_t)(int)(val + 0.5f); break;
        case BR_PARAM_VOLUME: br->volume = (uint8)(int)(val + 0.5f); break;
        case BR_PARAM_OQ_START: br->oq_start = val; break;
        case BR_PARAM_OQ_TARGET: br->oq_target = val; break;
        case BR_PARAM_VIB_RATE: br->vib_rate_hz = val; break;
        case BR_PARAM_VIB_DEPTH: br->vib_depth_pct = val; break;
        case BR_PARAM_VIB_ONSET: br->vib_onset_ms = (uint32)(int)(val + 0.5f); break;
        case BR_PARAM_BREATH: br->breath_amt = (uint8)(int)(val + 0.5f); break;
        case BR_PARAM_SHIMMER: br->shimmer_amt = (uint8)(int)(val + 0.5f); break;
    }
}

static const char* VOWELS[VOWEL_COUNT] = { "A", "E", "I", "O", "U" };

static void BRplgn_renderer(Window* win, br_panel_state_t* panel, br_params_t* br) {
    if (!panel->open) return;

    int px = win->x + panel->x, py = win->y + panel->y;
    int pw = BR_PANEL_W, ph = BR_PANEL_H;
    int handle_h = 10;

    rect(px, py, pw, 1, RGB(200, 160, 250));
    rect(px, py + ph, pw, 1, RGB(10, 5, 20));
    rect(px, py, 1, ph, RGB(180, 130, 230));
    rect(px + pw, py, 1, ph + 1, RGB(30, 15, 55));
    rect(px + 1, py + 1, pw - 1, ph - 1, RGB(38, 22, 65));

    rect(px + 1, py + 1, pw - 1, handle_h / 2, RGB(105, 60, 160));
    rect(px + 1, py + 1 + handle_h/2, pw - 1, handle_h / 2, RGB(82, 45, 128));
    for (int g = 0; g < 5; g++) { rect(px + pw/2 - 8 + g * 4, py + 4, 2, 2, RGB(160, 110, 210)); }

    text("x", px + pw - 10, py - 5, RGB(200, 150, 240), FONT_KALNIA, TRUE);

    rect(px + 1, py + 1 + handle_h, pw - 1, 1, RGB(100, 55, 150));

    int row_y_base = py + 1 + handle_h + 4;
    int bar_w = pw - 20;

    for (int row = 0; row < BR_PARAM_LEN; row++) {
        int ry = row_y_base + row * BR_ROW_H;
        BOOL hovered = (row == panel->hovered_row);

        if (hovered) rect(px + 1, ry, pw - 1, BR_ROW_H - 1, RGB(55, 30, 90));

        text(BR_PARAM_LABELS[row], px + 4, ry + 2,
             hovered ? RGB(220, 185, 255) : RGB(160, 120, 210), FONT_KALNIA, FALSE);

        float mn, mx, cur;
        br_param_range(row, &mn, &mx);
        cur = br_param_get_float(br, row);
        int fill_px = (int)(((mx > mn) ? (cur - mn) / (mx - mn) : 0.0f) * (float)bar_w);

        int bar_x = px + 4, bar_y = ry + BR_ROW_H - 6;
        rect(bar_x, bar_y, bar_w, 3, RGB(25, 14, 45));
        if (fill_px > 0)
            rect(bar_x, bar_y, fill_px, 3,
                 hovered ? RGB(175, 100, 255) : RGB(140, 75, 210));
        rect(bar_x + fill_px - 1, bar_y - 1, 3, 5, RGB(220, 180, 255));

        char val_str[12];
        if (row == BR_PARAM_VOWEL) {
            sprintf(val_str, "%s", VOWELS[(int)cur]);
        } else if (row == BR_PARAM_OQ_START || row == BR_PARAM_OQ_TARGET || row == BR_PARAM_VIB_DEPTH) {
            int whole = (int)cur, frac2 = (int)((cur - (float)whole) * 100.0f + 0.5f);
            sprintf(val_str, "%d.%d", whole, frac2);
        } else {
            sprintf(val_str, "%d", (int)(cur + 0.5f));
        }

        text(val_str, px + pw - 38 - ((int)strlen(val_str) - 3) * 11, ry + 2,
             RGB(240, 210, 255), FONT_KALNIA, FALSE);

        if (row < BR_PARAM_LEN - 1)
            rect(px + 1, ry + BR_ROW_H - 1, pw - 1, 1, RGB(55, 30, 85));
    }
}

static BOOL dd_hittest(int px, int py, int abs_x, int abs_y, int w,
                        BOOL open, int item_count) {
    if (px < abs_x || px >= abs_x + w) return FALSE;
    int total_h = DROPDOWN_H + 1 + (open ? (item_count * DROPDOWN_ITEM_H + 2) : 0);
    return (py >= abs_y && py < abs_y + total_h);
}

static int dd_list_item(int px, int py, int abs_x, int abs_y, int w,
                         BOOL open, int item_count) {
    if (!open) return -1;
    if (px < abs_x || px >= abs_x + w) return -1;
    int list_y = abs_y + DROPDOWN_H + 1;
    if (py < list_y) return -1;
    int item = (py - list_y) / DROPDOWN_ITEM_H;
    return (item < 0 || item >= item_count) ? -1 : item;
}

static const char* NOTES[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

static void clp_renderer(struct Window* win) {
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    daw_state_t* state = (daw_state_t*)p->data;

    daw_layout_t L; init_layout(win, &L);

    rect(L.cx, L.cy, L.cw, L.ch, RGB(30, 30, 35));

    rect_grad(L.cx, L.cy, L.cw, MENU_BAR_HEIGHT, RGB(214, 6, 96), RGB(140, 2, 62));
    rect(L.cx, L.cy + MENU_BAR_HEIGHT - 1, L.cw, 1, RGB(255, 80, 155));

    button_v(L.cx + 2, L.cy + 2, 30, 30, state->reset_btn_state, 248, 240, 218);
    icn_reset(L.cx + 10, L.cy + 11, 12, RGB(80, 20, 48));
    button_v(L.cx + 38, L.cy + 2, 30, 30, state->play_btn_state, 248, 240, 218);
    icn_play(L.cx + 48, L.cy + 10, 12,
                   state->is_playing ? RGB(214, 6, 96) : RGB(80, 20, 48));
    button_v(L.cx + 74, L.cy + 2, 30, 30, state->stop_btn_state, 248, 240, 218);
    icn_stop(L.cx + 83, L.cy + 11, 12, RGB(80, 20, 48));
    button_v(L.cx + 110, L.cy + 2, 30, 30, state->export_btn_state, 248, 240, 218);
    icn_export(L.cx + 119, L.cy + 11, 12, RGB(80, 20, 48));

    int tabs_y_start = L.cy + MENU_BAR_HEIGHT + 2;
    for (int t = 0; t < DAW_TRACKS; t++) {
        int tab_y = tabs_y_start + t * 40;
        BOOL active = (state->active_track == t);
        rect(L.cx, tab_y, TABS_WIDTH, 38,
             active ? RGB(214, 6, 96) : RGB(100, 2, 45));
        char tab_label[2] = { '1' + t, '\0' };
        text(tab_label, L.cx + 20, tab_y + 12,
             active ? RGB(255, 240, 218) : RGB(255, 140, 185), FONT_KALNIA, FALSE);
    }

    int scroll_x_px = state->scroll_step * STEP_WIDTH;
    for (int n = 0; n < DAW_NOTES; n++) {
        int ny = L.pr_y + (DAW_NOTES - 1 - n) * state->note_height;
        int noi = n % 12;
        BOOL is_black = (noi == 1 || noi == 3 || noi == 6 || noi == 8 || noi == 10);
        rect(L.keys_x, ny, KEYS_WIDTH, state->note_height,
             is_black ? RGB(28, 28, 33) : RGB(210, 210, 220));
        rect(L.keys_x, ny, KEYS_WIDTH, 1, RGB(90, 90, 100));
        rect(L.keys_x + KEYS_WIDTH - 1, ny, 1, state->note_height, RGB(70, 70, 80));

        char label[4];
        uint32 label_color;
        if (noi == 0) {
            label[0] = 'C'; label[1] = '0' + (n / 12 + 4); label[2] = '\0';
            label_color = RGB(40, 40, 50);
        } else {
            label[0] = NOTES[noi][0]; label[1] = NOTES[noi][1]; label[2] = '\0';
            label_color = is_black ? RGB(160, 160, 170) : RGB(90, 90, 105);
        }
        text(label, L.keys_x + 2, ny + 2, label_color, FONT_FUZZY, FALSE);
    }

    for (int n = 0; n < DAW_NOTES; n++) {
        int ny = L.pr_y + (DAW_NOTES - 1 - n) * state->note_height;
        int noi = n % 12;
        BOOL is_black = (noi == 1 || noi == 3 || noi == 6 || noi == 8 || noi == 10);
        rect(L.pr_x, ny, L.pr_w_vis, state->note_height,
             is_black ? RGB(25, 25, 30) : RGB(35, 35, 40));
        rect(L.pr_x, ny, L.pr_w_vis, 1, RGB(45, 45, 50));
    }
    for (int s = state->scroll_step; s <= DAW_STEPS; s++) {
        int sx = L.pr_x + (s * STEP_WIDTH) - scroll_x_px;
        if (sx < L.pr_x) continue; if (sx >= L.drawer_x) break;
        rect(sx, L.pr_y, 1, state->pr_h,
             (s % 4 == 0) ? RGB(80, 80, 90) : RGB(50, 50, 60));
    }

    daw_track_t* tr = &state->tracks[state->active_track];
    for (int i = 0; i < MAX_NOTES_PER_TRACK; i++) {
        if (!tr->notes[i].active) continue;
        int nx = L.pr_x + tr->notes[i].start_step * STEP_WIDTH - scroll_x_px;
        int ny = L.pr_y + (DAW_NOTES - 1 - tr->notes[i].note_index) * state->note_height;
        int nw = tr->notes[i].length_steps * STEP_WIDTH;
        if (nx + nw <= L.pr_x || nx >= L.drawer_x) continue;

        int draw_x = nx < L.pr_x ? L.pr_x : nx;
        int draw_w = nw - (draw_x - nx);
        if (draw_x + draw_w > L.drawer_x) draw_w = L.drawer_x - draw_x;
        if (draw_w > 0) {
            rect(draw_x + 1, ny + 1, draw_w - 2, state->note_height - 2, RGB(214, 6, 96));
            if (nx + nw <= L.drawer_x && nx + nw > L.pr_x)
                rect(nx + nw - 4, ny + 1, 3, state->note_height - 2, RGB(255, 80, 155));
        }
    }

    for (int s = state->scroll_step; s <= DAW_STEPS; s++) {
        int sx = L.pr_x + (s * STEP_WIDTH) - scroll_x_px;
        if (sx < L.pr_x) continue; if (sx >= L.drawer_x) break;
        if (s % 4 == 0) {
            char measure[8]; sprintf(measure, "%d", s/16 + 1);
            text(measure, sx + 2, L.pr_y_base + 2, RGB(150, 150, 160), FONT_FUZZY, FALSE);
        }
    }

    if (state->is_playing) {
        uint32 elapsed_ms = get_tick_ms() - state->play_start_tick;
        if (elapsed_ms < state->play_total_ms) {
            float fraction = (float)elapsed_ms / (float)state->play_total_ms;
            state->playhead_step = (float)state->play_start_step
                                 + fraction * (float)(state->play_total_ms / 250);
            if (state->playhead_step > (float)DAW_STEPS)
                state->playhead_step = (float)DAW_STEPS;
            is_dirty(TRUE);
        } else {
            uint32 cur = 0, tot = 0;
            if (!ac97_get_voice_progress(state->play_handle, &cur, &tot)) {
                state->is_playing = FALSE; state->play_handle = -1; state->play_sentinel_buf = NULL;
            }
            is_dirty(TRUE);
        }
    }

    int ph_x = L.pr_x + (int)(state->playhead_step * (float)STEP_WIDTH) - scroll_x_px;
    if (ph_x >= L.pr_x && ph_x < L.drawer_x) {
        rect(ph_x, L.pr_y, 1, state->pr_h, RGB(255, 60, 60));
        for (int row = 0; row < PLAYHEAD_HEADER_H - 1; row++)
            rect(ph_x - row, L.pr_y_base + row, row * 2 + 1, 1, RGB(255, 80, 80));
    }

    int sb_y = L.pr_y + state->pr_h + 2;
    int total_w = DAW_STEPS * STEP_WIDTH;
    int thumb_w = (L.pr_w_vis * L.pr_w_vis) / total_w;
    if (thumb_w < 20) thumb_w = 20;
    int thumb_x = L.pr_x + (scroll_x_px * (L.pr_w_vis - thumb_w)) / (total_w - L.pr_w_vis);
    rect(L.pr_x, sb_y, L.pr_w_vis, SCROLLBAR_H, RGB(40, 40, 45));
    rect(thumb_x, sb_y + 2, thumb_w, SCROLLBAR_H - 4, RGB(100, 100, 110));
    rect(thumb_x, sb_y + 2, thumb_w, 1, RGB(140, 140, 150));

    rect(L.dr_x, L.drawer_ys, DRAWER_WIDTH, L.drawer_h, RGB(58, 8, 30));
    rect(L.dr_x, L.drawer_ys, DRAWER_WIDTH, 1, RGB(255, 80, 155));
    rect(L.dr_x, L.drawer_ys + 1, 1, L.drawer_h - 1, RGB(130, 2, 56));
    text("options", L.dr_x + 8, L.drawer_ys + 8, RGB(255, 215, 235), FONT_KALNIA, TRUE);

    track_sound_t* ts = &state->sound[state->active_track];
    const char* preset_label = (ts->selected_preset >= 0 && ts->selected_preset < 2)
                               ? BR_PRESET_NAMES[ts->selected_preset] : "select...";
    dd_renderer(L.dd_abs_x, L.preset_dd_abs_y, L.dd_w, "PRESET", preset_label,
                  ts->preset_dd.open, TRUE, BR_PRESET_NAMES, 2, ts->preset_dd.hovered_item);

    
    int btn_y = L.edit_btn_y, btn_h = 24;
    rect(L.dd_abs_x, btn_y, L.dd_w, 1, RGB(180, 130, 230));
    rect(L.dd_abs_x, btn_y + 1, L.dd_w, btn_h/2, RGB(88, 50, 135));
    rect(L.dd_abs_x, btn_y + 1 + btn_h/2, L.dd_w, btn_h/2,
         ts->br_panel.open ? RGB(55, 28, 92) : RGB(68, 38, 108));
    rect(L.dd_abs_x, btn_y + btn_h, L.dd_w, 1, RGB(10, 5, 20));
    rect(L.dd_abs_x, btn_y + 1, 1, btn_h - 1, RGB(110, 62, 168));
    rect(L.dd_abs_x + L.dd_w - 1, btn_y + 1, 1, btn_h - 1, RGB(38, 18, 70));
    text(ts->br_panel.open ? "CLOSE" : "MODIFY", L.dd_abs_x + 6, btn_y + 7,
         ts->br_panel.open ? RGB(210, 175, 250) : RGB(230, 200, 255),
        FONT_KALNIA, FALSE);


    int y = L.clr_btn_y;
    rect(L.dd_abs_x, y, L.dd_w, 1, RGB(180, 100, 100));
    rect(L.dd_abs_x, y + 1, L.dd_w, 12, RGB(110, 55, 55));
    rect(L.dd_abs_x, y + 13, L.dd_w, 10, RGB(90, 42, 42));
    rect(L.dd_abs_x, y + 23, L.dd_w, 1, RGB(10, 5, 5));
    rect(L.dd_abs_x, y + 1, 1, 22, RGB(140, 70, 70));
    rect(L.dd_abs_x + L.dd_w - 1, y + 1, 1, 22, RGB(50, 22, 22));
    text("CLEAR", L.dd_abs_x + 4, y + 8, RGB(255, 200, 200), FONT_KALNIA, FALSE);


    BRplgn_renderer(win, &ts->br_panel, &ts->br);

    if (state->dialog_mode != EXPORT_MODE_NONE) dlg_renderer(win, state);
}

static void mdown(struct Window* win, int x, int y) {
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    daw_state_t* state = (daw_state_t*)p->data;
    track_sound_t* ts = &state->sound[state->active_track];

    if (state->dialog_mode != EXPORT_MODE_NONE) {
        int dx = (win->width - DAW_DIALOG_W) / 2;
        int dy = (win->height - DAW_DIALOG_H) / 2 - 26;
        int btn_top = dy + DAW_DIALOG_H - 30, btn_bot = dy + DAW_DIALOG_H - 5;

        if (x >= dx + 20 && x <= dx + 92 && y >= btn_top && y <= btn_bot) {
            state->dialog_mode = EXPORT_MODE_NONE; win->on_key_press = on_keypress;
        } else if (x >= dx + DAW_DIALOG_W - 92 && x <= dx + DAW_DIALOG_W - 20 && y >= btn_top && y <= btn_bot) {
            export(state); state->dialog_mode = EXPORT_MODE_NONE; win->on_key_press = on_keypress;
        } else if (x >= dx + 20 && x <= dx + 72 && y >= dy + 100 && y <= dy + 120) {
            state->export_type = EXPORT_MODE_WAV;
        } else if (x >= dx + 82 && x <= dx + 134 && y >= dy + 100 && y <= dy + 120) {
            state->export_type = EXPORT_MODE_RYS;
        }
        is_dirty(TRUE); return;
    }

    if (y >= 2 && y <= 32) {
        if (x >= 6 && x <= 36) { state->reset_btn_state = BUTTON_STATE_CLICKED; reset_playhead(state); }
        else if (x >= 42 && x <= 72) { state->play_btn_state = BUTTON_STATE_CLICKED; start_playback(state); }
        else if (x >= 78 && x <= 108) { state->stop_btn_state = BUTTON_STATE_CLICKED; stop_playback(state); }
        else if (x >= 114 && x <= 144) { state->export_btn_state = BUTTON_STATE_CLICKED; export_wav(state); }
        is_dirty(TRUE); return;
    }

    daw_layout_t L; init_layout(win, &L);
    int scr_x = win->x + 4 + x, scr_y = win->y + 24 + y;

    if (ts->br_panel.open) {
        br_panel_state_t* bp = &ts->br_panel;
        int px = win->x + bp->x, py = win->y + bp->y, handle_h = 10;

        if (scr_x >= px && scr_x < px + BR_PANEL_W &&
            scr_y >= py && scr_y < py + BR_PANEL_H) {

            if (scr_x >= px + BR_PANEL_W - 12 && scr_y < py + 1 + handle_h) {
                bp->open = FALSE;
            } else if (scr_y < py + 1 + handle_h) {
                bp->dragging = TRUE;
                bp->drag_off_x = scr_x - px;
                bp->drag_off_y = scr_y - py;
            } else {
                int row = (scr_y - (py + 1 + handle_h + 4)) / BR_ROW_H;
                if (row >= 0 && row < BR_PARAM_LEN) {
                    int bar_w = BR_PANEL_W - 20;
                    float frac = (float)(scr_x - (px + 4)) / (float)bar_w;
                    if (frac < 0.0f) frac = 0.0f; if (frac > 1.0f) frac = 1.0f;
                    float mn, mx; br_param_range(row, &mn, &mx);
                    br_param_set(&ts->br, row, mn + frac * (mx - mn));
                    free_trkcache(state);
                    bp->editing_row = row;
                    bp->edit_start_x = scr_x;
                    bp->edit_start_val = br_param_get_float(&ts->br, row);
                }
            }
            is_dirty(TRUE); return;
        }
    }

    if (scr_x >= L.drawer_x) {
        if (dd_hittest(scr_x, scr_y, L.dd_abs_x, L.preset_dd_abs_y, L.dd_w, ts->preset_dd.open, 2)) {
            int item = dd_list_item(scr_x, scr_y, L.dd_abs_x, L.preset_dd_abs_y, L.dd_w, ts->preset_dd.open, 2);
            if (item >= 0 && item < 2) {
                apply_preset_BR(&ts->br, item);
                ts->selected_preset = item;
                free_trkcache(state);
                ts->preset_dd.open = FALSE;
            } else if (scr_x >= L.dd_abs_x && scr_x < L.dd_abs_x + L.dd_w
                    && scr_y >= L.preset_dd_abs_y && scr_y < L.preset_dd_abs_y + DROPDOWN_H + 1) {
                ts->preset_dd.open = !ts->preset_dd.open;
            }
            is_dirty(TRUE); return;
        }

        if (scr_x >= L.dd_abs_x && scr_x < L.dd_abs_x + L.dd_w
        && scr_y >= L.edit_btn_y && scr_y < L.edit_btn_y + 25) {
            ts->br_panel.open = !ts->br_panel.open;
            if (ts->br_panel.open) {
                int spawn_x = win->width - DRAWER_WIDTH - BR_PANEL_W - 40;
                int spawn_y = L.edit_btn_y - win->y - 72;
                if (spawn_x < 4) spawn_x = 4;
                if (spawn_y + BR_PANEL_H > win->height) spawn_y = win->height - BR_PANEL_H;
                ts->br_panel.x = spawn_x;
                ts->br_panel.y = spawn_y;
            }
            is_dirty(TRUE); return;
        }

        if (scr_x >= L.dd_abs_x && scr_x < L.dd_abs_x + L.dd_w
        && scr_y >= L.clr_btn_y && scr_y < L.clr_btn_y + 24) {
            memset(state->tracks[state->active_track].notes, 0,
                   sizeof(state->tracks[state->active_track].notes));
            is_dirty(TRUE);
        }
        return;
    }

    if (x >= 0 && x <= TABS_WIDTH) {
        int t = (y - MENU_BAR_HEIGHT - 2) / 40;
        if (t >= 0 && t < DAW_TRACKS) {
            state->sound[state->active_track].profile_dd.open = FALSE;
            state->sound[state->active_track].wave_dd.open = FALSE;
            state->sound[state->active_track].preset_dd.open = FALSE;
            state->active_track = t;
            is_dirty(TRUE);
        }
        return;
    }

    int pr_x_rel = TABS_WIDTH + 5 + KEYS_WIDTH;
    int pr_y_base_rel = MENU_BAR_HEIGHT + 5;
    int dr_x_rel = (win->width - 8) - DRAWER_WIDTH;
    if (x >= pr_x_rel && x < dr_x_rel && y >= pr_y_base_rel && y < pr_y_base_rel + PLAYHEAD_HEADER_H) {
        state->is_dragging_ph = TRUE;
        float ns = (float)(x - pr_x_rel + state->scroll_step * STEP_WIDTH) / (float)STEP_WIDTH;
        if (ns < 0.0f) ns = 0.0f; if (ns > (float)DAW_STEPS) ns = (float)DAW_STEPS;
        state->playhead_step = ns;
        is_dirty(TRUE); return;
    }

    int sb_w = dr_x_rel - pr_x_rel;
    int total_w = DAW_STEPS * STEP_WIDTH;
    if (total_w > sb_w) {
        int pr_y_rel = pr_y_base_rel + PLAYHEAD_HEADER_H;
        int sb_y = pr_y_rel + state->pr_h + 2;
        if (x >= pr_x_rel && x <= pr_x_rel + sb_w && y >= sb_y && y < sb_y + SCROLLBAR_H) {
            state->is_dragging_scroll = TRUE;
            is_dirty(TRUE); return;
        }
    }
    
    int pr_y_rel = pr_y_base_rel + PLAYHEAD_HEADER_H;
    int scroll_x_px = state->scroll_step * STEP_WIDTH;
    if (x >= pr_x_rel && x < dr_x_rel && y >= pr_y_rel && y <= pr_y_rel + state->pr_h) {
        int s = (x - pr_x_rel + scroll_x_px) / STEP_WIDTH;
        int n = DAW_NOTES - 1 - (y - pr_y_rel) / state->note_height;
        daw_track_t* track = &state->tracks[state->active_track];
        state->drag_note_idx = -1;

        for (int i = 0; i < MAX_NOTES_PER_TRACK; i++) {
            if (!track->notes[i].active) continue;
            if (n == track->notes[i].note_index &&
                s >= track->notes[i].start_step &&
                s < track->notes[i].start_step + track->notes[i].length_steps) {
                state->drag_note_idx = i;
                int note_end_x = pr_x_rel + (track->notes[i].start_step +
                                                track->notes[i].length_steps) * STEP_WIDTH - scroll_x_px;
                if (x >= note_end_x - 10) {
                    state->is_resizing = TRUE;
                } else {
                    state->is_resizing = FALSE;
                    state->drag_offset_step = s - track->notes[i].start_step;
                    state->drag_offset_note = n - track->notes[i].note_index;
                    state->last_preview_note = track->notes[i].note_index;
                }
                return;
            }
        }
        for (int i = 0; i < MAX_NOTES_PER_TRACK; i++) {
            if (!track->notes[i].active) {
                track->notes[i] = (daw_note_t){
                    .active = TRUE, .note_index = n, .start_step = s, .length_steps = 1,
                };
                state->drag_note_idx = i;
                state->is_resizing = TRUE;
                state->last_preview_note = n;
                note_preview(state, n);
                is_dirty(TRUE); return;
            }
        }
    }
}

static void m_move(struct Window* win, int x, int y) {
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    daw_state_t* state = (daw_state_t*)p->data;
    track_sound_t* ts = &state->sound[state->active_track];
    BOOL dirty = FALSE;

    BUTTON_STATE old_r = state->reset_btn_state, old_pl = state->play_btn_state,
                 old_s = state->stop_btn_state, old_ex = state->export_btn_state;
    state->reset_btn_state = state->play_btn_state =
    state->stop_btn_state = state->export_btn_state = BUTTON_STATE_NORMAL;
    if (y >= 2 && y <= 32) {
        if (x >= 6 && x <= 36) state->reset_btn_state = BUTTON_STATE_HOVER;
        else if (x >= 42 && x <= 72) state->play_btn_state = BUTTON_STATE_HOVER;
        else if (x >= 78 && x <= 108) state->stop_btn_state = BUTTON_STATE_HOVER;
        else if (x >= 114 && x <= 144) state->export_btn_state = BUTTON_STATE_HOVER;
    }
    if (state->reset_btn_state != old_r || state->play_btn_state != old_pl ||
        state->stop_btn_state != old_s || state->export_btn_state != old_ex)
        dirty = TRUE;

    if (ts->profile == PROFILE_BLOODRAVEN) {
        br_panel_state_t* bp = &ts->br_panel;
        int scr_x = win->x + 4 + x, scr_y = win->y + 24 + y;

        if (bp->dragging) {
            int new_x = (scr_x - bp->drag_off_x) - win->x;
            int new_y = (scr_y - bp->drag_off_y) - win->y;
            if (new_x < 4) new_x = 4; if (new_y < 24) new_y = 24;
            if (new_x + BR_PANEL_W > win->width - 5) new_x = win->width - 5 - BR_PANEL_W;
            if (new_y + BR_PANEL_H > win->height - 5) new_y = win->height - 5 - BR_PANEL_H;
            bp->x = new_x; bp->y = new_y;
            dirty = TRUE;
        }

        if (bp->editing_row >= 0) {
            float mn, mx; br_param_range(bp->editing_row, &mn, &mx);
            float new_val = bp->edit_start_val + (float)(scr_x - bp->edit_start_x) * (mx - mn) / 80.0f;
            br_param_set(&ts->br, bp->editing_row, new_val);
            ts->selected_preset = eval_preset_BR(&ts->br);
            free_trkcache(state);
            dirty = TRUE;
        }
        
        int px = win->x + bp->x, py = win->y + bp->y, handle_h = 10;
        if (bp->open && scr_x >= px && scr_x < px + BR_PANEL_W &&
            scr_y >= py + 1 + handle_h && scr_y < py + BR_PANEL_H) {
            int row = (scr_y - (py + 1 + handle_h + 4)) / BR_ROW_H;
            if (row >= 0 && row < BR_PARAM_LEN && row != bp->hovered_row) {
                bp->hovered_row = row; dirty = TRUE;
            }
        } else if (bp->hovered_row != -1) {
            bp->hovered_row = -1; dirty = TRUE;
        }
    }

    daw_layout_t L; init_layout(win, &L);
    int scr_x = win->x + 4 + x, scr_y = win->y + 24 + y;
    if (ts->preset_dd.open) {
        int item = dd_list_item(scr_x, scr_y, L.dd_abs_x, L.preset_dd_abs_y, L.dd_w, TRUE, 2);
        if (item != ts->preset_dd.hovered_item) { ts->preset_dd.hovered_item = item; dirty = TRUE; }
    }

    // scrollbar drag
    if (state->is_dragging_scroll) {
        int pr_x = TABS_WIDTH + 5 + KEYS_WIDTH;
        int sb_w = (win->width - 8) - DRAWER_WIDTH - pr_x;
        int total_w = DAW_STEPS * STEP_WIDTH;
        int thumb_w = (sb_w * sb_w) / total_w; if (thumb_w < 20) thumb_w = 20;
        int scroll_range = total_w - sb_w;
        if (scroll_range > 0) {
            int thumb_range = sb_w - thumb_w;
            state->scroll_step = ((x - pr_x - thumb_w / 2) * scroll_range) / thumb_range / STEP_WIDTH;
            if (state->scroll_step < 0) state->scroll_step = 0;
            if (state->scroll_step > DAW_STEPS - (sb_w / STEP_WIDTH))
                state->scroll_step = DAW_STEPS - (sb_w / STEP_WIDTH);
            is_dirty(TRUE);
        }
        return;
    }

    // playhead drag
    if (state->is_dragging_ph) {
        int pr_x = TABS_WIDTH + 5 + KEYS_WIDTH;
        float ns = (float)(x - pr_x + state->scroll_step * STEP_WIDTH) / (float)STEP_WIDTH;
        if (ns < 0.0f) ns = 0.0f; if (ns > (float)DAW_STEPS) ns = (float)DAW_STEPS;
        state->playhead_step = ns;
        is_dirty(TRUE); return;
    }

    // drga & resize
    if (state->drag_note_idx != -1) {
        int pr_x = TABS_WIDTH + 5 + KEYS_WIDTH;
        int pr_y = MENU_BAR_HEIGHT + 5 + PLAYHEAD_HEADER_H;
        int scroll_x_px = state->scroll_step * STEP_WIDTH;
        int s = (x - pr_x + scroll_x_px) / STEP_WIDTH;
        int n = DAW_NOTES - 1 - (y - pr_y) / state->note_height;
        daw_note_t* note = &state->tracks[state->active_track].notes[state->drag_note_idx];

        if (state->is_resizing) {
            int new_len = s - note->start_step + 1;
            if (new_len < 1) new_len = 1;
            if (note->start_step + new_len > DAW_STEPS) new_len = DAW_STEPS - note->start_step;
            note->length_steps = new_len;
        } else {
            int new_start = s - state->drag_offset_step;
            int new_note = n - state->drag_offset_note;
            if (new_start < 0) new_start = 0;
            if (new_start + note->length_steps > DAW_STEPS) new_start = DAW_STEPS - note->length_steps;
            if (new_note < 0) new_note = 0;
            if (new_note >= DAW_NOTES) new_note = DAW_NOTES - 1;
            note->start_step = new_start;
            if (new_note != note->note_index) {
                note->note_index = new_note;
                if (new_note != state->last_preview_note) {
                    state->last_preview_note = new_note;
                    note_preview(state, new_note);
                }
            }
        }
        dirty = TRUE;
    }

    if (dirty) is_dirty(TRUE);
}

static void m_up(struct Window* win, int x, int y) {
    (void)x; (void)y;
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    daw_state_t* state = (daw_state_t*)p->data;
    track_sound_t* ts = &state->sound[state->active_track];

    state->reset_btn_state = state->play_btn_state =
    state->stop_btn_state = state->export_btn_state = BUTTON_STATE_NORMAL;

    ts->br_panel.dragging = FALSE;
    ts->br_panel.editing_row = -1;
    ts->br_panel.edit_start_x = 0;

    if (state->drag_note_idx >= 0 && state->is_resizing) {
        daw_note_t* note = &state->tracks[state->active_track].notes[state->drag_note_idx];
        if (note->active) get_note_buf(state, note->note_index, note->length_steps);
    }

    state->drag_note_idx = -1;
    state->is_dragging_ph = FALSE;
    state->is_dragging_scroll = FALSE;
    is_dirty(TRUE);
}

static void on_close(struct Window* win) {
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    daw_state_t* state = (daw_state_t*)p->data;
    if (state->is_playing) { ac97_stop_all(); state->play_sentinel_buf = NULL; }
    else if (state->play_sentinel_buf) { kfree(state->play_sentinel_buf); }
    for (int n = 0; n < DAW_NOTES; n++)
        for (int l = 0; l < DAW_STEPS; l++)
            if (state->pregen_notes[n][l]) {
                ac97_synth_free(state->pregen_notes[n][l]);
                state->pregen_notes[n][l] = NULL;
            }
    kfree(state);
    p->data = NULL;
}

void launch_cls(const char* filename) {
    Process* p = create_process("CL Studio");
    if (!p) return;

    daw_state_t* state = (daw_state_t*)kmalloc(sizeof(daw_state_t));
    memset(state, 0, sizeof(daw_state_t));
    state->active_track = 0;
    state->drag_note_idx = -1;
    state->last_preview_note = -1;
    state->playhead_step = 0.0f;
    state->is_dragging_ph = FALSE;
    state->play_handle = -1;
    state->play_sentinel_buf = NULL;
    state->dialog_mode = EXPORT_MODE_NONE;
    p->data = state;

    for (int t = 0; t < DAW_TRACKS; t++)
        sprintf(state->tracks[t].name, "TR %d", t + 1);

    uint8 r = 184, g = 4, b = 80;
    Window* win = window_r(p->pid, "CL Studio", -1, -1, 760, 420, r, g, b);
    if (!win) { cleanup_process(p->pid); return; }

    int roll_available_h = (win->height - 28) - (MENU_BAR_HEIGHT + 20 + PLAYHEAD_HEADER_H);
    state->note_height = roll_available_h / DAW_NOTES;
    if (state->note_height < 10) state->note_height = 10;
    state->pr_h = state->note_height * DAW_NOTES;

    int dr_x_local = (win->width - 8) - DRAWER_WIDTH + 10;
    int drawer_y_local = MENU_BAR_HEIGHT;
    for (int t = 0; t < DAW_TRACKS; t++)
        init_track(&state->sound[t], t, win->x, win->y, dr_x_local, drawer_y_local);

    win->content_renderer = clp_renderer;
    win->on_mouse_down = mdown;
    win->on_mouse_move = m_move;
    win->on_mouse_up = m_up;
    win->on_close = on_close;

    if (filename) {
        load_RYS(state, filename);
        const char* last_slash = strrchr(filename, '/');
        strcpy(state->current_filename, last_slash ? last_slash + 1 : filename);
    }
}