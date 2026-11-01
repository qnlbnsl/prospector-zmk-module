/*
 * Battery bar: one cell per split peripheral showing its battery level, or a red X while it is
 * disconnected.
 *
 * State is kept per slot and the whole bar is redrawn from it. (The previous version used
 * ZMK_DISPLAY_WIDGET_LISTENER, which keeps only the latest event: when both halves connected or
 * reported battery within the same display tick, one half's update was lost and its cell stayed
 * on "X" or blank. Its init also called the state function with a NULL event.)
 *
 * CONFIG_PROSPECTOR_BATTERY_SWAP reverses the cells, for when slot 0 (the half that was paired
 * first) is the right half.
 */

#include "battery_bar.h"

#include <string.h>
#include <zmk/display.h>
#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/split/central.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_central_status_changed.h>
#include <zmk/event_manager.h>

#include <fonts.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define SLOTS ZMK_SPLIT_BLE_PERIPHERAL_COUNT

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct slot_state {
    bool connected;
    uint8_t level; // 0 = unknown
};

static struct k_spinlock lock;
static struct slot_state slots[SLOTS];

struct cell {
    lv_obj_t *bar;
    lv_obj_t *num;
    lv_obj_t *nc_bar;
    lv_obj_t *nc_num;
};

static struct cell cells[SLOTS];
static bool initialized = false;

static int cell_for_slot(int slot) {
    return IS_ENABLED(CONFIG_PROSPECTOR_BATTERY_SWAP) ? SLOTS - 1 - slot : slot;
}

static void show(lv_obj_t *obj, bool visible) {
    lv_obj_set_style_opa(obj, visible ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
}

static void render_cell(struct cell *c, struct slot_state s) {
    show(c->nc_bar, !s.connected);
    show(c->nc_num, !s.connected);
    show(c->bar, s.connected);
    show(c->num, s.connected);
    if (!s.connected) {
        return;
    }

    lv_bar_set_value(c->bar, s.level, LV_ANIM_OFF);
    if (s.level == 0) {
        lv_label_set_text(c->num, "--");
    } else {
        lv_label_set_text_fmt(c->num, "%d", s.level);
    }

    bool low = s.level > 0 && s.level < 20;
    lv_obj_set_style_bg_color(c->bar, lv_color_hex(low ? 0xD3900F : 0x909090), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(c->bar, lv_color_hex(low ? 0xE8AC11 : 0xf0f0f0),
                                   LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(c->bar, lv_color_hex(low ? 0x6E4E07 : 0x202020), LV_PART_MAIN);
    lv_obj_set_style_text_color(c->num, lv_color_hex(low ? 0xFFB802 : 0xFFFFFF), 0);
}

static void render_work_cb(struct k_work *work) {
    if (!initialized) {
        return;
    }
    struct slot_state copy[SLOTS];
    K_SPINLOCK(&lock) { memcpy(copy, slots, sizeof(copy)); }
    for (int i = 0; i < SLOTS; i++) {
        render_cell(&cells[cell_for_slot(i)], copy[i]);
    }
}

static K_WORK_DEFINE(render_work, render_work_cb);

static void schedule_render(void) {
    if (zmk_display_is_initialized()) {
        k_work_submit_to_queue(zmk_display_work_q(), &render_work);
    }
}

static int battery_bar_listener(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *bat =
        as_zmk_peripheral_battery_state_changed(eh);
    if (bat) {
        if (bat->source < SLOTS) {
            K_SPINLOCK(&lock) {
                slots[bat->source].level = bat->state_of_charge;
                // A battery report can only come over a live connection.
                slots[bat->source].connected = true;
            }
            schedule_render();
        }
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct zmk_split_central_status_changed *conn = as_zmk_split_central_status_changed(eh);
    if (conn && conn->slot < SLOTS) {
        K_SPINLOCK(&lock) { slots[conn->slot].connected = conn->connected; }
        schedule_render();
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(widget_battery_bar, battery_bar_listener);
ZMK_SUBSCRIPTION(widget_battery_bar, zmk_peripheral_battery_state_changed);
ZMK_SUBSCRIPTION(widget_battery_bar, zmk_split_central_status_changed);

int zmk_widget_battery_bar_init(struct zmk_widget_battery_bar *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_width(widget->obj, lv_pct(100));
    lv_obj_set_flex_flow(widget->obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(widget->obj, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(widget->obj, 56, LV_PART_MAIN); /* room for the OS logo */
    lv_obj_set_style_pad_bottom(widget->obj, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(widget->obj, 16, LV_PART_MAIN);

    for (int i = 0; i < SLOTS; i++) {
        struct cell *c = &cells[i];
        lv_obj_t *box = lv_obj_create(widget->obj);
        lv_obj_center(box);
        lv_obj_set_height(box, lv_pct(100));
        lv_obj_set_flex_grow(box, 1);

        c->bar = lv_bar_create(box);
        lv_obj_set_size(c->bar, lv_pct(100), 4);
        lv_obj_align(c->bar, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(c->bar, lv_color_hex(0x202020), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(c->bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(c->bar, 1, LV_PART_MAIN);
        lv_obj_set_style_bg_color(c->bar, lv_color_hex(0x909090), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(c->bar, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_bg_grad_color(c->bar, lv_color_hex(0xf0f0f0), LV_PART_INDICATOR);
        lv_obj_set_style_bg_dither_mode(c->bar, LV_DITHER_ERR_DIFF, LV_PART_INDICATOR);
        lv_obj_set_style_bg_grad_dir(c->bar, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
        lv_obj_set_style_radius(c->bar, 1, LV_PART_INDICATOR);
        lv_bar_set_value(c->bar, 0, LV_ANIM_OFF);

        c->num = lv_label_create(box);
        lv_obj_set_style_text_font(c->num, &FoundryGridnikMedium_20, 0);
        lv_obj_set_style_text_color(c->num, lv_color_white(), 0);
        lv_obj_align(c->num, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(c->num, "--");

        c->nc_bar = lv_obj_create(box);
        lv_obj_set_size(c->nc_bar, lv_pct(100), 4);
        lv_obj_align(c->nc_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(c->nc_bar, lv_color_hex(0x9e2121), LV_PART_MAIN);
        lv_obj_set_style_radius(c->nc_bar, 1, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(c->nc_bar, LV_OPA_COVER, 0);

        c->nc_num = lv_label_create(box);
        lv_obj_set_style_text_color(c->nc_num, lv_color_hex(0xe63030), 0);
        lv_obj_align(c->nc_num, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(c->nc_num, LV_SYMBOL_CLOSE);
    }

    // Pick up levels already fetched before the display came up.
    for (int i = 0; i < SLOTS; i++) {
        uint8_t level = 0;
        if (zmk_split_central_get_peripheral_battery_level(i, &level) == 0 && level > 0) {
            K_SPINLOCK(&lock) {
                slots[i].level = level;
                slots[i].connected = true;
            }
        }
    }

    sys_slist_append(&widgets, &widget->node);
    initialized = true;
    render_work_cb(NULL);

    return 0;
}

lv_obj_t *zmk_widget_battery_bar_obj(struct zmk_widget_battery_bar *widget) { return widget->obj; }
