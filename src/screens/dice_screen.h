/**
 * dice_screen.h — full-screen d20 roller for the Waveshare knob. Opened
 * from the menu's "Roll d20". Tap the screen or turn the dial to roll;
 * swipe in any direction to go back to the life counter.
 */

#pragma once

#include <lvgl.h>

void dice_screen_create();
/* Tear down without deleting (a slide/fade animation auto-deletes it). */
void dice_screen_release();
