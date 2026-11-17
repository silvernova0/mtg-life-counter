#include "theme_picker_screen.h"
#include "counter_screen.h"
#include "../ui/theme.h"
#include "../app_state.h"
#include "../input_driver.h"

#define ROW_W        230
#define ROW_H        40
#define ROW_GAP      6
#define LIST_H       236     /* about five rows visible; the list scrolls */

static lv_obj_t *s_screen = nullptr;
static lv_obj_t *s_list = nullptr;
static lv_obj_t *s_ring = nullptr;
static lv_obj_t *s_title = nullptr;
static lv_obj_t *s_rows[THEME_PRESET_COUNT];
static lv_obj_t *s_names[THEME_PRESET_COUNT];
static uint8_t s_selected = 0;

static void restyle() {
    if (!s_screen) return;
    lv_obj_set_style_bg_color(s_screen, theme_bg_bottom(), 0);
    lv_obj_set_style_border_color(s_ring, theme_accent(), 0);
    lv_obj_set_style_text_color(s_title, theme_accent(), 0);
    for (uint8_t i = 0; i < THEME_PRESET_COUNT; i++) {
        bool sel = (i == s_selected);
        lv_obj_set_style_border_width(s_rows[i], sel ? 4 : 2, 0);
        lv_obj_set_style_border_opa(s_rows[i], sel ? LV_OPA_COVER : LV_OPA_40, 0);
        lv_obj_set_style_text_color(s_names[i],
            sel ? theme_preset_accent(i) : THEME_LIFE_TEXT, 0);
    }
}

static void go_back() {
    theme_set(s_selected, true);   /* save the final choice to flash once */
    counter_screen_create_anim(LV_SCR_LOAD_ANIM_MOVE_LEFT);
    theme_picker_release();
}

static void select(uint8_t idx) {
    s_selected = idx;
    theme_set(idx, false);         /* live preview, not yet saved */
    restyle();
    if (s_rows[idx]) lv_obj_scroll_to_view(s_rows[idx], LV_ANIM_ON);
}

static void row_clicked_cb(lv_event_t *e) {
    if (!s_screen) return;
    select((uint8_t)(uintptr_t)lv_event_get_user_data(e));
    go_back();
}

static void screen_gesture_cb(lv_event_t *e) {
    (void)e;
    if (!s_screen) return;
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    if (lv_indev_get_gesture_dir(indev) == LV_DIR_LEFT) go_back();
}

static void encoder_handler(input_encoder_event_t evt, int16_t data) {
    if (g_game.current_screen != SCREEN_THEME_PICKER || !s_screen) return;
    if (evt != INPUT_ENC_TURN) return;
    int16_t idx = (int16_t)s_selected + data;
    while (idx < 0) idx += THEME_PRESET_COUNT;
    select((uint8_t)(idx % THEME_PRESET_COUNT));
}

void theme_picker_create(lv_scr_load_anim_t anim) {
    g_game.current_screen = SCREEN_THEME_PICKER;
    s_selected = theme_index();

    s_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_screen, screen_gesture_cb, LV_EVENT_GESTURE, nullptr);

    s_ring = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_ring);
    lv_obj_set_size(s_ring, 352, 352);
    lv_obj_center(s_ring);
    lv_obj_set_style_radius(s_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s_ring, 6, 0);
    lv_obj_set_style_border_opa(s_ring, LV_OPA_60, 0);
    lv_obj_clear_flag(s_ring, LV_OBJ_FLAG_CLICKABLE);

    s_title = lv_label_create(s_screen);
    lv_label_set_text(s_title, "THEME");
    lv_obj_set_style_text_font(s_title, &font_bold_24, 0);
    lv_obj_set_style_text_letter_space(s_title, 4, 0);
    lv_obj_align(s_title, LV_ALIGN_CENTER, 0, -130);

    s_list = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_list);
    lv_obj_set_size(s_list, ROW_W + 8, LIST_H);
    lv_obj_align(s_list, LV_ALIGN_CENTER, 0, 16);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_list, ROW_GAP, 0);
    lv_obj_set_style_pad_ver(s_list, 4, 0);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_GESTURE_BUBBLE);

    for (uint8_t i = 0; i < THEME_PRESET_COUNT; i++) {
        lv_obj_t *row = lv_obj_create(s_list);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, ROW_W, ROW_H);
        lv_obj_set_style_radius(row, ROW_H / 2, 0);
        lv_obj_set_style_bg_color(row, theme_preset_bg_top(i), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, theme_preset_accent(i), 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_SCROLL_CHAIN);
        lv_obj_add_event_cb(row, row_clicked_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        s_rows[i] = row;

        lv_obj_t *swatch = lv_obj_create(row);
        lv_obj_remove_style_all(swatch);
        lv_obj_set_size(swatch, 22, 22);
        lv_obj_align(swatch, LV_ALIGN_LEFT_MID, 12, 0);
        lv_obj_set_style_radius(swatch, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(swatch, theme_preset_accent(i), 0);
        lv_obj_set_style_bg_opa(swatch, LV_OPA_COVER, 0);
        lv_obj_clear_flag(swatch, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *name = lv_label_create(row);
        lv_label_set_text(name, theme_preset_name(i));
        lv_obj_set_style_text_font(name, &font_bold_24, 0);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 50, 0);
        s_names[i] = name;
    }

    restyle();
    /* Start with the current theme in view. */
    lv_obj_update_layout(s_screen);
    lv_obj_scroll_to_view(s_rows[s_selected], LV_ANIM_OFF);
    input_driver_set_encoder_handler(encoder_handler);
    if (anim == LV_SCR_LOAD_ANIM_NONE) lv_scr_load(s_screen);
    else lv_scr_load_anim(s_screen, anim, SCREEN_SLIDE_MS, 0, true);
}

void theme_picker_release() {
    s_screen = nullptr;
    s_list = nullptr;
    s_ring = nullptr;
    s_title = nullptr;
    for (uint8_t i = 0; i < THEME_PRESET_COUNT; i++) {
        s_rows[i] = nullptr;
        s_names[i] = nullptr;
    }
}
