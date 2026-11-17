/**
 * Puts the knob's second chip (ESP32-U4WDH) to sleep permanently.
 *
 * From Waveshare's schematic: this chip's IO32 drives the PCM5100A DAC's
 * XSMT pin (low = soft mute). Drive it low and hold it there through deep
 * sleep so the DAC stays muted, then sleep with no wake source. The chip
 * only wakes on a reset, which the USB-serial bridge can still trigger,
 * so it can always be reflashed.
 */

#include <Arduino.h>
#include "driver/rtc_io.h"
#include "esp_sleep.h"

#define PIN_DAC_XSMT GPIO_NUM_32

void setup() {
    Serial.begin(115200);

    rtc_gpio_init(PIN_DAC_XSMT);
    rtc_gpio_set_direction(PIN_DAC_XSMT, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level(PIN_DAC_XSMT, 0);
    rtc_gpio_hold_en(PIN_DAC_XSMT);
    /* Keep the RTC IO domain powered so the hold survives deep sleep. */
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

    Serial.println("[u4_sleep] DAC muted; entering deep sleep (no wake source)");
    Serial.flush();
    esp_deep_sleep_start();
}

void loop() {}
