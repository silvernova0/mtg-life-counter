/**
 * theme_picker_screen.h — color theme list for the Waveshare one-player
 * layout. Reached by swiping right on the counter screen. Turning the
 * knob moves the highlight and previews that theme live; tapping a theme
 * (or swiping back left) keeps it and slides back to the counter.
 */

#pragma once

#include <lvgl.h>

void theme_picker_create(lv_scr_load_anim_t anim);
/* Tear down without deleting (when a slide animation auto-deletes it). */
void theme_picker_release();
