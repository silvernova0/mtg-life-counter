/**
 * life_history.h — rolling log of life-total changes for the current game.
 *
 * The counter screen groups quick consecutive dial turns into a single
 * entry (e.g. five clicks down in a row is one "-5"), then pushes it here.
 * app_state_reset_game() clears the log and restarts the game clock.
 */

#pragma once

#include <stdint.h>

#define LIFE_HISTORY_LEN 64

typedef struct {
    uint32_t t_ms;   /* milliseconds since the game started */
    int16_t delta;   /* net change for this entry */
    int16_t life;    /* life total after the change */
} life_history_entry_t;

void life_history_clear(int16_t starting_life);
void life_history_push(int16_t delta, int16_t life);

/* Number of stored entries (oldest are dropped past LIFE_HISTORY_LEN). */
uint8_t life_history_count();

/* 0 = newest. Returns nullptr when out of range. */
const life_history_entry_t *life_history_get(uint8_t newest_idx);

int16_t life_history_starting_life();
