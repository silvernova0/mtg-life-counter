#include "counters_screen.h"
#include "counter_screen.h"
#include "../ui/theme.h"
#include "../ui/nav_home.h"
#include "../app_state.h"
#include "../input_driver.h"
#include "../life_history.h"
#include <stdio.h>

#define ROW_W        250
#define ROW_H        44
#define ROW_GAP      6
#define LIST_H       220
#define CMDR_OPPONENTS (MAX_PLAYERS - 1)   /* a four-player pod */
#define CMDR_LETHAL  21

typedef enum {
    CTR_CMDR = 0,   /* + opponent index in s_rows[].opp */
    CTR_POISON,
    CTR_ENERGY,
    CTR_EXPERIENCE,
    CTR_STORM,
} counter_kind_t;

typedef struct {
    counter_kind_t kind;
    uint8_t opp;
    lv_obj_t *row;
    lv_obj_t *value;
} counter_row_t;

#define MAX_ROWS (CMDR_OPPONENTS + 4)

static lv_obj_t *s_screen = nullptr;
static lv_obj_t *s_life_lbl = nullptr;
static counter_row_t s_rows[MAX_ROWS];
static uint8_t s_row_count = 0;
static uint8_t s_selected = 0;
static int16_t s_life_on_entry = 0;

static int16_t row_value(const counter_row_t &r) {
    const player_state_t &p = g_game.players[0];
    switch (r.kind) {
        case CTR_CMDR:       return p.commander_damage[r.opp + 1];
        case CTR_POISON:     return p.poison;
        case CTR_ENERGY:     return p.energy;
        case CTR_EXPERIENCE: return p.experience;
        case CTR_STORM:      return g_game.storm_count;
    }
    return 0;
}

static void row_adjust(const counter_row_t &r, int16_t delta) {
    switch (r.kind) {
        case CTR_CMDR:       app_state_adjust_opponent_commander_damage(r.opp, delta); break;
        case CTR_POISON:     app_state_adjust_poison(0, delta); break;
        case CTR_ENERGY:     app_state_adjust_energy(0, delta); break;
        case CTR_EXPERIENCE: app_state_adjust_experience(0, delta); break;
        case CTR_STORM:      app_state_adjust_storm(delta); break;
    }
}

static bool row_lethal(const counter_row_t &r) {
    int16_t v = row_value(r);
    if (r.kind == CTR_CMDR) return v >= CMDR_LETHAL;
    if (r.kind == CTR_POISON) return v >= POISON_DEATH_THRESHOLD;
    return false;
}

static lv_color_t kind_color(counter_kind_t k) {
    switch (k) {
        case CTR_CMDR:       return lv_color_hex(0xff8a3d);
        case CTR_POISON:     return lv_color_hex(0x7bd65a);
        case CTR_ENERGY:     return lv_color_hex(0x4cc4ff);
        case CTR_EXPERIENCE: return lv_color_hex(0xc08bff);
        case CTR_STORM:      return lv_color_hex(0xf2c14e);
    }
    return THEME_LIFE_TEXT;
}

static void refresh() {
    if (!s_screen) return;
    char buf[16];
    for (uint8_t i = 0; i < s_row_count; i++) {
        counter_row_t &r = s_rows[i];
        bool sel = (i == s_selected);
        snprintf(buf, sizeof(buf), "%d", row_value(r));
        lv_label_set_text(r.value, buf);
        lv_obj_set_style_text_color(r.value, row_lethal(r) ? THEME_LIFE_CRITICAL : THEME_LIFE_TEXT, 0);
        lv_obj_set_style_border_width(r.row, sel ? 3 : 1, 0);
        lv_obj_set_style_border_opa(r.row, sel ? LV_OPA_COVER : LV_OPA_30, 0);
        lv_obj_set_style_bg_color(r.row, sel ? theme_bg_top() : THEME_PANEL, 0);
    }
    if (s_life_lbl) {
        snprintf(buf, sizeof(buf), "LIFE %d", g_game.players[0].life);
        lv_label_set_text(s_life_lbl, buf);
    }
}

static void select_row(uint8_t idx) {
    if (idx >= s_row_count) return;
    s_selected = idx;
    refresh();
    lv_obj_scroll_to_view(s_rows[idx].row, LV_ANIM_ON);
}

static bool was_swipe() {
    lv_indev_t *indev = lv_indev_get_act();
    return indev && lv_indev_get_gesture_dir(indev) != LV_DIR_NONE;
}

static void go_back() {
    /* Commander damage moves life; log the net change like a dial turn. */
    int16_t diff = g_game.players[0].life - s_life_on_entry;
    if (diff != 0) life_history_push(diff, g_game.players[0].life);
    counter_screen_create_anim(LV_SCR_LOAD_ANIM_FADE_ON);
    counters_screen_release();
}

static void row_clicked_cb(lv_event_t *e) {
    if (!s_screen || was_swipe()) return;
    select_row((uint8_t)(uintptr_t)lv_event_get_user_data(e));
}

static void row_long_pressed_cb(lv_event_t *e) {
    if (!s_screen) return;
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (idx >= s_row_count) return;
    row_adjust(s_rows[idx], -row_value(s_rows[idx]));   /* zero it */
    select_row(idx);
}

static void screen_gesture_cb(lv_event_t *e) {
    (void)e;
    if (!s_screen) return;
    if (was_swipe()) go_back();
}

static void encoder_handler(input_encoder_event_t evt, int16_t data) {
    if (g_game.current_screen != SCREEN_COUNTERS || !s_screen) return;
    if (evt != INPUT_ENC_TURN || s_row_count == 0) return;
    row_adjust(s_rows[s_selected], data);
    refresh();
}

static void add_row(lv_obj_t *list, counter_kind_t kind, uint8_t opp, const char *name) {
    if (s_row_count >= MAX_ROWS) return;
    uint8_t idx = s_row_count++;
    counter_row_t &r = s_rows[idx];
    r.kind = kind;
    r.opp = opp;

    lv_obj_t *row = lv_obj_create(list);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, ROW_W, ROW_H);
    lv_obj_set_style_radius(row, ROW_H / 2, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(row, kind_color(kind), 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(row, row_clicked_cb, LV_EVENT_SHORT_CLICKED, (void *)(uintptr_t)idx);
    lv_obj_add_event_cb(row, row_long_pressed_cb, LV_EVENT_LONG_PRESSED, (void *)(uintptr_t)idx);
    r.row = row;

    lv_obj_t *name_lbl = lv_label_create(row);
    lv_label_set_text(name_lbl, name);
    lv_obj_set_style_text_font(name_lbl, &font_bold_24, 0);
    lv_obj_set_style_text_color(name_lbl, kind_color(kind), 0);
    lv_obj_align(name_lbl, LV_ALIGN_LEFT_MID, 20, 0);

    lv_obj_t *val = lv_label_create(row);
    lv_obj_set_style_text_font(val, &font_bold_32, 0);
    lv_obj_align(val, LV_ALIGN_RIGHT_MID, -20, 0);
    r.value = val;
}

void counters_screen_create() {
    g_game.current_screen = SCREEN_COUNTERS;
    s_row_count = 0;
    s_selected = 0;
    s_life_on_entry = g_game.players[0].life;

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, theme_bg_bottom(), 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_screen, screen_gesture_cb, LV_EVENT_GESTURE, nullptr);
    nav_home_set(go_back);   /* double-tap background = back to life */

    lv_obj_t *ring = lv_obj_create(s_screen);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, 352, 352);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ring, 6, 0);
    lv_obj_set_style_border_color(ring, theme_accent(), 0);
    lv_obj_set_style_border_opa(ring, LV_OPA_60, 0);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "COUNTERS");
    lv_obj_set_style_text_font(title, &font_bold_24, 0);
    lv_obj_set_style_text_letter_space(title, 4, 0);
    lv_obj_set_style_text_color(title, theme_accent(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -134);

    lv_obj_t *list = lv_obj_create(s_screen);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, ROW_W + 8, LIST_H);
    lv_obj_align(list, LV_ALIGN_CENTER, 0, -4);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list, ROW_GAP, 0);
    lv_obj_set_style_pad_ver(list, 4, 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(list, LV_OBJ_FLAG_GESTURE_BUBBLE);

    if (g_game.track_commander_damage) {
        static const char *names[CMDR_OPPONENTS] = {"CMDR 1", "CMDR 2", "CMDR 3"};
        for (uint8_t i = 0; i < CMDR_OPPONENTS; i++) add_row(list, CTR_CMDR, i, names[i]);
    }
    if (g_game.track_poison)     add_row(list, CTR_POISON, 0, "POISON");
    if (g_game.track_energy)     add_row(list, CTR_ENERGY, 0, "ENERGY");
    if (g_game.track_experience) add_row(list, CTR_EXPERIENCE, 0, "EXP");
    if (g_game.track_storm)      add_row(list, CTR_STORM, 0, "STORM");

    if (s_row_count == 0) {
        lv_obj_t *msg = lv_label_create(s_screen);
        lv_label_set_text(msg, "NONE ON\n\nSWIPE DOWN\nFOR THE MENU");
        lv_obj_set_style_text_font(msg, &font_bold_24, 0);
        lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(msg, THEME_LIFE_TEXT, 0);
        lv_obj_center(msg);
    }

    s_life_lbl = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_life_lbl, &font_bold_24, 0);
    lv_obj_set_style_text_color(s_life_lbl, theme_healthy(), 0);
    lv_obj_align(s_life_lbl, LV_ALIGN_CENTER, 0, 132);

    refresh();
    input_driver_set_encoder_handler(encoder_handler);
    lv_scr_load_anim(s_screen, LV_SCR_LOAD_ANIM_FADE_ON, SCREEN_SLIDE_MS, 0, true);
}

void counters_screen_release() {
    s_screen = nullptr;
    s_life_lbl = nullptr;
    for (uint8_t i = 0; i < MAX_ROWS; i++) {
        s_rows[i].row = nullptr;
        s_rows[i].value = nullptr;
    }
    s_row_count = 0;
}
