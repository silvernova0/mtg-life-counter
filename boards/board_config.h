/**
 * board_config.h — per-board pin mapping and capability flags.
 *
 * Exactly one BOARD_* target macro is defined by platformio.ini depending
 * on which environment you build. Nothing else in the codebase should
 * #include a board-specific header directly — always go through this file.
 */

#pragma once

/* ===========================================================================
 * DIYmalls ESP32-C3 round touch board  (ESP32-2424S012C-I-Y(B))
 * GC9A01 240x240 round LCD + CST816S capacitive touch, SPI display bus.
 *
 * Pin values below are the commonly reported mapping for this board family
 * (as used by several open-source ports of this exact panel). Confirmed
 * working per project notes — flashed and verified booting.
 * ==========================================================================*/
#if defined(BOARD_DIYMALLS_C3)

    #define BOARD_NAME          "DIYmalls ESP32-C3 (touch)"
    #define BOARD_HAS_TOUCH     1
    #define BOARD_HAS_ENCODER   0
    #define BOARD_HAS_ENCODER_BUTTON 0

    /* Display (GC9A01 over SPI) */
    #define PIN_TFT_MOSI        7
    #define PIN_TFT_SCLK        6
    #define PIN_TFT_CS          10
    #define PIN_TFT_DC          2
    #define PIN_TFT_RST         -1   /* tied to EN / not separately controlled */
    #define PIN_TFT_BL          3

    /* SPI bus tuning (C3's SPI is happy at a lower clock than the S3's) */
    #define TFT_SPI_HOST        SPI2_HOST
    #define TFT_FREQ_WRITE      40000000
    #define TFT_FREQ_READ       16000000
    #define TFT_DMA_CHANNEL     SPI_DMA_CH_AUTO
    #define TFT_MEMORY_WIDTH    240
    #define TFT_MEMORY_HEIGHT   240
    #define TFT_DUMMY_READ_PIXEL 8
    #define TFT_DUMMY_READ_BITS  1

    /* Touch (CST816S over I2C) */
    #define PIN_TOUCH_SDA       4
    #define PIN_TOUCH_SCL       5
    #define PIN_TOUCH_INT       0
    #define PIN_TOUCH_RST       -1   /* shared with TFT reset line */
    #define TOUCH_I2C_ADDR      0x15

    #define DISPLAY_HOR_RES     240
    #define DISPLAY_VER_RES     240
    #define DISPLAY_ROTATION    0

    /* LEDC channel for the backlight's PWM dimming (LovyanGFX's
     * Light_PWM). The ESP32-C3 only has 6 LEDC channels (0-5) — channel 7
     * (what this project used unconditionally before board-specific
     * values existed) is valid on the S3 but out of range here, which
     * fails silently-ish (an "no more LEDC channels available" log, no
     * crash) and leaves the backlight never turned on: panel stays dark
     * even though everything else boots and runs fine. */
    #define TFT_BL_PWM_CHANNEL  0

/* ===========================================================================
 * Meshnology ESP32-S3 round board
 * 1.28" 240x240 round GC9A01 LCD + CST816D touch + rotary encoder w/ button.
 *
 * All pins below are CONFIRMED (vendor example + Devin's board header).
 * The board also has an SSD1306 OLED, a WS2812 RGB LED strip, and a power
 * indicator LED that this project doesn't use yet — pins are recorded
 * below in case a future feature wants them.
 * ==========================================================================*/
#elif defined(BOARD_MESHNOLOGY_S3)

    #define BOARD_NAME          "Meshnology ESP32-S3"
    #define BOARD_HAS_TOUCH     1
    #define BOARD_HAS_ENCODER   1
    #define BOARD_HAS_ENCODER_BUTTON 1

    /* Power-enable rails for the display circuit (level-shifter/LDO
     * enable, per the factory source code) — MUST be driven HIGH before
     * gfx.init() or the panel has no power and silently does nothing,
     * regardless of backlight state. Not documented anywhere except in
     * the actual factory setup() code. */
    #define PIN_DISPLAY_PWR_EN1 1
    #define PIN_DISPLAY_PWR_EN2 2

    /* Display (GC9A01 over SPI) */
    #define PIN_TFT_MOSI        11
    #define PIN_TFT_SCLK        10
    #define PIN_TFT_CS          9
    #define PIN_TFT_DC          3
    #define PIN_TFT_RST         14
    #define PIN_TFT_BL          46

    /* SPI bus tuning */
    #define TFT_SPI_HOST        SPI2_HOST
    #define TFT_FREQ_WRITE      80000000
    #define TFT_FREQ_READ       20000000
    #define TFT_DMA_CHANNEL     SPI_DMA_CH_AUTO
    #define TFT_MEMORY_WIDTH    240
    #define TFT_MEMORY_HEIGHT   240
    #define TFT_DUMMY_READ_PIXEL 8
    #define TFT_DUMMY_READ_BITS  1

    /* Touch (CST816D over I2C) */
    #define PIN_TOUCH_SDA       6
    #define PIN_TOUCH_SCL       7
    #define PIN_TOUCH_INT       5
    #define PIN_TOUCH_RST       13
    #define TOUCH_I2C_ADDR      0x15

    /* Rotary encoder */
    #define PIN_ENCODER_A       45
    #define PIN_ENCODER_B       42
    #define PIN_ENCODER_BTN     41

    #define DISPLAY_HOR_RES     240
    #define DISPLAY_VER_RES     240
    #define DISPLAY_ROTATION    0

    /* LEDC channel for the backlight's PWM dimming — see the matching
     * comment on the DIYmalls C3 block above. The S3 has 8 channels
     * (0-7), so the highest one is used here to stay out of the way of
     * anything else that might claim low-numbered channels first. */
    #define TFT_BL_PWM_CHANNEL  7

    /* --- Not wired up by this project yet, recorded for future use --- */
    #define PIN_OLED_SDA        38   /* SSD1306, separate I2C bus from touch */
    #define PIN_OLED_SCL        39
    #define PIN_RGB_LED         48   /* WS2812, LED_NUM 5 */
    #define RGB_LED_NUM         5
    #define PIN_POWER_LIGHT     40
    /* Unassigned/test I/O broken out on this board: GPIO 4, GPIO 12 */

#elif defined(BOARD_WOKWI_S3)

    #define BOARD_NAME          "Wokwi ESP32-S3"
    #define BOARD_HAS_TOUCH     0
    #define BOARD_HAS_ENCODER   1
    #define BOARD_HAS_ENCODER_BUTTON 1

    /* Simulated ILI9341 over 4-wire SPI, using its native portrait geometry
     * with LVGL rotated to the 320x240 landscape orientation. */
    #define PIN_TFT_MOSI        11
    #define PIN_TFT_SCLK        12
    #define PIN_TFT_CS          10
    #define PIN_TFT_DC          9
    #define PIN_TFT_RST         -1
    #define PIN_TFT_BL          -1

    #define TFT_SPI_HOST        SPI2_HOST
    #define TFT_FREQ_WRITE      40000000
    #define TFT_FREQ_READ       16000000
    #define TFT_DMA_CHANNEL     SPI_DMA_CH_AUTO
    #define TFT_MEMORY_WIDTH    240
    #define TFT_MEMORY_HEIGHT   320
    #define TFT_DUMMY_READ_PIXEL 8
    #define TFT_DUMMY_READ_BITS  1
    #define TFT_BL_PWM_CHANNEL  0

    /* KY-040: CLK=A, DT=B, SW=active-low push button. */
    #define PIN_ENCODER_A       4
    #define PIN_ENCODER_B       5
    #define PIN_ENCODER_BTN     6

    #define DISPLAY_HOR_RES     320
    #define DISPLAY_VER_RES     240
    #define DISPLAY_ROTATION    1

#elif defined(BOARD_WAVESHARE_S3)

    #define BOARD_NAME          "Waveshare ESP32-S3 Knob Touch LCD 1.8"
    #define BOARD_HAS_TOUCH     1
    #define BOARD_HAS_ENCODER   1
    #define BOARD_HAS_ENCODER_BUTTON 0

    /* Display (ST77916 over QSPI), per Waveshare's board configuration. */
    #define PIN_TFT_SCLK        13
    #define PIN_TFT_IO0         15
    #define PIN_TFT_IO1         16
    #define PIN_TFT_IO2         17
    #define PIN_TFT_IO3         18
    #define PIN_TFT_CS          14
    #define PIN_TFT_RST         21
    #define PIN_TFT_BL          47

    #define TFT_SPI_HOST        SPI2_HOST
    #define TFT_FREQ_WRITE     40000000
    #define TFT_FREQ_READ      16000000
    #define TFT_DMA_CHANNEL    SPI_DMA_CH_AUTO
    #define TFT_MEMORY_WIDTH   360
    #define TFT_MEMORY_HEIGHT  360
    #define TFT_DUMMY_READ_PIXEL 8
    #define TFT_DUMMY_READ_BITS  1
    #define TFT_BL_PWM_CHANNEL  0

    /* Touch (CST816S over I2C). */
    #define PIN_TOUCH_SDA       11
    #define PIN_TOUCH_SCL       12
    #define PIN_TOUCH_INT       9
    #define PIN_TOUCH_RST       10
    #define TOUCH_I2C_ADDR      0x15

    /* Rotary encoder; this board's encoder has no push-switch contact. */
    #define PIN_ENCODER_A       8
    #define PIN_ENCODER_B       7

    #define DISPLAY_HOR_RES     360
    #define DISPLAY_VER_RES     360
    #define DISPLAY_ROTATION    0

#else
    #error "No board selected. Choose a supported PlatformIO environment."
#endif
