#include <Arduino.h>
#include "display_driver.h"

LGFX gfx;

static lv_disp_draw_buf_t draw_buf;
#if defined(BOARD_WAVESHARE_S3)
/* Taller strips = fewer render/flush passes per frame. Kept in internal
 * RAM (DMA-capable, and much faster for LVGL to draw into than PSRAM). */
#define DRAW_BUF_LINES 60
#else
#define DRAW_BUF_LINES 40
#endif
static lv_color_t buf1[DISPLAY_HOR_RES * DRAW_BUF_LINES];
static lv_color_t buf2[DISPLAY_HOR_RES * DRAW_BUF_LINES];

/* Set to 1 to log every screen refresh's duration over serial. */
#define DISPLAY_PERF_LOG 0

/* Time spent inside flush_cb (sending pixels) for the refresh in progress,
 * reported with LVGL's own per-refresh total by perf_monitor_cb. */
static uint32_t s_flush_us = 0;

static void perf_monitor_cb(lv_disp_drv_t *drv, uint32_t time_ms, uint32_t px) {
    (void)drv;
    Serial.printf("[perf] refresh %lums (flush %lums) %lupx\n",
                  (unsigned long)time_ms, (unsigned long)(s_flush_us / 1000), (unsigned long)px);
    s_flush_us = 0;
}

void display_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t t0 = micros();
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

#if defined(BOARD_WAVESHARE_S3)
    /* Asynchronous DMA: hand the strip to the SPI DMA engine and return
     * at once, so LVGL renders the next strip into the other buffer while
     * this one is still being sent. LovyanGFX waits for the previous DMA
     * transfer before starting a new one, and LVGL only reuses a buffer
     * after the *next* flush call returns, so the buffer being sent is
     * never overwritten. LV_COLOR_16_SWAP=1 on this board means LVGL's
     * pixels are already in the panel's byte order: no conversion copy.
     * The bus stays claimed (startWrite once) — nothing else uses it. */
    if (gfx.getStartCount() == 0) gfx.startWrite();
    gfx.pushImageDMA(area->x1, area->y1, w, h, (lgfx::swap565_t *)color_p);
#else
    gfx.startWrite();
    gfx.setAddrWindow(area->x1, area->y1, w, h);
    gfx.writePixels((lgfx::rgb565_t *)color_p, w * h);
    gfx.endWrite();
#endif
    s_flush_us += micros() - t0;

    lv_disp_flush_ready(disp);
}

void display_driver_init(lv_disp_drv_t *disp_drv) {
#ifdef PIN_DISPLAY_PWR_EN1
    /* Power-enable rails the display circuit needs before it will respond
     * to anything — see the comment in board_config.h. Only defined on
     * boards that need it. */
    pinMode(PIN_DISPLAY_PWR_EN1, OUTPUT);
    digitalWrite(PIN_DISPLAY_PWR_EN1, HIGH);
#endif
#ifdef PIN_DISPLAY_PWR_EN2
    pinMode(PIN_DISPLAY_PWR_EN2, OUTPUT);
    digitalWrite(PIN_DISPLAY_PWR_EN2, HIGH);
#endif

    gfx.init();
    gfx.setRotation(DISPLAY_ROTATION);
    gfx.setBrightness(255);

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISPLAY_HOR_RES * DRAW_BUF_LINES);

    lv_disp_drv_init(disp_drv);
    disp_drv->hor_res = DISPLAY_HOR_RES;
    disp_drv->ver_res = DISPLAY_VER_RES;
    disp_drv->flush_cb = display_flush_cb;
    disp_drv->draw_buf = &draw_buf;
#if DISPLAY_PERF_LOG
    disp_drv->monitor_cb = perf_monitor_cb;
#else
    (void)perf_monitor_cb;
#endif
}
