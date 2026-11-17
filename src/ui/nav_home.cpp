#include "nav_home.h"
#include <Arduino.h>

#define DOUBLE_TAP_MS   350   /* max gap between the two taps */
#define DOUBLE_TAP_PX   50    /* max distance between them */

static nav_home_fn_t s_go_home = nullptr;
static uint32_t s_last_tap_ms = 0;
static lv_point_t s_last_tap_pt = {0, 0};
static bool s_have_first = false;

void nav_home_set(nav_home_fn_t go_home) {
    s_go_home = go_home;
    s_have_first = false;
}

/* True for objects with their own event handlers (buttons, list rows,
 * the brightness arc): a tap there belongs to that control. The screen
 * itself always counts, even though it has a gesture handler. */
static bool handles_taps(lv_obj_t *obj) {
    if (!obj || obj == lv_scr_act()) return false;
    return obj->spec_attr && obj->spec_attr->event_dsc_cnt > 0;
}

static void go_home_async(void *p) {
    (void)p;
    nav_home_fn_t fn = s_go_home;
    if (fn) fn();
}

void nav_home_feedback(lv_indev_drv_t *drv, uint8_t code) {
    (void)drv;
    if (code != LV_EVENT_SHORT_CLICKED || !s_go_home) return;

    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    /* The release that ends a swipe also counts as a click; ignore it. */
    if (lv_indev_get_gesture_dir(indev) != LV_DIR_NONE ||
        handles_taps(lv_indev_get_obj_act())) {
        s_have_first = false;
        return;
    }

    lv_point_t pt;
    lv_indev_get_point(indev, &pt);
    uint32_t now = millis();
    if (s_have_first && now - s_last_tap_ms <= DOUBLE_TAP_MS &&
        LV_ABS(pt.x - s_last_tap_pt.x) <= DOUBLE_TAP_PX &&
        LV_ABS(pt.y - s_last_tap_pt.y) <= DOUBLE_TAP_PX) {
        s_have_first = false;
        /* Leave the screen once LVGL has finished delivering this tap. */
        lv_async_call(go_home_async, nullptr);
        return;
    }
    s_have_first = true;
    s_last_tap_ms = now;
    s_last_tap_pt = pt;
}
