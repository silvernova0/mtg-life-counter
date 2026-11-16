/**
 * counter_screen.h — the main life-total screen. Layout adapts to
 * g_game.player_count (1-4), set on the setup screen.
 */

#pragma once

#include <lvgl.h>

/* Duration of the sideways slide between the knob's one-player screens. */
#define SCREEN_SLIDE_MS 250

void counter_screen_create();
/* Same, but slides the new screen in with `anim`. LVGL then deletes the
 * previous screen itself when the slide ends, so tear the previous
 * screen's module down with its *_release() instead of *_destroy(). */
void counter_screen_create_anim(lv_scr_load_anim_t anim);
void counter_screen_destroy();
/* Tear down without deleting the LVGL screen (see create_anim above). */
void counter_screen_release();

/* Refreshes all life labels from g_game — call after any adjustment made
 * outside a widget event (e.g. from the encoder handler). */
void counter_screen_refresh();

/* Call after table-sync applies a remote state/action update. No-op if
 * this screen isn't currently active. If g_game.player_count still
 * matches what's built, this is just counter_screen_refresh(); if a
 * remote New Game changed the player count out from under the built
 * layout, it does a full safe rebuild (new screen created and loaded,
 * then the old one async-deleted) instead. */
void counter_screen_rebuild_if_active();
