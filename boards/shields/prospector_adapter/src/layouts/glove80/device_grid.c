/*
 * Device tiles, laid out like the hardware:
 *
 *   [o LH      87]  [o RH      91]     keyboard halves: link dot, battery %
 *   [o Pad L   on]  [o Pad R 11ms]     trackpads: off / on / scan interval while touched
 *
 * Dot: red = not connected, white = connected, cyan = trackpad being touched.
 *
 * Halves: state per split peripheral slot, from ZMK's battery and connection events (same
 * handling as the old battery bar: kept per slot and redrawn from the display work queue, so
 * two events in one tick can't drop an update). Only the first CONFIG_PROSPECTOR_BATTERY_CELLS
 * slots are halves; CONFIG_PROSPECTOR_BATTERY_SWAP means slot 0 is the right half.
 *
 * Pads: polled from the split central (ptp_split_pad_status) four times a second.
 */

#include "device_grid.h"

#include <string.h>
#include <zephyr/kernel.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_central_status_changed.h>
#include <zmk/split/central.h>

#if IS_ENABLED(CONFIG_PTP_SPLIT_CENTRAL)
#include <ptp.h>
#endif

#include <fonts.h>

#define HALVES MIN(CONFIG_PROSPECTOR_BATTERY_CELLS, ZMK_SPLIT_BLE_PERIPHERAL_COUNT)

#define COLOR_TEXT 0xF2F2F2
#define COLOR_VALUE 0xB4B4B4
#define COLOR_DIM 0x8A8A8A
#define COLOR_BORDER 0x3A3A3A
#define COLOR_OFF 0xE63030
#define COLOR_OFF_BORDER 0x5A1C1C
#define COLOR_ON 0xF2F2F2
#define COLOR_TOUCH 0x2FD3E0
#define COLOR_TOUCH_BORDER 0x1D6E76
#define COLOR_LOW 0xFFB802

struct tile {
    lv_obj_t *box, *dot, *name, *value;
};

/* 0 = LH, 1 = RH, 2 = Pad L, 3 = Pad R */
static struct tile tiles[4];
static bool initialized;

struct half_state {
    bool connected;
    uint8_t level; /* 0 = unknown */
};
static struct k_spinlock lock;
static struct half_state halves[HALVES];

static void style_tile(struct tile *t, uint32_t dot, uint32_t border, uint32_t name_color) {
    lv_obj_set_style_bg_color(t->dot, lv_color_hex(dot), LV_PART_MAIN);
    lv_obj_set_style_border_color(t->box, lv_color_hex(border), LV_PART_MAIN);
    lv_obj_set_style_text_color(t->name, lv_color_hex(name_color), LV_PART_MAIN);
}

static int tile_for_half_slot(int slot) {
    return IS_ENABLED(CONFIG_PROSPECTOR_BATTERY_SWAP) ? HALVES - 1 - slot : slot;
}

static void render_halves(void) {
    struct half_state copy[HALVES];
    K_SPINLOCK(&lock) { memcpy(copy, halves, sizeof(copy)); }
    for (int i = 0; i < HALVES; i++) {
        struct tile *t = &tiles[tile_for_half_slot(i)];
        if (!copy[i].connected) {
            style_tile(t, COLOR_OFF, COLOR_OFF_BORDER, COLOR_DIM);
            lv_label_set_text(t->value, "off");
            lv_obj_set_style_text_color(t->value, lv_color_hex(COLOR_DIM), LV_PART_MAIN);
            continue;
        }
        bool low = copy[i].level > 0 && copy[i].level < 20;
        style_tile(t, low ? COLOR_LOW : COLOR_ON, COLOR_BORDER, COLOR_TEXT);
        if (copy[i].level == 0) {
            lv_label_set_text(t->value, "--");
        } else {
            lv_label_set_text_fmt(t->value, "%d", copy[i].level);
        }
        lv_obj_set_style_text_color(t->value, lv_color_hex(low ? COLOR_LOW : COLOR_VALUE),
                                    LV_PART_MAIN);
    }
}

static void render_work_cb(struct k_work *work) {
    if (initialized) {
        render_halves();
    }
}

static K_WORK_DEFINE(render_work, render_work_cb);

static int device_grid_listener(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *bat =
        as_zmk_peripheral_battery_state_changed(eh);
    if (bat && bat->source < HALVES) {
        K_SPINLOCK(&lock) {
            halves[bat->source].level = bat->state_of_charge;
            halves[bat->source].connected = true; /* a report only comes over a live link */
        }
    }
    const struct zmk_split_central_status_changed *conn = as_zmk_split_central_status_changed(eh);
    if (conn && conn->slot < HALVES) {
        K_SPINLOCK(&lock) { halves[conn->slot].connected = conn->connected; }
    }
    if ((bat || conn) && zmk_display_is_initialized()) {
        k_work_submit_to_queue(zmk_display_work_q(), &render_work);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(widget_device_grid, device_grid_listener);
ZMK_SUBSCRIPTION(widget_device_grid, zmk_peripheral_battery_state_changed);
ZMK_SUBSCRIPTION(widget_device_grid, zmk_split_central_status_changed);

/* Runs in the display thread (LVGL timer). */
static void pads_timer_cb(lv_timer_t *timer) {
    for (int i = 0; i < 2; i++) {
        struct tile *t = &tiles[2 + i];
#if IS_ENABLED(CONFIG_PTP_SPLIT_CENTRAL)
        struct ptp_split_pad_status st;
        ptp_split_pad_status(i == 0 ? PTP_SIDE_LEFT : PTP_SIDE_RIGHT, &st);
#else
        struct {
            bool connected, touching;
            uint16_t interval_us;
        } st = {0};
#endif
        if (!st.connected) {
            style_tile(t, COLOR_OFF, COLOR_OFF_BORDER, COLOR_DIM);
            lv_label_set_text(t->value, "off");
        } else if (st.touching && st.interval_us) {
            style_tile(t, COLOR_TOUCH, COLOR_TOUCH_BORDER, COLOR_TEXT);
            lv_label_set_text_fmt(t->value, "%dms", (st.interval_us + 500) / 1000);
        } else {
            style_tile(t, COLOR_ON, COLOR_BORDER, COLOR_TEXT);
            lv_label_set_text(t->value, "on");
        }
        lv_obj_set_style_text_color(t->value,
                                    lv_color_hex(st.connected ? COLOR_VALUE : COLOR_DIM),
                                    LV_PART_MAIN);
    }
}

static void make_tile(struct tile *t, lv_obj_t *parent, const char *name) {
    t->box = lv_obj_create(parent);
    lv_obj_remove_style_all(t->box);
    lv_obj_remove_flag(t->box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(t->box, lv_pct(48), 28);
    lv_obj_set_style_border_width(t->box, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(t->box, lv_color_hex(COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_radius(t->box, 7, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(t->box, 8, LV_PART_MAIN);

    t->dot = lv_obj_create(t->box);
    lv_obj_remove_style_all(t->dot);
    lv_obj_set_size(t->dot, 9, 9);
    lv_obj_set_style_radius(t->dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(t->dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(t->dot, LV_ALIGN_LEFT_MID, 0, 0);

    t->name = lv_label_create(t->box);
    lv_obj_set_style_text_font(t->name, &DINishCondensed_SemiBold_20, LV_PART_MAIN);
    lv_label_set_text(t->name, name);
    lv_obj_align(t->name, LV_ALIGN_LEFT_MID, 15, 1);

    t->value = lv_label_create(t->box);
    lv_obj_set_style_text_font(t->value, &DINishCondensed_SemiBold_20, LV_PART_MAIN);
    lv_label_set_text(t->value, "off");
    lv_obj_align(t->value, LV_ALIGN_RIGHT_MID, 0, 1);

    style_tile(t, COLOR_OFF, COLOR_OFF_BORDER, COLOR_DIM);
    lv_obj_set_style_text_color(t->value, lv_color_hex(COLOR_DIM), LV_PART_MAIN);
}

lv_obj_t *zmk_widget_device_grid_create(lv_obj_t *parent) {
    lv_obj_t *grid = lv_obj_create(parent);
    lv_obj_remove_style_all(grid);
    lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(grid, 6, LV_PART_MAIN);

    static const char *const names[] = {"LH", "RH", "Pad L", "Pad R"};
    for (int i = 0; i < 4; i++) {
        make_tile(&tiles[i], grid, names[i]);
    }

    /* Pick up halves that connected or reported battery before the display came up. */
    for (int i = 0; i < HALVES; i++) {
        uint8_t level = 0;
        if (zmk_split_central_get_peripheral_battery_level(i, &level) == 0 && level > 0) {
            K_SPINLOCK(&lock) {
                halves[i].level = level;
                halves[i].connected = true;
            }
        }
    }
    initialized = true;
    render_halves();
    pads_timer_cb(NULL);
    lv_timer_create(pads_timer_cb, 250, NULL);
    return grid;
}
