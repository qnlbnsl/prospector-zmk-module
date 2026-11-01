#pragma once

#include <lvgl.h>

/* Typing speed: ZMK's WPM (both halves, counted on the dongle) as a number plus a one-minute
 * history graph. */
lv_obj_t *zmk_widget_wpm_graph_create(lv_obj_t *parent);
