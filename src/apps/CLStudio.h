#ifndef CLSTUDIO_H
#define CLSTUDIO_H

#include "types.h"
#include "gui.h"
#include "synth.h"

#define DAW_TRACKS 4
#define DAW_STEPS 512
#define DAW_NOTES 24
#define MAX_NOTES_PER_TRACK 64

typedef enum {
    PROFILE_BLOODRAVEN = 0,
    PROFILE_COUNT = 1,
} sound_profile_t;

typedef struct {
    vowel_t vowel;
    uint8 volume;
    float oq_start;
    float oq_target;
    float vib_rate_hz;
    float vib_depth_pct;
    uint32 vib_onset_ms;
    uint8 breath_amt;
    uint8 shimmer_amt;
} br_params_t;

typedef struct {
    BOOL open;
    int x, y;
    BOOL dragging;
    int drag_off_x, drag_off_y;
    int hovered_row;
    int editing_row;
    int edit_start_x;
    float edit_start_val;
} br_panel_state_t;

typedef struct {
    int x, y, w;
    BOOL open;
    int hovered_item;
} dropdown_state_t;

typedef struct {
    sound_profile_t profile;
    wave_type_t wave;
    br_params_t br;
    dropdown_state_t profile_dd;
    dropdown_state_t wave_dd;
    dropdown_state_t preset_dd;
    int selected_preset;
    br_panel_state_t br_panel;
} track_sound_t;

typedef struct {
    int note_index;
    int start_step;
    int length_steps;
    BOOL active;
} daw_note_t;

typedef struct {
    char name[32];
    daw_note_t notes[MAX_NOTES_PER_TRACK];
} daw_track_t;

typedef enum {
    EXPORT_MODE_NONE,
    EXPORT_MODE_WAV,
    EXPORT_MODE_RYS
} export_mode_t;

typedef struct {
    daw_track_t tracks[DAW_TRACKS];
    track_sound_t sound[DAW_TRACKS];
    int active_track;
    void* pregen_notes[DAW_NOTES][DAW_STEPS];
    BOOL is_playing;

    int drag_note_idx;
    BOOL is_resizing;
    int drag_offset_step;
    int drag_offset_note;
    int last_preview_note;

    BOOL drawer_open;

    float playhead_step;
    BOOL is_dragging_ph;
    uint32 play_start_tick;
    int play_handle;

    sint16* play_sentinel_buf;
    uint32 play_total_ms;
    uint32 play_start_step;

    BUTTON_STATE reset_btn_state;
    BUTTON_STATE play_btn_state;
    BUTTON_STATE stop_btn_state;
    BUTTON_STATE export_btn_state;

    int note_height;
    int pr_h;

    int scroll_step;
    BOOL is_dragging_scroll;

    export_mode_t dialog_mode;
    export_mode_t export_type;
    char dialog_input_buffer[64];
    int dialog_input_pos;
    char current_filename[64];
} daw_state_t;

void launch_cls(const char* filename);

#endif