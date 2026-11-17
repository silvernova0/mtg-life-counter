/**
 * counters_screen.h — extra counters for the Waveshare knob's one-player
 * layout: commander damage from up to three opponents, poison, energy,
 * experience and storm. Only the ones switched on in the menu are listed.
 *
 * Opened by tapping the life total. Tap a row to pick it, turn the dial
 * to change it, long-press a row to zero it. Swipe to go back.
 */

#pragma once

#include <lvgl.h>

void counters_screen_create();
/* Tear down without deleting (a slide/fade animation auto-deletes it). */
void counters_screen_release();
