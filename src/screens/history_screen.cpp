#include "history_screen.h"
#include "counter_screen.h"
#include "../ui/theme.h"
#include "../ui/nav_home.h"
#include "../app_state.h"
#include "../input_driver.h"
#include "../life_history.h"
#include <Arduino.h>
#include <stdio.h>

static lv_obj_t *s_screen = nullptr;
static lv_obj_t *s_list = nullptr;
static lv_obj_t *s_reset_btn = nullptr;
static lv_obj_t *s_reset_lbl = nullptr;
static lv_timer_t *s_confirm_timer = nullptr;
static bool s_confirming = false;

#define HISTORY_SCROLL_PX 44   /* one knob click scrolls about one row */
#define RESET_CONFIRM_MS  3000

static void go_back() {
    /* Same create-before-destroy order as every other screen swap. */
    counter_screen_create_anim(LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    history_screen_release();
}

/* Two-tap reset: first tap arms it (button turns red, "CONFIRM"); a second
 * tap within RESET_CONFIRM_MS resets life to the starting total and clears
 * the history. Otherwise it quietly disarms. */
static void set_confirming(bool on) {
    s_confirming = on;
    if (!s_reset_btn) return;
    lv_label_set_text(s_reset_lbl, on ? "CONFIRM" : "RESET");
    lv_obj_set_style_bg_color(s_reset_btn, on ? THEME_LIFE_CRITICAL : theme_bg_top(), 0);
    lv_obj_set_style_border_color(s_reset_btn, on ? THEME_LIFE_CRITICAL : theme_accent(), 0);
    lv_obj_set_style_text_color(s_reset_lbl, on ? lv_color_white() : theme_accent(), 0);
}

static void confirm_timeout_cb(lv_timer_t *t) {
    lv_timer_pause(t);
    set_confirming(false);
}

static void reset_btn_cb(lv_event_t *e) {
    (void)e;
    if (!s_screen) return;
    if (!s_confirming) {
        set_confirming(true);
        lv_timer_reset(s_confirm_timer);
        lv_timer_resume(s_confirm_timer);
        return;
    }
    lv_timer_pause(s_confirm_timer);
    s_confirming = false;
    /* Also clears the life history and restarts the game clock. */
    app_state_reset_game(g_game.player_count, g_game.starting_life);
    go_back();
}

static void screen_gesture_cb(lv_event_t *e) {
    (void)e;
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    if (lv_indev_get_gesture_dir(indev) == LV_DIR_RIGHT) go_back();
}

static void encoder_handler(input_encoder_event_t evt, int16_t data) {
    if (g_game.current_screen != SCREEN_HISTORY || !s_list) return;
    if (evt == INPUT_ENC_TURN) {
        /* Clockwise moves down the list, toward older entries. */
        lv_obj_scroll_by(s_list, 0, -data * HISTORY_SCROLL_PX, LV_ANIM_ON);
    }
}

static void format_time(char *buf, size_t len, uint32_t ms) {
    uint32_t s = ms / 1000;
    if (s >= 3600) {
        snprintf(buf, len, "%lu:%02lu:%02lu", (unsigned long)(s / 3600),
                 (unsigned long)((s / 60) % 60), (unsigned long)(s % 60));
    } else {
        snprintf(buf, len, "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
    }
}

static lv_obj_t *add_row(const char *time_txt, const char *delta_txt, lv_color_t delta_color,
                         const char *life_txt) {
    lv_obj_t *row = lv_obj_create(s_list);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), 40);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_SCROLL_CHAIN);

    lv_obj_t *t = lv_label_create(row);
    lv_label_set_text(t, time_txt);
    lv_obj_set_style_text_font(t, &font_bold_24, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(0x7c8190), 0);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t *d = lv_label_create(row);
    lv_label_set_text(d, delta_txt);
    lv_obj_set_style_text_font(d, &font_bold_24, 0);
    lv_obj_set_style_text_color(d, delta_color, 0);
    lv_obj_align(d, LV_ALIGN_CENTER, 14, 0);

    lv_obj_t *l = lv_label_create(row);
    lv_label_set_text(l, life_txt);
    lv_obj_set_style_text_font(l, &font_bold_24, 0);
    lv_obj_set_style_text_color(l, THEME_LIFE_TEXT, 0);
    lv_obj_align(l, LV_ALIGN_RIGHT_MID, -8, 0);
    return row;
}

void history_screen_create(lv_scr_load_anim_t anim) {
    g_game.current_screen = SCREEN_HISTORY;

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, theme_bg_bottom(), 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_screen, screen_gesture_cb, LV_EVENT_GESTURE, nullptr);
    nav_home_set(go_back);   /* double-tap background = back to life */

    /* Thin accent ring echoing the counter screen's life ring. */
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
    lv_label_set_text(title, "HISTORY");
    lv_obj_set_style_text_font(title, &font_bold_24, 0);
    lv_obj_set_style_text_letter_space(title, 4, 0);
    lv_obj_set_style_text_color(title, theme_accent(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    /* List sits inside the round area: 250 wide is safe across the middle
     * band, and rows near the top/bottom edges stay readable. */
    s_list = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_list);
    lv_obj_set_size(s_list, 250, 196);
    lv_obj_align(s_list, LV_ALIGN_CENTER, 0, -4);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_list, 4, 0);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_GESTURE_BUBBLE);

    char time_buf[12], delta_buf[8], life_buf[8];
    uint8_t n = life_history_count();
    if (n == 0) {
        lv_obj_t *empty = lv_label_create(s_list);
        lv_label_set_text(empty, "No changes yet");
        lv_obj_set_style_text_font(empty, &font_bold_24, 0);
        lv_obj_set_style_text_color(empty, lv_color_hex(0x7c8190), 0);
        lv_obj_set_width(empty, LV_PCT(100));
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_top(empty, 60, 0);
    }
    for (uint8_t i = 0; i < n; i++) {
        const life_history_entry_t *e = life_history_get(i);
        if (!e) break;
        format_time(time_buf, sizeof(time_buf), e->t_ms);
        snprintf(delta_buf, sizeof(delta_buf), "%+d", e->delta);
        snprintf(life_buf, sizeof(life_buf), "%d", e->life);
        add_row(time_buf, delta_buf, e->delta > 0 ? THEME_GAIN : THEME_LOSS, life_buf);
    }
    if (n > 0) {
        snprintf(life_buf, sizeof(life_buf), "%d", life_history_starting_life());
        add_row("0:00", "start", theme_accent(), life_buf);
    }

    /* Reset button along the bottom of the round display. */
    s_reset_btn = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_reset_btn);
    lv_obj_set_size(s_reset_btn, 160, 44);
    lv_obj_align(s_reset_btn, LV_ALIGN_CENTER, 0, 124);
    lv_obj_set_style_radius(s_reset_btn, 22, 0);
    lv_obj_set_style_bg_opa(s_reset_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_reset_btn, 3, 0);
    lv_obj_set_style_bg_opa(s_reset_btn, LV_OPA_70, LV_STATE_PRESSED);
    lv_obj_clear_flag(s_reset_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_reset_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_reset_btn, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(s_reset_btn, reset_btn_cb, LV_EVENT_CLICKED, nullptr);
    s_reset_lbl = lv_label_create(s_reset_btn);
    lv_obj_set_style_text_font(s_reset_lbl, &font_bold_24, 0);
    lv_obj_set_style_text_letter_space(s_reset_lbl, 2, 0);
    lv_obj_center(s_reset_lbl);
    if (!s_confirm_timer) {
        s_confirm_timer = lv_timer_create(confirm_timeout_cb, RESET_CONFIRM_MS, nullptr);
    }
    lv_timer_pause(s_confirm_timer);
    set_confirming(false);

    input_driver_set_encoder_handler(encoder_handler);
    if (anim == LV_SCR_LOAD_ANIM_NONE) lv_scr_load(s_screen);
    else lv_scr_load_anim(s_screen, anim, SCREEN_SLIDE_MS, 0, true);
}

void history_screen_destroy() {
    if (s_screen) lv_obj_del_async(s_screen);
    history_screen_release();
}

void history_screen_release() {
    s_screen = nullptr;
    s_list = nullptr;
    s_reset_btn = nullptr;
    s_reset_lbl = nullptr;
    s_confirming = false;
    if (s_confirm_timer) lv_timer_pause(s_confirm_timer);
}
