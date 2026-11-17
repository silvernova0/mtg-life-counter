/**
 * brightness_screen.h — backlight control for the Waveshare one-player
 * layout. Reached by swiping up on the counter screen; swipe down to
 * return. Turn the knob or drag the ring; the level is saved on exit.
 */

#pragma once

#include <lvgl.h>

void brightness_screen_create(lv_scr_load_anim_t anim);
/* Tear down without deleting (when a slide animation auto-deletes it). */
void brightness_screen_release();
