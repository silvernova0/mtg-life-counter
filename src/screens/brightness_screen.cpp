#include "brightness_screen.h"
#include "counter_screen.h"
#include "../ui/theme.h"
#include "../ui/nav_home.h"
#include "../app_state.h"
#include "../input_driver.h"
#include "../backlight.h"
#include <stdio.h>

#define BRIGHTNESS_STEP 5   /* percent per knob click */

static lv_obj_t *s_screen = nullptr;
static lv_obj_t *s_arc = nullptr;
static lv_obj_t *s_value_lbl = nullptr;

static void show_value(uint8_t percent) {
    if (!s_value_lbl) return;
    char buf[6];
    snprintf(buf, sizeof(buf), "%u", (unsigned)percent);
    lv_label_set_text(s_value_lbl, buf);
}

static void set_level(int v, bool move_arc) {
    backlight_set_percent((uint8_t)(v < 0 ? 0 : v), false);  /* live, saved on exit */
    uint8_t p = backlight_get_percent();
    if (move_arc && s_arc) lv_arc_set_value(s_arc, p);
    show_value(p);
}

static void go_back() {
    backlight_set_percent(backlight_get_percent(), true);  /* save once */
    counter_screen_create_anim(LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
    brightness_screen_release();
}

static void arc_changed_cb(lv_event_t *e) {
    if (!s_arc) return;
    set_level(lv_arc_get_value(lv_event_get_target(e)), false);
}

static void screen_gesture_cb(lv_event_t *e) {
    (void)e;
    if (!s_screen) return;
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    if (lv_indev_get_gesture_dir(indev) == LV_DIR_BOTTOM) go_back();
}

static void encoder_handler(input_encoder_event_t evt, int16_t data) {
    if (g_game.current_screen != SCREEN_BRIGHTNESS || !s_screen) return;
    if (evt != INPUT_ENC_TURN) return;
    set_level((int)backlight_get_percent() + data * BRIGHTNESS_STEP, true);
}

void brightness_screen_create(lv_scr_load_anim_t anim) {
    g_game.current_screen = SCREEN_BRIGHTNESS;

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, theme_bg_bottom(), 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_screen, screen_gesture_cb, LV_EVENT_GESTURE, nullptr);
    nav_home_set(go_back);   /* double-tap background = back to life */

    /* 270-degree ring with a draggable knob, open at the bottom. */
    s_arc = lv_arc_create(s_screen);
    lv_obj_set_size(s_arc, 320, 320);
    lv_obj_center(s_arc);
    lv_arc_set_rotation(s_arc, 135);
    lv_arc_set_bg_angles(s_arc, 0, 270);
    lv_arc_set_range(s_arc, BACKLIGHT_MIN_PERCENT, 100);
    lv_arc_set_value(s_arc, backlight_get_percent());
    lv_obj_set_style_arc_width(s_arc, 22, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 22, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc, theme_track(), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, theme_accent(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s_arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_arc, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_arc, 6, LV_PART_KNOB);
    lv_obj_add_event_cb(s_arc, arc_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    /* Only the ring itself takes touches, so taps in the middle reach the
     * screen (double-tap = back to life); a little extra margin keeps the
     * ring easy to grab. */
    lv_obj_add_flag(s_arc, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_set_ext_click_area(s_arc, 14);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "BRIGHTNESS");
    lv_obj_set_style_text_font(title, &font_bold_24, 0);
    lv_obj_set_style_text_letter_space(title, 3, 0);
    lv_obj_set_style_text_color(title, theme_accent(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -88);

    s_value_lbl = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_value_lbl, &font_life_110, 0);
    lv_obj_set_style_text_color(s_value_lbl, THEME_LIFE_TEXT, 0);
    lv_obj_align(s_value_lbl, LV_ALIGN_CENTER, 0, 6);

    lv_obj_t *pct = lv_label_create(s_screen);
    lv_label_set_text(pct, "%");
    lv_obj_set_style_text_font(pct, &font_bold_32, 0);
    lv_obj_set_style_text_color(pct, theme_accent(), 0);
    lv_obj_align(pct, LV_ALIGN_CENTER, 0, 92);

    show_value(backlight_get_percent());

    input_driver_set_encoder_handler(encoder_handler);
    if (anim == LV_SCR_LOAD_ANIM_NONE) lv_scr_load(s_screen);
    else lv_scr_load_anim(s_screen, anim, SCREEN_SLIDE_MS, 0, true);
}

void brightness_screen_release() {
    s_screen = nullptr;
    s_arc = nullptr;
    s_value_lbl = nullptr;
}
