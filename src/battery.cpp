#include "battery.h"
#include "board_config.h"
#include <Arduino.h>

#if BOARD_HAS_BATTERY_ADC

#define SAMPLE_EVERY_MS 2000

static uint16_t s_pin_mv = 0;
static uint32_t s_cell_mv_filt = 0;   /* x16 fixed point */
static uint32_t s_last_ms = 0;

static uint16_t read_pin_mv() {
    uint32_t sum = 0;
    for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(PIN_BATTERY_ADC);
    return (uint16_t)(sum / 8);
}

static void sample() {
    s_pin_mv = read_pin_mv();
    uint32_t cell = (uint32_t)s_pin_mv * BATTERY_DIVIDER_NUM / BATTERY_DIVIDER_DEN;
    if (s_cell_mv_filt == 0) s_cell_mv_filt = cell << 4;
    else s_cell_mv_filt += ((int32_t)(cell << 4) - (int32_t)s_cell_mv_filt) / 4;
}

void battery_init() {
    analogSetPinAttenuation(PIN_BATTERY_ADC, ADC_11db);
    sample();
    s_last_ms = millis();
}

void battery_poll() {
    uint32_t now = millis();
    if (now - s_last_ms < SAMPLE_EVERY_MS) return;
    s_last_ms = now;
    sample();
}

#define USB_POWER_MV 4350

bool battery_available() { return true; }
bool battery_on_usb() { return battery_millivolts() >= USB_POWER_MV; }
uint16_t battery_millivolts() { return (uint16_t)(s_cell_mv_filt >> 4); }
uint16_t battery_pin_millivolts() { return s_pin_mv; }

/* Typical single-cell Li-ion resting voltage vs remaining charge. */
static const struct { uint16_t mv; uint8_t pct; } CURVE[] = {
    {4180, 100}, {4100, 92}, {4000, 80}, {3920, 70}, {3850, 60},
    {3790, 50}, {3750, 40}, {3710, 30}, {3670, 20}, {3610, 10},
    {3500, 5},  {3300, 0},
};

uint8_t battery_percent() {
    uint16_t mv = battery_millivolts();
    const int n = sizeof(CURVE) / sizeof(CURVE[0]);
    if (mv >= CURVE[0].mv) return 100;
    for (int i = 1; i < n; i++) {
        if (mv >= CURVE[i].mv) {
            uint16_t hi = CURVE[i - 1].mv, lo = CURVE[i].mv;
            return CURVE[i].pct + (uint8_t)((uint32_t)(mv - lo) * (CURVE[i - 1].pct - CURVE[i].pct) / (hi - lo));
        }
    }
    return 0;
}

#else

void battery_init() {}
void battery_poll() {}
bool battery_available() { return false; }
bool battery_on_usb() { return false; }
uint16_t battery_millivolts() { return 0; }
uint8_t battery_percent() { return 0; }
uint16_t battery_pin_millivolts() { return 0; }

#endif
