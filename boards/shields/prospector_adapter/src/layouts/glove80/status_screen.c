/*
 * Glove80 layout (280x240, landscape), spaced by what is visible, not by the boxes:
 *
 *   visible       block
 *   y   8- 35     modifier icons (row 0-44: 33 px line + 5 px padding top and bottom)
 *   y  55-105     layer name caps top to underline bottom (widget 45-105: 52 px line box,
 *                 underline 2 px below it)
 *   y 125-187     device tiles (2 x 28 px + 6 px)
 *   y 207-232     OS logo, "72 wpm", graph (row 203-235, each with a little padding)
 *
 * 8 px clear at the top and bottom edges and 20 px between blocks. Change the gaps here
 * together so they stay even.
 */

#include <lvgl.h>
#include "layer_name.h"
#include "status_row.h"
#include "device_grid.h"
#include "wpm_graph.h"
#include <fonts.h>

static struct zmk_widget_layer_name layer_name_widget;
static struct zmk_widget_status_row status_row_widget;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, 255, LV_PART_MAIN);

    zmk_widget_status_row_init(&status_row_widget, screen);
    lv_obj_set_size(zmk_widget_status_row_obj(&status_row_widget), lv_pct(100), 44);
    lv_obj_align(zmk_widget_status_row_obj(&status_row_widget), LV_ALIGN_TOP_MID, 0, 0);

    zmk_widget_layer_name_init(&layer_name_widget, screen);
    lv_obj_set_size(zmk_widget_layer_name_obj(&layer_name_widget), lv_pct(100), 60);
    lv_obj_align(zmk_widget_layer_name_obj(&layer_name_widget), LV_ALIGN_TOP_MID, 0, 45);

    lv_obj_t *grid = zmk_widget_device_grid_create(screen);
    lv_obj_set_size(grid, 260, 62);
    lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, 125);

    lv_obj_t *os_icon = zmk_widget_os_icon_create(screen);
    lv_obj_align(os_icon, LV_ALIGN_BOTTOM_LEFT, 12, -6); /* 31 px box: 203-234 */

    lv_obj_t *wpm = zmk_widget_wpm_graph_create(screen);
    lv_obj_set_size(wpm, 214, 32);
    lv_obj_align(wpm, LV_ALIGN_BOTTOM_RIGHT, -10, -5); /* 203-235 */

    return screen;
}
