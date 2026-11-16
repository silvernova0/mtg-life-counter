#include "life_history.h"
#include <Arduino.h>

static life_history_entry_t s_entries[LIFE_HISTORY_LEN];
static uint8_t s_head = 0;    /* next write slot */
static uint8_t s_count = 0;
static uint32_t s_game_start_ms = 0;
static int16_t s_starting_life = 0;

void life_history_clear(int16_t starting_life) {
    s_head = 0;
    s_count = 0;
    s_game_start_ms = millis();
    s_starting_life = starting_life;
}

void life_history_push(int16_t delta, int16_t life) {
    if (delta == 0) return;
    s_entries[s_head].t_ms = millis() - s_game_start_ms;
    s_entries[s_head].delta = delta;
    s_entries[s_head].life = life;
    s_head = (s_head + 1) % LIFE_HISTORY_LEN;
    if (s_count < LIFE_HISTORY_LEN) s_count++;
}

uint8_t life_history_count() {
    return s_count;
}

const life_history_entry_t *life_history_get(uint8_t newest_idx) {
    if (newest_idx >= s_count) return nullptr;
    uint8_t slot = (s_head + LIFE_HISTORY_LEN - 1 - newest_idx) % LIFE_HISTORY_LEN;
    return &s_entries[slot];
}

int16_t life_history_starting_life() {
    return s_starting_life;
}
