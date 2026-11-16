#include "theme.h"
#include <Preferences.h>

static void save_index();

struct theme_preset_t {
    const char *name;
    uint32_t accent_hex;
    uint32_t bg_top_hex;
    uint32_t bg_bottom_hex;
    uint32_t healthy_hex;
    uint32_t track_hex;
};

static const theme_preset_t PRESETS[THEME_PRESET_COUNT] = {
    /* name       accent    bg top    bg bottom healthy   ring track */
    {"Emerald", 0x2ee59d, 0x123026, 0x05100c, 0x2ee59d, 0x1d3a30},
    {"Azure",   0x4cb4ff, 0x10223f, 0x050a16, 0x4cb4ff, 0x1b2b45},
    {"Arcane",  0xc08bff, 0x25163a, 0x0a0613, 0xc08bff, 0x2e2340},
    {"Gold",    0xf2c14e, 0x2e2410, 0x0e0a04, 0xf2c14e, 0x3a3020},
    {"Frost",   0xbfe9ff, 0x1c2a33, 0x080c10, 0x9fe6ff, 0x26343d},
};

/* Chosen theme survives power cycles (NVS namespace "mtg", key "theme"). */
static uint8_t s_theme_index = 0;
static bool s_loaded = false;

static const theme_preset_t &current() {
    if (!s_loaded) {
        s_loaded = true;
        Preferences prefs;
        if (prefs.begin("mtg", true)) {
            s_theme_index = prefs.getUChar("theme", 0);
            prefs.end();
        }
        if (s_theme_index >= THEME_PRESET_COUNT) s_theme_index = 0;
    }
    return PRESETS[s_theme_index];
}

lv_color_t theme_accent()    { return lv_color_hex(current().accent_hex); }
lv_color_t theme_bg_top()    { return lv_color_hex(current().bg_top_hex); }
lv_color_t theme_bg_bottom() { return lv_color_hex(current().bg_bottom_hex); }
lv_color_t theme_healthy()   { return lv_color_hex(current().healthy_hex); }
lv_color_t theme_track()     { return lv_color_hex(current().track_hex); }

const char *theme_cycle() {
    current();
    s_theme_index = (s_theme_index + 1) % THEME_PRESET_COUNT;
    save_index();
    return PRESETS[s_theme_index].name;
}

const char *theme_name() {
    return current().name;
}

static void save_index() {
    Preferences prefs;
    if (prefs.begin("mtg", false)) {
        prefs.putUChar("theme", s_theme_index);
        prefs.end();
    }
}

uint8_t theme_index() {
    current();
    return s_theme_index;
}

void theme_set(uint8_t index, bool persist) {
    current();
    if (index >= THEME_PRESET_COUNT) return;
    s_theme_index = index;
    if (persist) save_index();
}

const char *theme_preset_name(uint8_t index) {
    return PRESETS[index % THEME_PRESET_COUNT].name;
}

lv_color_t theme_preset_accent(uint8_t index) {
    return lv_color_hex(PRESETS[index % THEME_PRESET_COUNT].accent_hex);
}

lv_color_t theme_preset_bg_top(uint8_t index) {
    return lv_color_hex(PRESETS[index % THEME_PRESET_COUNT].bg_top_hex);
}
