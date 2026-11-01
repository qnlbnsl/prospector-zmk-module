#include <lvgl.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>

#include "widgets/layer_name.h"
#include "widgets/battery_bar.h"
#include "widgets/status_row.h"

#include <fonts.h>
#include <sf_symbols.h>

#include <zmk/keymap.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static struct zmk_widget_layer_name layer_name_widget;
static struct zmk_widget_battery_bar battery_bar_widget;
static struct zmk_widget_status_row status_row_widget;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen;
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, 255, LV_PART_MAIN);

    zmk_widget_status_row_init(&status_row_widget, screen);
    lv_obj_set_size(zmk_widget_status_row_obj(&status_row_widget), lv_pct(100), 44);
    lv_obj_align(zmk_widget_status_row_obj(&status_row_widget), LV_ALIGN_TOP_MID, 0, 8);

    zmk_widget_battery_bar_init(&battery_bar_widget, screen);
    // lv_obj_set_width(zmk_widget_battery_bar_obj(&battery_bar_widget), lv_pct(100));
    lv_obj_set_size(zmk_widget_battery_bar_obj(&battery_bar_widget), lv_pct(100), 48);
    lv_obj_align(zmk_widget_battery_bar_obj(&battery_bar_widget), LV_ALIGN_BOTTOM_MID, 0, 0);

    zmk_widget_layer_name_init(&layer_name_widget, screen);
    lv_obj_set_size(zmk_widget_layer_name_obj(&layer_name_widget), lv_pct(100), 140);
    lv_obj_align(zmk_widget_layer_name_obj(&layer_name_widget), LV_ALIGN_CENTER, 0, -20);


    return screen;
}

