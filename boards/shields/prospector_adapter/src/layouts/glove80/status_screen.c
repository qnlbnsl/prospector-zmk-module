/*
 * Glove80 layout: modifier/caps status row on top, the effective layer's name (cross-fading)
 * in the middle, battery cells for the two halves at the bottom with the OS logo between them.
 */
#include <lvgl.h>

#include "layer_name.h"
#include "battery_bar.h"
#include "status_row.h"

#include <fonts.h>

static struct zmk_widget_layer_name layer_name_widget;
static struct zmk_widget_battery_bar battery_bar_widget;
static struct zmk_widget_status_row status_row_widget;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, 255, LV_PART_MAIN);

    zmk_widget_status_row_init(&status_row_widget, screen);
    lv_obj_set_size(zmk_widget_status_row_obj(&status_row_widget), lv_pct(100), 44);
    lv_obj_align(zmk_widget_status_row_obj(&status_row_widget), LV_ALIGN_TOP_MID, 0, 8);

    zmk_widget_battery_bar_init(&battery_bar_widget, screen);
    lv_obj_set_size(zmk_widget_battery_bar_obj(&battery_bar_widget), lv_pct(100), 48);
    lv_obj_align(zmk_widget_battery_bar_obj(&battery_bar_widget), LV_ALIGN_BOTTOM_MID, 0, 0);

    /* OS logo sits between the two battery cells. */
    lv_obj_t *os_icon = zmk_widget_os_icon_create(screen);
    lv_obj_align(os_icon, LV_ALIGN_BOTTOM_MID, 0, -14);

    zmk_widget_layer_name_init(&layer_name_widget, screen);
    lv_obj_set_size(zmk_widget_layer_name_obj(&layer_name_widget), lv_pct(100), 140);
    lv_obj_align(zmk_widget_layer_name_obj(&layer_name_widget), LV_ALIGN_CENTER, 0, -20);

    return screen;
}
