#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_status_row {
    sys_snode_t node;
    lv_obj_t *obj;
};

int zmk_widget_status_row_init(struct zmk_widget_status_row *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_status_row_obj(struct zmk_widget_status_row *widget);
