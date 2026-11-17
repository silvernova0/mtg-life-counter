/**
 * nav_home.h — double-tap the background of any non-life screen to jump
 * back to the life counter (Waveshare knob).
 *
 * Each screen registers the function it already uses to return to the
 * life counter (the same one its swipe calls); the life screen registers
 * nullptr. Taps on anything that handles taps itself (buttons, rows, the
 * brightness ring) never count, so a double tap on e.g. the history RESET
 * button can't confirm it and leave.
 */

#pragma once

#include <lvgl.h>

typedef void (*nav_home_fn_t)();

void nav_home_set(nav_home_fn_t go_home);
/* Hook for the touch input driver's feedback_cb. */
void nav_home_feedback(lv_indev_drv_t *drv, uint8_t code);
