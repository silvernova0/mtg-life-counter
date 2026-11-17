/**
 * backlight.h — display brightness, saved across power cycles, plus idle
 * dimming on boards that set BOARD_IDLE_DIM (battery-powered knob).
 *
 * Brightness is a 0-100 "percent" that maps onto the PWM with a squared
 * curve, so equal steps look roughly equal to the eye.
 */

#pragma once

#include <stdint.h>

#define BACKLIGHT_MIN_PERCENT   5
#define BACKLIGHT_IDLE_DIM_MS   60000   /* no dial/touch input for 1 minute */
#define BACKLIGHT_DIM_PERCENT   8       /* level while dimmed (or lower, if the user's is) */

void backlight_init();
/* Call from loop(): dims after BACKLIGHT_IDLE_DIM_MS without input. */
void backlight_poll();
/* Any user input (dial turns; touch is picked up from LVGL automatically). */
void backlight_note_activity();

uint8_t backlight_get_percent();
/* Applies right away; persist=true also saves it to flash (avoid on every
 * dial click — save once when the user is done adjusting). */
void backlight_set_percent(uint8_t percent, bool persist);
