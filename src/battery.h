/**
 * battery.h — Li-ion battery voltage and approximate charge level, on
 * boards with a battery divider into an ADC pin (BOARD_HAS_BATTERY_ADC).
 *
 * Only voltage is measurable: there is no fuel gauge, so the capacity of
 * the fitted battery (600 vs 800 mAh) can't be detected, and the percent
 * is an estimate from a typical Li-ion discharge curve. While USB power
 * is charging the cell, the reading sits higher than its true level.
 */

#pragma once

#include <stdint.h>

void battery_init();
/* Cheap; samples at most once every few seconds. Call from loop(). */
void battery_poll();

bool battery_available();          /* false: board can't measure it */
uint16_t battery_millivolts();     /* smoothed cell voltage */
uint8_t battery_percent();         /* 0..100 estimate */
uint16_t battery_pin_millivolts(); /* raw ADC pin voltage, for calibration */
/* On the knob the ADC sees the power rail, which sits near 4.5 V while
 * USB is plugged in (above any Li-ion cell), so a reading that high
 * means "on USB power / charging" rather than a real cell level. */
bool battery_on_usb();
