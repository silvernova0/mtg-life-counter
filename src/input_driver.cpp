#include "input_driver.h"
#include "ui/nav_home.h"
#include "display_driver.h"
#include <esp_timer.h>
#include "backlight.h"

#if BOARD_HAS_TOUCH
static lv_indev_drv_t touch_indev_drv;

static void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    lgfx::touch_point_t tp;
    if (gfx.getTouch(&tp)) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = tp.x;
        data->point.y = tp.y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
#endif

#if BOARD_HAS_ENCODER
static volatile int32_t s_enc_raw_count = 0;
static int32_t s_enc_last_reported = 0;
static volatile uint8_t s_enc_last_state = 0;

#if defined(BOARD_WAVESHARE_S3)
#endif

#if defined(BOARD_WAVESHARE_S3)
/* The Waveshare knob's contacts chatter heavily (thousands of raw edges
 * while resting at some detents), so edge interrupts are useless here.
 * Instead a 1 ms esp_timer samples both lines; a line's level is only
 * accepted after it has held for ENC_STABLE_SAMPLES consecutive samples.
 * Debounced state changes are decoded as switch pulses. */
static const uint8_t ENC_STABLE_SAMPLES = 4;
static esp_timer_handle_t s_enc_timer = nullptr;
static uint8_t s_enc_cand_state = 0;
static uint8_t s_enc_cand_count = 0;


static void encoder_sample_cb(void *) {
    uint8_t raw = (digitalRead(PIN_ENCODER_A) << 1) | digitalRead(PIN_ENCODER_B);
    if (raw != s_enc_cand_state) {
        s_enc_cand_state = raw;
        s_enc_cand_count = 0;
        return;
    }
    if (s_enc_cand_count < ENC_STABLE_SAMPLES) {
        s_enc_cand_count++;
        if (s_enc_cand_count < ENC_STABLE_SAMPLES) return;
    } else {
        return;  /* already accepted */
    }
    if (raw == s_enc_last_state) return;
    /* Bidirectional switch knob (confirmed from the [knob] log): turning
     * clockwise pulses only B (GPIO7) low, counter-clockwise pulses only
     * A (GPIO8) low; one debounced low pulse == one detent. */
    uint8_t fell = s_enc_last_state & ~raw;
    /* Signs confirmed on hardware: B pulses must decrease, A increase. */
    if (fell & 0x01) s_enc_raw_count = s_enc_raw_count - 1;  /* B (GPIO7) */
    if (fell & 0x02) s_enc_raw_count = s_enc_raw_count + 1;  /* A (GPIO8) */
    s_enc_last_state = raw;
}
#else

static void IRAM_ATTR encoder_isr() {
    uint8_t a = digitalRead(PIN_ENCODER_A);
    uint8_t b = digitalRead(PIN_ENCODER_B);
    uint8_t state = (a << 1) | b;
    static const int8_t transition_table[16] = {
        0, -1, 1, 0,
        1, 0, 0, -1,
        -1, 0, 0, 1,
        0, 1, -1, 0
    };
    uint8_t idx = (s_enc_last_state << 2) | state;
    s_enc_raw_count += transition_table[idx & 0x0F];
    s_enc_last_state = state;
}
#endif

static input_encoder_cb_t s_enc_cb = nullptr;
#if BOARD_HAS_ENCODER_BUTTON
static bool s_btn_down = false;
static uint32_t s_btn_down_at = 0;
static bool s_long_press_fired = false;

/* Debounce: a mechanical/tactile encoder button's contacts chatter for a
 * few ms on every press and release. Without filtering that out, a single
 * physical click can be read as press-release-press-release in the same
 * loop() burst, firing several INPUT_ENC_CLICK events for what the user
 * felt as one click — most noticeable right after a long-press (e.g.
 * going back from the menu screen), where a bounce on release can look
 * like an extra quick click immediately afterward. Require the raw
 * reading to hold steady for BTN_DEBOUNCE_MS before it's trusted. */
#define BTN_DEBOUNCE_MS 25
static bool s_btn_raw_candidate = false;
static uint32_t s_btn_candidate_since = 0;
#endif
#endif

void input_driver_init() {
#if BOARD_HAS_TOUCH
    lv_indev_drv_init(&touch_indev_drv);
    touch_indev_drv.type = LV_INDEV_TYPE_POINTER;
    touch_indev_drv.read_cb = touch_read_cb;
#if defined(BOARD_WAVESHARE_S3)
    touch_indev_drv.feedback_cb = nav_home_feedback;   /* double-tap = home */
#endif
    lv_indev_drv_register(&touch_indev_drv);
#endif

#if BOARD_HAS_ENCODER
    pinMode(PIN_ENCODER_A, INPUT_PULLUP);
    pinMode(PIN_ENCODER_B, INPUT_PULLUP);
    s_enc_last_state = (digitalRead(PIN_ENCODER_A) << 1) | digitalRead(PIN_ENCODER_B);
#if BOARD_HAS_ENCODER_BUTTON
    pinMode(PIN_ENCODER_BTN, INPUT_PULLUP);
#endif
#if defined(BOARD_WAVESHARE_S3)
    s_enc_cand_state = s_enc_last_state;
    s_enc_cand_count = ENC_STABLE_SAMPLES;
    {
        esp_timer_create_args_t args = {};
        args.callback = encoder_sample_cb;
        args.name = "knob";
        esp_timer_create(&args, &s_enc_timer);
        esp_timer_start_periodic(s_enc_timer, 1000);
    }
#else
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_A), encoder_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_B), encoder_isr, CHANGE);
#endif
#if defined(BOARD_WAVESHARE_S3)
    Serial.printf("[encoder] interrupt decoding GPIO%d/GPIO%d; idle levels A=%d B=%d\n",
                  PIN_ENCODER_A, PIN_ENCODER_B,
                  digitalRead(PIN_ENCODER_A), digitalRead(PIN_ENCODER_B));
#endif
#endif
}

void input_driver_set_encoder_handler(input_encoder_cb_t cb) {
#if BOARD_HAS_ENCODER
    s_enc_cb = cb;
#else
    (void)cb;
#endif
}

void input_driver_poll() {
#if BOARD_HAS_ENCODER
    /* Quadrature ISR increments by 4 per physical detent on most
     * encoders — divide down so one detent == one step. Adjust
     * ENC_STEPS_PER_DETENT if your knob feels 2x/4x too sensitive. */
#if defined(BOARD_WAVESHARE_S3)
    const int32_t ENC_STEPS_PER_DETENT = 1;  /* one low pulse per detent */
#else
    const int32_t ENC_STEPS_PER_DETENT = 4;
#endif
    int32_t raw = s_enc_raw_count;
    int32_t detents = (raw - s_enc_last_reported) / ENC_STEPS_PER_DETENT;
    if (detents != 0) {
        s_enc_last_reported += detents * ENC_STEPS_PER_DETENT;
        backlight_note_activity();
        if (s_enc_cb) s_enc_cb(INPUT_ENC_TURN, (int16_t)detents);
    }

#if BOARD_HAS_ENCODER_BUTTON
    uint32_t now = millis();
    bool raw_pressed = (digitalRead(PIN_ENCODER_BTN) == LOW);

    /* Debounce stage: only let a raw transition through once it's been
     * stable for BTN_DEBOUNCE_MS. */
    if (raw_pressed != s_btn_raw_candidate) {
        s_btn_raw_candidate = raw_pressed;
        s_btn_candidate_since = now;
    }
    bool pressed = s_btn_down; /* debounced state carried across calls */
    if (s_btn_raw_candidate != s_btn_down &&
        (now - s_btn_candidate_since) >= BTN_DEBOUNCE_MS) {
        pressed = s_btn_raw_candidate;
    }

    if (pressed && !s_btn_down) {
        s_btn_down = true;
        s_btn_down_at = now;
        s_long_press_fired = false;
    } else if (pressed && s_btn_down) {
        if (!s_long_press_fired && (now - s_btn_down_at) >= INPUT_LONG_PRESS_MS) {
            s_long_press_fired = true;
            if (s_enc_cb) s_enc_cb(INPUT_ENC_LONG_PRESS, 0);
        }
    } else if (!pressed && s_btn_down) {
        s_btn_down = false;
        if (!s_long_press_fired) {
            if (s_enc_cb) s_enc_cb(INPUT_ENC_CLICK, 0);
        }
    }
#endif
#endif
}
