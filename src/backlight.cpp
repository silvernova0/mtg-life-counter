#include "backlight.h"
#include "display_driver.h"
#include "board_config.h"
#include <Arduino.h>
#include <Preferences.h>
#include <lvgl.h>

static uint8_t s_percent = 80;
static bool s_dimmed = false;
static uint32_t s_last_activity_ms = 0;

static void apply(uint8_t percent) {
    /* Squared curve: perceived brightness is far from linear in PWM duty. */
    uint32_t pwm = (uint32_t)percent * percent * 255 / 10000;
    if (percent > 0 && pwm < 2) pwm = 2;
    gfx.setBrightness((uint8_t)pwm);
}

static uint8_t clamp_percent(int v) {
    if (v < BACKLIGHT_MIN_PERCENT) return BACKLIGHT_MIN_PERCENT;
    if (v > 100) return 100;
    return (uint8_t)v;
}

void backlight_init() {
    Preferences prefs;
    if (prefs.begin("mtg", true)) {
        s_percent = clamp_percent(prefs.getUChar("bright", 80));
        prefs.end();
    }
    s_dimmed = false;
    s_last_activity_ms = millis();
    apply(s_percent);
}

uint8_t backlight_get_percent() {
    return s_percent;
}

void backlight_set_percent(uint8_t percent, bool persist) {
    s_percent = clamp_percent(percent);
    s_dimmed = false;
    s_last_activity_ms = millis();
    apply(s_percent);
    if (persist) {
        Preferences prefs;
        if (prefs.begin("mtg", false)) {
            prefs.putUChar("bright", s_percent);
            prefs.end();
        }
    }
}

void backlight_note_activity() {
    s_last_activity_ms = millis();
    if (s_dimmed) {
        s_dimmed = false;
        apply(s_percent);
    }
}

void backlight_poll() {
#if BOARD_IDLE_DIM
    /* Touch goes through LVGL, which tracks its own inactivity time. */
    uint32_t touch_idle = lv_disp_get_inactive_time(NULL);
    uint32_t dial_idle = millis() - s_last_activity_ms;
    uint32_t idle = touch_idle < dial_idle ? touch_idle : dial_idle;

    if (!s_dimmed && idle >= BACKLIGHT_IDLE_DIM_MS) {
        s_dimmed = true;
        apply(s_percent < BACKLIGHT_DIM_PERCENT ? s_percent : BACKLIGHT_DIM_PERCENT);
    } else if (s_dimmed && idle < BACKLIGHT_IDLE_DIM_MS) {
        s_dimmed = false;
        apply(s_percent);
    }
#endif
}
