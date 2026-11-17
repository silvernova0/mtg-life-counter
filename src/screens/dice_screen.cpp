#include "dice_screen.h"
#include "counter_screen.h"
#include "../ui/theme.h"
#include "../ui/nav_home.h"
#include "../app_state.h"
#include "../input_driver.h"
#include <Arduino.h>
#include <stdio.h>

#define ROLL_MS       700    /* length of the tumble */
#define ROLL_FRAMES   14     /* how many fake faces flash past */

#define COLOR_NAT20   lv_color_hex(0xf2c14e)
#define COLOR_NAT1    lv_color_hex(0xff3b3b)
#define COLOR_DIM     lv_color_hex(0x8a8a90)

static lv_obj_t *s_screen = nullptr;
static lv_obj_t *s_ring = nullptr;
static lv_obj_t *s_result = nullptr;
static lv_obj_t *s_flavor = nullptr;
static lv_obj_t *s_prev = nullptr;
static bool s_rolling = false;
static int s_shown = 0;      /* face currently on screen (0 = none yet) */
static int s_last = 0;       /* previous final roll */

static int d20() { return (int)(esp_random() % 20) + 1; }

static void show_face(int v, lv_color_t color) {
    if (!s_result) return;
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", v);
    lv_label_set_text(s_result, buf);
    lv_obj_set_style_text_color(s_result, color, 0);
}

static void tumble_exec_cb(void *var, int32_t v) {
    (void)var;
    (void)v;
    if (!s_result) return;
    int face;
    do { face = d20(); } while (face == s_shown);   /* always visibly change */
    s_shown = face;
    show_face(face, THEME_LIFE_TEXT);
}

/* lv_anim only calls exec_cb when the value changes, so stepping 0..FRAMES
 * with an ease-out path gives a tumble that slows down before it lands. */
static void tumble_ready_cb(lv_anim_t *a) {
    (void)a;
    s_rolling = false;
    if (!s_screen) return;
    int r = d20();
    s_shown = r;

    lv_color_t c = theme_accent();
    const char *flavor = "";
    if (r == 20) { c = COLOR_NAT20; flavor = "NATURAL 20!"; }
    else if (r == 1) { c = COLOR_NAT1; flavor = "NATURAL 1"; }
    show_face(r, c);
    lv_label_set_text(s_flavor, flavor);
    lv_obj_set_style_text_color(s_flavor, c, 0);
    lv_obj_set_style_border_color(s_ring, c, 0);
    lv_obj_set_style_border_opa(s_ring, LV_OPA_COVER, 0);

    if (s_last > 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "LAST %d", s_last);
        lv_label_set_text(s_prev, buf);
    }
    s_last = r;
}

static void roll() {
    if (!s_screen || s_rolling) return;
    s_rolling = true;
    lv_label_set_text(s_flavor, "");
    lv_obj_set_style_border_color(s_ring, theme_accent(), 0);
    lv_obj_set_style_border_opa(s_ring, LV_OPA_40, 0);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_result);
    lv_anim_set_values(&a, 0, ROLL_FRAMES);
    lv_anim_set_time(&a, ROLL_MS);
    lv_anim_set_exec_cb(&a, tumble_exec_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&a, tumble_ready_cb);
    lv_anim_start(&a);
}

static void go_back() {
    /* Stop a tumble still in flight; its callbacks would touch this screen. */
    if (s_result) lv_anim_del(s_result, nullptr);
    s_rolling = false;
    counter_screen_create_anim(LV_SCR_LOAD_ANIM_FADE_ON);
    dice_screen_release();
}

static void screen_clicked_cb(lv_event_t *e) {
    (void)e;
    lv_indev_t *indev = lv_indev_get_act();
    /* The release that ends a swipe still arrives as a click. */
    if (indev && lv_indev_get_gesture_dir(indev) != LV_DIR_NONE) return;
    roll();
}

static void screen_gesture_cb(lv_event_t *e) {
    (void)e;
    if (!s_screen) return;
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    if (lv_indev_get_gesture_dir(indev) != LV_DIR_NONE) go_back();
}

static void encoder_handler(input_encoder_event_t evt, int16_t data) {
    (void)data;
    if (g_game.current_screen != SCREEN_DICE || !s_screen) return;
    if (evt == INPUT_ENC_TURN) roll();
}

void dice_screen_create() {
    g_game.current_screen = SCREEN_DICE;
    s_shown = 0;

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, theme_bg_bottom(), 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_screen, screen_gesture_cb, LV_EVENT_GESTURE, nullptr);
    nav_home_set(go_back);   /* double-tap background = back to life */
    lv_obj_add_event_cb(s_screen, screen_clicked_cb, LV_EVENT_CLICKED, nullptr);

    s_ring = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_ring);
    lv_obj_set_size(s_ring, 352, 352);
    lv_obj_center(s_ring);
    lv_obj_set_style_radius(s_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s_ring, 8, 0);
    lv_obj_set_style_border_color(s_ring, theme_accent(), 0);
    lv_obj_set_style_border_opa(s_ring, LV_OPA_40, 0);
    lv_obj_clear_flag(s_ring, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *disc = lv_obj_create(s_screen);
    lv_obj_remove_style_all(disc);
    lv_obj_set_size(disc, 300, 300);
    lv_obj_center(disc);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(disc, theme_bg_top(), 0);
    lv_obj_set_style_bg_grad_color(disc, theme_bg_bottom(), 0);
    lv_obj_set_style_bg_grad_dir(disc, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(disc, LV_OPA_COVER, 0);
    lv_obj_clear_flag(disc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "D20");
    lv_obj_set_style_text_font(title, &font_bold_24, 0);
    lv_obj_set_style_text_letter_space(title, 6, 0);
    lv_obj_set_style_text_color(title, theme_accent(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -100);

    s_result = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_result, &font_life_110, 0);
    lv_obj_align(s_result, LV_ALIGN_CENTER, 0, 2);
    lv_label_set_text(s_result, "-");
    lv_obj_set_style_text_color(s_result, COLOR_DIM, 0);

    s_flavor = lv_label_create(s_screen);
    lv_label_set_text(s_flavor, "");
    lv_obj_set_style_text_font(s_flavor, &font_bold_24, 0);
    lv_obj_align(s_flavor, LV_ALIGN_CENTER, 0, 92);

    s_prev = lv_label_create(s_screen);
    lv_label_set_text(s_prev, "");
    lv_obj_set_style_text_font(s_prev, &font_bold_24, 0);
    lv_obj_set_style_text_color(s_prev, COLOR_DIM, 0);
    lv_obj_align(s_prev, LV_ALIGN_CENTER, 0, 128);

    if (s_last > 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "LAST %d", s_last);
        lv_label_set_text(s_prev, buf);
    }

    input_driver_set_encoder_handler(encoder_handler);
    lv_scr_load(s_screen);
    roll();   /* opening it is asking for a roll */
}

void dice_screen_release() {
    if (s_result) lv_anim_del(s_result, nullptr);
    s_rolling = false;
    s_screen = nullptr;
    s_ring = nullptr;
    s_result = nullptr;
    s_flavor = nullptr;
    s_prev = nullptr;
}
