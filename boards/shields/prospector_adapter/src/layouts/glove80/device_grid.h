#pragma once

#include <lvgl.h>

/* 2x2 device tiles: keyboard halves on top (link + battery), trackpads below (link, tracking
 * interval while in use). Mirrored like the hardware: left devices left, right devices right. */
lv_obj_t *zmk_widget_device_grid_create(lv_obj_t *parent);
