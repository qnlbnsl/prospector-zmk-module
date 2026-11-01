/*
 * Glove80 layout (280x240, landscape):
 *
 *   y   0- 40   modifier row (Ctrl Alt Super Shift)
 *   y  36-100   layer name, cross-fading, underline right below it (ends ~y 100)
 *   y 118-180   device tiles: LH / RH (battery), Pad L / Pad R (link, tracking interval)
 *   (about 18-22 px clear above and below the tiles)
 *   y 202-232   OS logo bottom left, then "72 wpm" and a small one-minute graph
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
    lv_obj_set_size(zmk_widget_status_row_obj(&status_row_widget), lv_pct(100), 38);
    lv_obj_align(zmk_widget_status_row_obj(&status_row_widget), LV_ALIGN_TOP_MID, 0, 2);


    zmk_widget_layer_name_init(&layer_name_widget, screen);
    lv_obj_set_size(zmk_widget_layer_name_obj(&layer_name_widget), lv_pct(100), 64);
    lv_obj_align(zmk_widget_layer_name_obj(&layer_name_widget), LV_ALIGN_TOP_MID, 0, 36);

    lv_obj_t *grid = zmk_widget_device_grid_create(screen);
    lv_obj_set_size(grid, 260, 62);
    lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, 118);

    lv_obj_t *os_icon = zmk_widget_os_icon_create(screen);
    lv_obj_align(os_icon, LV_ALIGN_BOTTOM_LEFT, 12, -10);

    lv_obj_t *wpm = zmk_widget_wpm_graph_create(screen);
    lv_obj_set_size(wpm, 214, 30);
    lv_obj_align(wpm, LV_ALIGN_BOTTOM_RIGHT, -10, -8);

    return screen;
}
