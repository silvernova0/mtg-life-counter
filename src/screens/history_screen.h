/**
 * history_screen.h — scrollable log of life changes for the current game
 * (Waveshare one-player layout). Reached by swiping left on the counter
 * screen; swipe right to return. Turning the knob scrolls the list.
 */

#pragma once

#include <lvgl.h>

void history_screen_create(lv_scr_load_anim_t anim);
void history_screen_destroy();
/* Tear down without deleting (when a slide animation auto-deletes it). */
void history_screen_release();
