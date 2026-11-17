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
    uint32_t ring2_hex;       /* second ring color; 0 = one-color ring */
    const char *watermark;    /* glyph in font_guild_240, or nullptr */
};

static const theme_preset_t PRESETS[THEME_PRESET_COUNT] = {
    /* name      accent    bg top    bg bottom ring/num  ring track ring2   glyph */
    /* Guilds: ring blends between the two colors, emblem behind the life.
     * Emblem glyphs are from the Mana font (U+E915 = Simic). */
    {"Simic",   0x2fd6a8, 0x0c2b2a, 0x04100f, 0x3ccf5a, 0x173a38, 0x2f9bff, "\xEE\xA4\x95"},
    /* Ring/number color first (the brighter of the pair), then the color
     * it blends to. "Black" is a cool violet-gray so it shows on the dark
     * background. Glyphs: U+E90C..E914 in font_guild_240. */
    {"Azorius",  0x8fc4ff, 0x15243a, 0x060a12, 0xf4efd6, 0x26344a, 0x3d9bff, "\xEE\xA4\x8C"},
    {"Dimir",    0x5aa0ff, 0x161a33, 0x07070f, 0x3d8bff, 0x24263e, 0x8c7fa0, "\xEE\xA4\x8E"},
    {"Rakdos",   0xff4a4a, 0x2e1018, 0x0e0508, 0xe8303a, 0x3c1e26, 0x8c7fa0, "\xEE\xA4\x93"},
    {"Gruul",    0xff7a3a, 0x2e2012, 0x0e0905, 0xff5a2e, 0x3c2c1e, 0x3ccf5a, "\xEE\xA4\x90"},
    {"Selesnya", 0x9be37a, 0x1e2e18, 0x080e06, 0x5ad35a, 0x2c3c26, 0xf4efd6, "\xEE\xA4\x94"},
    {"Orzhov",   0xe6dcc0, 0x26222c, 0x0b0a0e, 0xf4efd6, 0x36323c, 0x8c7fa0, "\xEE\xA4\x92"},
    {"Izzet",    0x6aa8ff, 0x2a1630, 0x0c060f, 0x3d9bff, 0x36243e, 0xff4a3a, "\xEE\xA4\x91"},
    {"Golgari",  0x8fbf4a, 0x1c2414, 0x080a06, 0x7fbf4a, 0x2a3220, 0x8c7fa0, "\xEE\xA4\x8F"},
    {"Boros",    0xff6a4a, 0x2e1a12, 0x0e0805, 0xff4a3a, 0x3c2820, 0xf4efd6, "\xEE\xA4\x8D"},
    /* Basic lands: one-color ring, mana symbol (U+E600..E604) behind. */
    {"Plains",   0xf4efd6, 0x2e2a1e, 0x0e0c08, 0xf4efd6, 0x3e3a2c, 0, "\xEE\x98\x80"},
    {"Island",   0x3d9bff, 0x0f2240, 0x050a16, 0x3d9bff, 0x1b2b45, 0, "\xEE\x98\x81"},
    {"Swamp",    0xb7a6d6, 0x221c2c, 0x09070d, 0xb7a6d6, 0x302a3a, 0, "\xEE\x98\x82"},
    {"Mountain", 0xff5a3c, 0x3a1410, 0x120605, 0xff5a3c, 0x4a2420, 0, "\xEE\x98\x83"},
    {"Forest",   0x4fd15a, 0x12301a, 0x051008, 0x4fd15a, 0x1e3c26, 0, "\xEE\x98\x84"},
};

/* Chosen theme survives power cycles (NVS namespace "mtg", key "theme2").
 * The original key "theme" indexed a list that began with nine plain
 * color presets (since removed); a value from it is moved over once. */
#define OLD_PLAIN_PRESETS 9

static uint8_t s_theme_index = 0;
static bool s_loaded = false;

static const theme_preset_t &current() {
    if (!s_loaded) {
        s_loaded = true;
        Preferences prefs;
        if (prefs.begin("mtg", true)) {
            uint8_t v = prefs.getUChar("theme2", 0xFF);
            if (v == 0xFF) {
                uint8_t old = prefs.getUChar("theme", 0);
                v = old >= OLD_PLAIN_PRESETS ? old - OLD_PLAIN_PRESETS : 0;
                prefs.end();
                s_theme_index = v < THEME_PRESET_COUNT ? v : 0;
                save_index();
            } else {
                prefs.end();
                s_theme_index = v;
            }
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

bool theme_ring_second(lv_color_t *out) {
    uint32_t c = current().ring2_hex;
    if (c == 0) return false;
    if (out) *out = lv_color_hex(c);
    return true;
}

const char *theme_watermark() { return current().watermark; }

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
        prefs.putUChar("theme2", s_theme_index);
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

/* Second ring color of a guild preset, for the picker's swatch. */
bool theme_preset_ring2(uint8_t index, lv_color_t *a, lv_color_t *b) {
    const theme_preset_t &p = PRESETS[index % THEME_PRESET_COUNT];
    if (p.ring2_hex == 0) return false;
    if (a) *a = lv_color_hex(p.healthy_hex);
    if (b) *b = lv_color_hex(p.ring2_hex);
    return true;
}
