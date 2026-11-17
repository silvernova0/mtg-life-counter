#include "counter_screen.h"
#include "dice_overlay.h"
#include "menu_screen.h"
#include "player_detail_overlay.h"
#include "history_screen.h"
#include "theme_picker_screen.h"
#include "brightness_screen.h"
#include "counters_screen.h"
#include "../life_history.h"
#include "../ui/theme.h"
#include "../ui/nav_home.h"
#include "../app_state.h"
#include "../input_driver.h"
#include "../led_driver.h"
#include "board_config.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <stdio.h>

typedef struct { int16_t dx, dy; } player_pos_t;

/* Offsets from screen center (120,120), tuned so labels + their +/- row
 * stay inside the round visible area (corners of the square panel are
 * hidden under the bezel). Indexed [player_count-1][player_idx]. */
static const player_pos_t POSITIONS[MAX_PLAYERS][MAX_PLAYERS] = {
    /* 1 player */  { {0, 0} },
    /* 2 players */ { {0, -55}, {0, 55} },
    /* 3 players */ { {0, -70}, {61, 35}, {-61, 35} },
    /* 4 players */ { {44, -44}, {44, 44}, {-44, 44}, {-44, -44} },
};

static const lv_font_t *LIFE_FONTS[MAX_PLAYERS] = {
    &lv_font_montserrat_48,
    &lv_font_montserrat_40,
    &lv_font_montserrat_28,
    &lv_font_montserrat_28,
};

static lv_obj_t *s_screen = nullptr;
static lv_obj_t *s_containers[MAX_PLAYERS];
static lv_obj_t *s_life_labels[MAX_PLAYERS];
static lv_obj_t *s_status_labels[MAX_PLAYERS]; /* small poison/energy badge, blank when nothing to show */
/* player_count the currently-built widget tree was laid out for — used by
 * counter_screen_rebuild_if_active() (table sync) to tell "just refresh
 * the numbers" apart from "the player count itself changed under us,
 * rebuild the layout". */
static uint8_t s_built_for_player_count = 0;

#if defined(BOARD_WAVESHARE_S3)
/* Single-player "hero" display on the 360x360 round panel: a colored life
 * ring around the bezel, a large bold life number tinted to match, and a
 * transient "+N"/"-N" badge that sums quick dial turns. When the badge
 * times out, that summed change is written to the life history. */
#define HERO_DELTA_MS    1500
#define WATERMARK_OPA    LV_OPA_40   /* strong enough to see, still keeps the number readable */

static lv_obj_t *s_ring = nullptr;
static lv_obj_t *s_hero_disc = nullptr;
static lv_obj_t *s_hero_title = nullptr;
static lv_obj_t *s_delta_label = nullptr;
static lv_obj_t *s_watermark = nullptr;
static lv_timer_t *s_delta_timer = nullptr;
static int16_t s_last_life = 0;
static int16_t s_delta_accum = 0;

static bool hero_mode() {
    return g_game.player_count == 1 && s_ring != nullptr;
}

static lv_color_t hero_life_color(int16_t life) {
    int16_t start = g_game.starting_life > 0 ? g_game.starting_life : DEFAULT_START_LIFE;
    if (life <= 0) return THEME_DANGER;
    int32_t pct = (int32_t)life * 100 / start;
    if (pct >= 60) return theme_healthy();
    if (pct >= 30) return THEME_LIFE_HURT;
    return THEME_LIFE_CRITICAL;
}

/* Writes any not-yet-logged change to the history (called when the badge
 * times out, and before leaving the screen so nothing is lost). */
static void hero_commit_pending() {
    if (s_delta_accum != 0) life_history_push(s_delta_accum, s_last_life);
    s_delta_accum = 0;
}

static void hero_delta_timer_cb(lv_timer_t *t) {
    hero_commit_pending();
    if (s_delta_label) lv_obj_add_flag(s_delta_label, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(t);
}

static void hero_show_badge(const char *text, lv_color_t color) {
    if (!s_delta_label || !s_delta_timer) return;
    lv_label_set_text(s_delta_label, text);
    lv_obj_set_style_text_color(s_delta_label, color, 0);
    lv_obj_clear_flag(s_delta_label, LV_OBJ_FLAG_HIDDEN);
    lv_timer_reset(s_delta_timer);
    lv_timer_resume(s_delta_timer);
}

/* Guild themes paint the healthy ring with a two-color texture: color A
 * at the top blending to color B at the bottom, symmetric left/right so
 * there is no seam where the ring starts. Built once per color pair in
 * PSRAM (352x352 RGB565, ~240 KB) and reused. */
#define RING_PX 352
static lv_img_dsc_t s_ring_img;
static lv_color_t *s_ring_buf = nullptr;
static uint32_t s_ring_key = 0;

static const lv_img_dsc_t *ring_texture() {
    lv_color_t b;
    if (!theme_ring_second(&b)) return nullptr;
    lv_color_t a = theme_healthy();
    uint32_t key = ((uint32_t)lv_color_to16(a) << 16) | lv_color_to16(b);
    if (s_ring_buf && key == s_ring_key) return &s_ring_img;

    if (!s_ring_buf) {
        s_ring_buf = (lv_color_t *)heap_caps_malloc(RING_PX * RING_PX * sizeof(lv_color_t),
                                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_ring_buf) return nullptr;
    }
    const float c = (RING_PX - 1) / 2.0f;
    for (int y = 0; y < RING_PX; y++) {
        float dy = y - c;
        for (int x = 0; x < RING_PX; x++) {
            float dx = x - c;
            float d = sqrtf(dx * dx + dy * dy);
            float t = d > 0.5f ? (1.0f + dy / d) * 0.5f : 0.5f;   /* 0 top .. 1 bottom */
            s_ring_buf[y * RING_PX + x] = lv_color_mix(b, a, (uint8_t)(t * 255.0f + 0.5f));
        }
    }
    s_ring_key = key;
    s_ring_img.header.always_zero = 0;
    s_ring_img.header.cf = LV_IMG_CF_TRUE_COLOR;
    s_ring_img.header.w = RING_PX;
    s_ring_img.header.h = RING_PX;
    s_ring_img.data_size = RING_PX * RING_PX * sizeof(lv_color_t);
    s_ring_img.data = (const uint8_t *)s_ring_buf;
    return &s_ring_img;
}

static void hero_update(int16_t life) {
    lv_color_t c = hero_life_color(life);
    int16_t start = g_game.starting_life > 0 ? g_game.starting_life : DEFAULT_START_LIFE;

    lv_arc_set_range(s_ring, 0, start);
    lv_arc_set_value(s_ring, life < 0 ? 0 : (life > start ? start : life));
    lv_obj_set_style_arc_color(s_ring, c, LV_PART_INDICATOR);
    /* Guild themes keep their two-color ring at every life total; only
     * one-color themes shift to the hurt/critical warning colors. */
    const lv_img_dsc_t *tex = ring_texture();
    if (lv_obj_get_style_arc_img_src(s_ring, LV_PART_INDICATOR) != tex) {
        lv_obj_set_style_arc_img_src(s_ring, tex, LV_PART_INDICATOR);
    }
    /* The number keeps the theme color all game; only the ring shifts
     * through hurt/critical. */
    if (s_life_labels[0]) lv_obj_set_style_text_color(s_life_labels[0], theme_healthy(), 0);

    int16_t diff = life - s_last_life;
    s_last_life = life;
    if (diff == 0) return;
    s_delta_accum += diff;
    if (s_delta_accum == 0) {
        if (s_delta_label) lv_obj_add_flag(s_delta_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    char buf[8];
    snprintf(buf, sizeof(buf), "%+d", s_delta_accum);
    hero_show_badge(buf, s_delta_accum > 0 ? THEME_GAIN : THEME_LOSS);
}

/* Pushes the active theme's palette onto the already-built widgets. */
static void hero_apply_theme() {
    if (!hero_mode()) return;
    lv_obj_set_style_bg_color(s_screen, theme_bg_bottom(), 0);
    lv_obj_set_style_arc_color(s_ring, theme_track(), LV_PART_MAIN);
    if (s_hero_disc) {
        lv_obj_set_style_bg_color(s_hero_disc, theme_bg_top(), 0);
        lv_obj_set_style_bg_grad_color(s_hero_disc, theme_bg_bottom(), 0);
    }
    if (s_hero_title) lv_obj_set_style_text_color(s_hero_title, theme_accent(), 0);
    if (s_watermark) {
        const char *glyph = theme_watermark();
        if (glyph) {
            lv_label_set_text(s_watermark, glyph);
            lv_obj_set_style_text_color(s_watermark, theme_accent(), 0);
            lv_obj_clear_flag(s_watermark, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_watermark, LV_OBJ_FLAG_HIDDEN);
        }
    }
    hero_update(g_game.players[0].life);
}

#endif

static void update_container_style(uint8_t i) {
    if (!s_containers[i]) return;
    bool eliminated = (g_game.players[i].life <= 0);
    lv_obj_set_style_text_color(s_life_labels[i],
        eliminated ? THEME_DANGER : THEME_LIFE_TEXT, 0);

#if BOARD_HAS_ENCODER
#if defined(BOARD_WAVESHARE_S3)
    bool selected = (g_game.player_count > 1 && i == g_game.selected_player);
#else
    bool selected = (i == g_game.selected_player);
#endif
    lv_obj_set_style_border_width(s_containers[i], selected ? 2 : 0, 0);
    lv_obj_set_style_border_color(s_containers[i], THEME_SELECTED_RING, 0);
#endif
}

static void update_status_badge(uint8_t i) {
    if (!s_status_labels[i]) return;
    char buf[24];
    buf[0] = '\0';
    size_t len = 0;
    if (g_game.track_poison && g_game.players[i].poison > 0) {
        len += snprintf(buf + len, sizeof(buf) - len, "P%d ", g_game.players[i].poison);
    }
    if (g_game.track_energy && g_game.players[i].energy > 0) {
        len += snprintf(buf + len, sizeof(buf) - len, "E%d ", g_game.players[i].energy);
    }
    if (g_game.track_experience && g_game.players[i].experience > 0) {
        len += snprintf(buf + len, sizeof(buf) - len, "X%d", g_game.players[i].experience);
    }
    while (len > 0 && buf[len - 1] == ' ') buf[--len] = ' ';
    lv_label_set_text(s_status_labels[i], buf);
}

void counter_screen_refresh() {
    /* Guards against a dangling call: if this screen isn't the one on
     * display right now (e.g. a queued event fires after we've already
     * navigated away), s_life_labels[]/s_containers[] may still be
     * holding pointers into a just-deleted widget tree. Touching those
     * is exactly what produces LVGL's "No screen found" warning followed
     * by a hard crash — bail out instead. */
    if (!s_screen) return;
    for (uint8_t i = 0; i < g_game.player_count; i++) {
        if (!s_life_labels[i]) continue;
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", g_game.players[i].life);
        lv_label_set_text(s_life_labels[i], buf);
        update_container_style(i);
        update_status_badge(i);
    }
#if defined(BOARD_WAVESHARE_S3)
    if (hero_mode()) hero_update(g_game.players[0].life);
#endif
}

#if !defined(BOARD_WAVESHARE_S3)
static void plus_btn_cb(lv_event_t *e) {
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
#if BOARD_HAS_ENCODER
    g_game.selected_player = idx;
#endif
    app_state_adjust_life(idx, +1);
    counter_screen_refresh();
#if BOARD_HAS_ENCODER
    led_driver_refresh();
#endif
}

static void minus_btn_cb(lv_event_t *e) {
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
#if BOARD_HAS_ENCODER
    g_game.selected_player = idx;
#endif
    app_state_adjust_life(idx, -1);
    counter_screen_refresh();
#if BOARD_HAS_ENCODER
    led_driver_refresh();
#endif
}
#endif

#if BOARD_HAS_ENCODER
static void player_select_cb(lv_event_t *e) {
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (idx >= g_game.player_count || idx == g_game.selected_player) return;
    g_game.selected_player = idx;
    counter_screen_refresh();
    led_driver_refresh();
}
#endif

static void player_long_press_cb(lv_event_t *e) {
#if defined(BOARD_WAVESHARE_S3)
    if (hero_mode()) return;   /* a tap opens the counters screen instead */
#endif
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    /* Opens the poison/energy/commander-damage detail view for this
     * player. This touch gesture works on every supported board. */
    player_detail_overlay_open(idx);
}

/* Swiping left off the counter screen (empty background, not one of the
 * player boxes) opens the settings/utility menu — the d20 roller, extra
 * counter toggles, theme/brightness, etc. now live there instead of a
 * button in the middle of this screen. */
static void screen_gesture_cb(lv_event_t *e) {
    (void)e;
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
#if defined(BOARD_WAVESHARE_S3)
    /* One-player knob layout: left = history, right = theme picker,
     * up = brightness, down = menu (dice, settings). */
    if (hero_mode()) {
        if (dir == LV_DIR_LEFT) {
            history_screen_create(LV_SCR_LOAD_ANIM_MOVE_LEFT);
            counter_screen_release();
        } else if (dir == LV_DIR_RIGHT) {
            theme_picker_create(LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            counter_screen_release();
        } else if (dir == LV_DIR_TOP) {
            brightness_screen_create(LV_SCR_LOAD_ANIM_MOVE_TOP);
            counter_screen_release();
        } else if (dir == LV_DIR_BOTTOM) {
            menu_screen_create();
            counter_screen_destroy();
        }
        return;
    }
#endif
    if (dir == LV_DIR_LEFT) {
        menu_screen_create();
        counter_screen_destroy();
    }
}

#if defined(BOARD_WAVESHARE_S3)
/* Tapping the life disc opens the extra counters (commander damage etc). */
static void hero_tap_cb(lv_event_t *e) {
    (void)e;
    if (!hero_mode()) return;
    lv_indev_t *indev = lv_indev_get_act();
    /* The release at the end of a swipe also arrives as a click. */
    if (indev && lv_indev_get_gesture_dir(indev) != LV_DIR_NONE) return;
    counters_screen_create();
    counter_screen_release();
}
#endif

static void build_player_widget(uint8_t idx) {
    player_pos_t pos = POSITIONS[g_game.player_count - 1][idx];

    lv_obj_t *cont = lv_obj_create(s_screen);
    lv_obj_remove_style_all(cont);
#if defined(BOARD_WAVESHARE_S3)
    if (g_game.player_count == 1) {
        lv_obj_set_size(cont, 340, 340);
    } else {
        lv_obj_set_size(cont, 96, 72);
    }
#else
    lv_obj_set_size(cont, 96, 72);
#endif
    lv_obj_align(cont, LV_ALIGN_CENTER, pos.dx, pos.dy);
#if defined(BOARD_WAVESHARE_S3)
    lv_obj_set_style_radius(cont, g_game.player_count == 1 ? LV_RADIUS_CIRCLE : 8, 0);
#else
    lv_obj_set_style_radius(cont, 8, 0);
#endif
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    /* A swipe that starts on a player box (not just empty background)
     * should still reach the screen's own gesture handler and open the
     * menu, instead of being swallowed here. */
    lv_obj_add_flag(cont, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(cont, player_long_press_cb, LV_EVENT_LONG_PRESSED, (void *)(uintptr_t)idx);
    s_containers[idx] = cont;

#if defined(BOARD_WAVESHARE_S3)
    if (g_game.player_count == 1) {
        /* Guild emblem watermark, created first so it sits under the number. */
        lv_obj_t *wm = lv_label_create(cont);
        lv_label_set_text(wm, "");
        lv_obj_set_style_text_font(wm, &font_guild_240, 0);
        lv_obj_set_style_text_opa(wm, WATERMARK_OPA, 0);
        lv_obj_align(wm, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_flag(wm, LV_OBJ_FLAG_HIDDEN);
        s_watermark = wm;
    }
#endif

    lv_obj_t *label = lv_label_create(cont);
    lv_label_set_text(label, "--");
#if defined(BOARD_WAVESHARE_S3)
    if (g_game.player_count == 1) {
        lv_obj_set_style_text_font(label, &font_life_110, 0);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 2);
    } else {
        lv_obj_set_style_text_font(label, LIFE_FONTS[g_game.player_count - 1], 0);
        lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);
    }
#else
    lv_obj_set_style_text_font(label, LIFE_FONTS[g_game.player_count - 1], 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);
#endif
    lv_obj_set_style_text_color(label, THEME_LIFE_TEXT, 0);
#if BOARD_HAS_ENCODER
    lv_obj_add_flag(label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(label, player_select_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)idx);
    lv_obj_add_event_cb(label, player_long_press_cb, LV_EVENT_LONG_PRESSED, (void *)(uintptr_t)idx);
#endif
    s_life_labels[idx] = label;

    lv_obj_t *status = lv_label_create(cont);
    lv_label_set_text(status, "");
    lv_obj_set_style_text_color(status, theme_accent(), 0);
#if defined(BOARD_WAVESHARE_S3)
    if (g_game.player_count == 1) {
        lv_obj_set_style_text_font(status, &font_bold_24, 0);
        lv_obj_align(status, LV_ALIGN_CENTER, 0, 134);
    } else {
        lv_obj_set_style_text_font(status, &lv_font_montserrat_12, 0);
        lv_obj_align(status, LV_ALIGN_TOP_RIGHT, 2, -4);
    }
#else
    lv_obj_set_style_text_font(status, &lv_font_montserrat_12, 0);
    lv_obj_align(status, LV_ALIGN_TOP_RIGHT, 2, -4);
#endif
    s_status_labels[idx] = status;

#if defined(BOARD_WAVESHARE_S3)
    if (g_game.player_count == 1) {
        /* Inner disc gives the number a slightly lifted panel to sit on. */
        lv_obj_set_style_bg_color(cont, theme_bg_top(), 0);
        lv_obj_set_style_bg_grad_color(cont, theme_bg_bottom(), 0);
        lv_obj_set_style_bg_grad_dir(cont, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
        lv_obj_set_size(cont, 300, 300);
        lv_obj_align(cont, LV_ALIGN_CENTER, 0, 0);
        s_hero_disc = cont;
        lv_obj_add_event_cb(cont, hero_tap_cb, LV_EVENT_SHORT_CLICKED, nullptr);
        /* Let taps on the number fall through to the disc. */
        lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *title = lv_label_create(cont);
        lv_label_set_text(title, "LIFE");
        lv_obj_set_style_text_font(title, &font_bold_24, 0);
        lv_obj_set_style_text_letter_space(title, 6, 0);
        lv_obj_set_style_text_color(title, theme_accent(), 0);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, -100);
        s_hero_title = title;

        lv_obj_t *delta = lv_label_create(cont);
        lv_label_set_text(delta, "");
        lv_obj_set_style_text_font(delta, &font_bold_32, 0);
        lv_obj_align(delta, LV_ALIGN_CENTER, 0, 100);
        lv_obj_add_flag(delta, LV_OBJ_FLAG_HIDDEN);
        s_delta_label = delta;
    }
#endif

#if !defined(BOARD_WAVESHARE_S3)
    lv_obj_t *minus_btn = lv_btn_create(cont);
    lv_obj_set_size(minus_btn, 34, 26);
    lv_obj_align(minus_btn, LV_ALIGN_BOTTOM_LEFT, 4, 0);
    lv_obj_set_style_bg_color(minus_btn, THEME_BTN_MINUS, 0);
    lv_obj_add_event_cb(minus_btn, minus_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)idx);
    lv_obj_t *minus_lbl = lv_label_create(minus_btn);
    lv_label_set_text(minus_lbl, "-");
    lv_obj_set_style_text_font(minus_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(minus_lbl);

    lv_obj_t *plus_btn = lv_btn_create(cont);
    lv_obj_set_size(plus_btn, 34, 26);
    lv_obj_align(plus_btn, LV_ALIGN_BOTTOM_RIGHT, -4, 0);
    lv_obj_set_style_bg_color(plus_btn, THEME_BTN_PLUS, 0);
    lv_obj_add_event_cb(plus_btn, plus_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)idx);
    lv_obj_t *plus_lbl = lv_label_create(plus_btn);
    lv_label_set_text(plus_lbl, "+");
    lv_obj_set_style_text_font(plus_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(plus_lbl);
#endif
}

#if BOARD_HAS_ENCODER
static void encoder_handler(input_encoder_event_t evt, int16_t data) {
    if (dice_overlay_is_open()) {
        /* While the dice overlay is up, the knob controls it instead. */
        if (evt == INPUT_ENC_CLICK) dice_overlay_handle_roll();
        if (evt == INPUT_ENC_LONG_PRESS) dice_overlay_close();
        return;
    }
    if (g_game.current_screen != SCREEN_COUNTER) return;
    if (!s_screen) return; /* belt-and-suspenders: see counter_screen_refresh() */

    switch (evt) {
        case INPUT_ENC_TURN:
            app_state_adjust_life(g_game.selected_player, data);
            counter_screen_refresh();
            break;
        case INPUT_ENC_CLICK:
            app_state_select_next_player();
            counter_screen_refresh();
            /* If "Turn Colors" is on (menu screen), the ambient LEDs
             * switch to the new player's assigned color. A harmless
             * no-op call otherwise (led_driver_refresh() checks both
             * led_driver_available() and whether turn colors are on). */
            led_driver_refresh();
            break;
        case INPUT_ENC_LONG_PRESS:
            dice_overlay_open();
            break;
    }
}
#endif

void counter_screen_create() {
    counter_screen_create_anim(LV_SCR_LOAD_ANIM_NONE);
}

void counter_screen_create_anim(lv_scr_load_anim_t anim) {
    g_game.current_screen = SCREEN_COUNTER;
    nav_home_set(nullptr);   /* already home */
    s_built_for_player_count = g_game.player_count;

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, THEME_BG, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
        s_containers[i] = nullptr;
        s_life_labels[i] = nullptr;
        s_status_labels[i] = nullptr;
    }

#if defined(BOARD_WAVESHARE_S3)
    s_ring = nullptr;
    s_hero_disc = nullptr;
    s_hero_title = nullptr;
    s_delta_label = nullptr;
    s_watermark = nullptr;
    if (g_game.player_count == 1) {
        /* Life ring hugging the round bezel; drawn before the player
         * widget so the inner disc sits on top of its inner edge. */
        lv_obj_t *ring = lv_arc_create(s_screen);
        lv_obj_set_size(ring, 352, 352);
        lv_obj_center(ring);
        lv_arc_set_rotation(ring, 270);
        lv_arc_set_bg_angles(ring, 0, 360);
        lv_obj_remove_style(ring, NULL, LV_PART_KNOB);
        lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(ring, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_set_style_arc_width(ring, 18, LV_PART_MAIN);
        lv_obj_set_style_arc_width(ring, 18, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(ring, theme_track(), LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(ring, true, LV_PART_INDICATOR);
        s_ring = ring;
        s_last_life = g_game.players[0].life;
        s_delta_accum = 0;
        if (!s_delta_timer) {
            s_delta_timer = lv_timer_create(hero_delta_timer_cb, HERO_DELTA_MS, nullptr);
        }
        lv_timer_pause(s_delta_timer);
    }
#endif

    for (uint8_t i = 0; i < g_game.player_count; i++) {
        build_player_widget(i);
    }

    /* Swipe left anywhere on the background to reach the menu screen (d20
     * roller, settings, extra-counter toggles now live there). */
    lv_obj_add_event_cb(s_screen, screen_gesture_cb, LV_EVENT_GESTURE, nullptr);

#if BOARD_HAS_ENCODER
    input_driver_set_encoder_handler(encoder_handler);
    Serial.printf("[counter] encoder handler registered; players=%u life=%d\n",
                  g_game.player_count, g_game.players[0].life);
#endif

    counter_screen_refresh();
#if defined(BOARD_WAVESHARE_S3)
    hero_apply_theme();
#endif
    /* Make sure the ambient LEDs match whoever's turn it is (or the
     * chosen ambient color) as soon as this screen becomes active —
     * covers New Game resetting selected_player back to 0, and coming
     * back from the menu after changing LED settings there. */
    led_driver_refresh();
    if (anim == LV_SCR_LOAD_ANIM_NONE) {
        lv_scr_load(s_screen);
    } else {
        /* Slide in; LVGL deletes the outgoing screen when the slide ends,
         * so the caller must use its module's release(), not destroy(). */
        lv_scr_load_anim(s_screen, anim, SCREEN_SLIDE_MS, 0, true);
    }
}

static void counter_teardown(bool delete_screen);

void counter_screen_destroy() { counter_teardown(true); }
void counter_screen_release() { counter_teardown(false); }

static void counter_teardown(bool delete_screen) {
    /* The dice roller and the player-detail view are both created as
     * children of this screen (lv_obj_create(lv_scr_act())). If either is
     * still open when this screen gets torn down (e.g. the user swiped to
     * the menu without closing it first), deleting s_screen below deletes
     * their widgets right along with it — but their own modules don't
     * know that unless we tell them. Without this, dice_overlay_is_open()
     * / player_detail_overlay_is_open() would keep reporting "open"
     * forever, pointing at freed memory, and the next encoder click or
     * touch routed into them reads through a dangling pointer. This is
     * exactly what was crashing before this fix. */
    dice_overlay_discard();
    player_detail_overlay_discard();

    if (s_screen) {
        /* Async delete — see the matching comment in setup_screen.cpp. Call
         * this only after a replacement screen has already been loaded. */
        if (delete_screen) lv_obj_del_async(s_screen);
        s_screen = nullptr;
    }
    /* Clear every cached child-widget pointer along with it. These were
     * only ever valid for the lifetime of the s_screen tree they belong
     * to; leaving them set after that tree is gone means any late-firing
     * event (a queued encoder click, an in-flight touch release, an
     * async callback that hasn't run yet) reads dangling pointers into
     * freed heap memory instead of getting a clean "nothing to do here". */
    for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
        s_containers[i] = nullptr;
        s_life_labels[i] = nullptr;
        s_status_labels[i] = nullptr;
    }
#if defined(BOARD_WAVESHARE_S3)
    hero_commit_pending();   /* don't lose a change still on the badge */
    s_ring = nullptr;
    s_hero_disc = nullptr;
    s_hero_title = nullptr;
    s_delta_label = nullptr;
    s_watermark = nullptr;
    if (s_delta_timer) lv_timer_pause(s_delta_timer);
#endif
}

void counter_screen_rebuild_if_active() {
    if (!s_screen) return;

    if (s_built_for_player_count == g_game.player_count) {
        counter_screen_refresh();
        return;
    }

    /* Player count changed out from under the built layout (a remote New
     * Game with a different count, applied via table sync) — the existing
     * boxes are positioned/sized for the old count, so rebuild instead of
     * just re-labeling. Same create-before-destroy ordering as every other
     * screen transition here: build + load the replacement first, then
     * async-delete the old tree. */
    lv_obj_t *old_screen = s_screen;
    s_screen = nullptr; /* so counter_screen_create() below starts clean */
    dice_overlay_discard();
    player_detail_overlay_discard();

    counter_screen_create();

    lv_obj_del_async(old_screen);
}
