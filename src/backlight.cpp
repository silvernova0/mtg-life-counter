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

/* Fast CPU only while something is happening. At 80 MHz the S3 draws
 * noticeably less, and the APB bus stays at 80 MHz either way, so the
 * display SPI, backlight PWM and USB keep running unchanged. Input is
 * acted on in the same loop pass that raises the clock, so the redraw
 * after a dial click or tap already runs at full speed. */
#define CPU_FAST_MHZ        240
#define CPU_IDLE_MHZ        80
#define CPU_IDLE_AFTER_MS   3000

static bool s_cpu_slow = false;

bool backlight_cpu_is_slow() { return s_cpu_slow; }

static void cpu_scale(uint32_t idle_ms) {
#if BOARD_CPU_SCALING
    /* Animations (slides, dice tumble, scrolling) still want full speed. */
    bool busy = idle_ms < CPU_IDLE_AFTER_MS || lv_anim_count_running() > 0;
    if (busy && s_cpu_slow) {
        setCpuFrequencyMhz(CPU_FAST_MHZ);
        s_cpu_slow = false;
    } else if (!busy && !s_cpu_slow) {
        setCpuFrequencyMhz(CPU_IDLE_MHZ);
        s_cpu_slow = true;
    }
#else
    (void)idle_ms;
#endif
}

void backlight_poll() {
    /* Touch goes through LVGL, which tracks its own inactivity time. */
    uint32_t touch_idle = lv_disp_get_inactive_time(NULL);
    uint32_t dial_idle = millis() - s_last_activity_ms;
    uint32_t idle = touch_idle < dial_idle ? touch_idle : dial_idle;

    cpu_scale(idle);

#if BOARD_IDLE_DIM
    if (!s_dimmed && idle >= BACKLIGHT_IDLE_DIM_MS) {
        s_dimmed = true;
        apply(s_percent < BACKLIGHT_DIM_PERCENT ? s_percent : BACKLIGHT_DIM_PERCENT);
    } else if (s_dimmed && idle < BACKLIGHT_IDLE_DIM_MS) {
        s_dimmed = false;
        apply(s_percent);
    }
#endif
}
