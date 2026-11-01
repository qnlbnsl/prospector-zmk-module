/*
 * Typing speed row:   72  [graph of the last minute]
 *                    wpm
 *
 * Samples zmk_wpm_get_state() every 2 s from an LVGL timer (display thread). The graph's top
 * follows the highest value shown, but never below 60 wpm, so a slow minute isn't drawn as
 * a full-height spike.
 */

#include "wpm_graph.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zmk/wpm.h>

#include <fonts.h>

#define POINTS 30
#define SAMPLE_MS 2000
#define MIN_TOP 60

static lv_obj_t *num;
static lv_obj_t *chart;
static lv_chart_series_t *series;
static int32_t history[POINTS];
static int head;

static void set_top(int32_t top) {
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, top);
}

static void sample_cb(lv_timer_t *timer) {
    int wpm = CLAMP(zmk_wpm_get_state(), 0, 999);
    lv_label_set_text_fmt(num, "%d", wpm);

    history[head] = wpm;
    head = (head + 1) % POINTS;
    int32_t top = MIN_TOP;
    for (int i = 0; i < POINTS; i++) {
        top = MAX(top, history[i]);
    }
    set_top(top);
    lv_chart_set_next_value(chart, series, wpm);
}

lv_obj_t *zmk_widget_wpm_graph_create(lv_obj_t *parent) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    num = lv_label_create(row);
    lv_obj_set_style_text_font(num, &FG_Medium_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(num, lv_color_hex(0xF2F2F2), LV_PART_MAIN);
    lv_obj_set_style_text_align(num, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(num, 56);
    lv_label_set_text(num, "0");
    lv_obj_align(num, LV_ALIGN_LEFT_MID, 0, -9);

    lv_obj_t *unit = lv_label_create(row);
    lv_obj_set_style_text_font(unit, &DINishCondensed_SemiBold_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(unit, lv_color_hex(0x8A8A8A), LV_PART_MAIN);
    lv_obj_set_style_text_align(unit, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(unit, 56);
    lv_label_set_text(unit, "wpm");
    lv_obj_align(unit, LV_ALIGN_LEFT_MID, 0, 13);

    chart = lv_chart_create(row);
    lv_obj_set_size(chart, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_left(chart, 64, LV_PART_MAIN);
    lv_obj_set_style_pad_right(chart, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(chart, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(chart, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(chart, 0, LV_PART_MAIN);
    lv_obj_remove_flag(chart, LV_OBJ_FLAG_SCROLLABLE);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(chart, 0, 0);
    lv_chart_set_point_count(chart, POINTS);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_width(chart, 0, LV_PART_INDICATOR); /* no point markers */
    lv_obj_set_style_height(chart, 0, LV_PART_INDICATOR);
    series = lv_chart_add_series(chart, lv_color_hex(0x2FD3E0), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(chart, series, 0);
    set_top(MIN_TOP);
    /* The number sits over the chart's left padding; keep it on top. */
    lv_obj_move_foreground(num);
    lv_obj_move_foreground(unit);

    lv_timer_create(sample_cb, SAMPLE_MS, NULL);
    return row;
}
