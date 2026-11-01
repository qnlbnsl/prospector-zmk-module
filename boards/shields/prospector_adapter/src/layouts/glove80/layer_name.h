#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_layer_name {
    sys_snode_t node;
    lv_obj_t *obj;       // container
    lv_obj_t *label[2];  // cross-faded pair; label[cur] is the visible one
    lv_obj_t *bar;       // layer colour bar
    uint8_t cur;
    int16_t shown;       // layer index currently shown, -1 before the first update
};

int zmk_widget_layer_name_init(struct zmk_widget_layer_name *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_layer_name_obj(struct zmk_widget_layer_name *widget);
