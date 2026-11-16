/**
 * theme.h — shared colors/sizing so every screen looks consistent.
 *
 * Most colors are static #defines — only the accent color is a runtime
 * "theme", cyclable from the menu screen, since a full recolor engine is
 * overkill for a life counter and a single accent swap already gives a
 * meaningfully different look.
 */

#pragma once

#include <lvgl.h>

#define THEME_BG            lv_color_hex(0x101014)
#define THEME_PANEL         lv_color_hex(0x1c1c22)
#define THEME_LIFE_TEXT     lv_color_hex(0xf5f5f0)
#define THEME_DANGER        lv_color_hex(0xb23a2f)
#define THEME_SELECTED_RING lv_color_hex(0xffffff)
#define THEME_POISON        lv_color_hex(0x4caf50)
#define THEME_ENERGY        lv_color_hex(0x29b6f6)

#define THEME_BTN_PLUS      lv_color_hex(0x2f6b3a)
#define THEME_BTN_MINUS     lv_color_hex(0x6b2f2f)

#define THEME_PRESET_COUNT 5

/* Life-ring states that stay fixed across themes, so "hurt" and
 * "critical" always read the same no matter which palette is active. */
#define THEME_LIFE_HURT     lv_color_hex(0xff9a2e)
#define THEME_LIFE_CRITICAL lv_color_hex(0xff3b3b)
#define THEME_GAIN          lv_color_hex(0x2ee59d)
#define THEME_LOSS          lv_color_hex(0xff5a3c)

/* Bold Montserrat fonts generated with lv_font_conv (tools/gen_fonts.sh). */
LV_FONT_DECLARE(font_life_110);   /* digits and '-' only */
LV_FONT_DECLARE(font_bold_24);    /* printable ASCII */
LV_FONT_DECLARE(font_bold_32);    /* printable ASCII */

/* Current accent color — replaces the old THEME_ACCENT macro. Every
 * screen that used to write THEME_ACCENT now calls theme_accent(). */
lv_color_t theme_accent();

/* Extra palette entries used by the Waveshare one-player screen. */
lv_color_t theme_bg_top();      /* inner disc gradient, top */
lv_color_t theme_bg_bottom();   /* inner disc gradient, bottom + screen bg */
lv_color_t theme_healthy();     /* life ring/number while life >= 60% */
lv_color_t theme_track();       /* unfilled part of the life ring */

/* Advances to the next preset (wraps around) and returns its name. */
const char *theme_cycle();

/* Name of the currently active preset, for the settings screen label. */
const char *theme_name();

/* Direct selection, for the knob's theme picker. theme_set() applies the
 * preset right away; pass persist=false while previewing and true once
 * the choice is final (saves to flash, so avoid it on every dial click). */
uint8_t theme_index();
void theme_set(uint8_t index, bool persist);
const char *theme_preset_name(uint8_t index);
lv_color_t theme_preset_accent(uint8_t index);
lv_color_t theme_preset_bg_top(uint8_t index);
