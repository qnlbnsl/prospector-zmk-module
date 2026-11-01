/*
 * Layer name widget: shows the name of the effective layer in large type, cross-fading to the
 * next name on change, with a colour bar underneath.
 *
 * Effective layer = the highest active layer that has a display-name (with
 * CONFIG_PROSPECTOR_LAYER_ROLLER_HIDE_UNNAMED; otherwise simply the highest active layer). Helper
 * layers such as home-row-mod layers have no display-name, so they never flash by while typing.
 *
 * Deliberately simple: two labels and a rectangle, LVGL's built-in opacity fade, no draw masks.
 */

#include "layer_name.h"

#include <string.h>
#include <zmk/display.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>

#include <fonts.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct layer_name_state {
    uint8_t index;
};

static bool name_skipped(const char *name) {
    const char *list = CONFIG_PROSPECTOR_LAYER_NAME_SKIP;
    size_t len = strlen(name);
    while (*list) {
        const char *end = strchr(list, ',');
        size_t n = end ? (size_t)(end - list) : strlen(list);
        if (n == len && strncmp(list, name, n) == 0) {
            return true;
        }
        if (!end) {
            break;
        }
        list = end + 1;
    }
    return false;
}

static bool layer_visible(uint8_t index) {
    const char *name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(index));
    if (name && *name && name_skipped(name)) {
        return false;
    }
#if IS_ENABLED(CONFIG_PROSPECTOR_LAYER_ROLLER_HIDE_UNNAMED)
    return name && *name;
#else
    return true;
#endif
}

static struct layer_name_state layer_name_get_state(const zmk_event_t *eh) {
    for (int i = ZMK_KEYMAP_LAYERS_LEN - 1; i > 0; i--) {
        zmk_keymap_layer_id_t id = zmk_keymap_layer_index_to_id(i);
        if (id != ZMK_KEYMAP_LAYER_ID_INVAL && zmk_keymap_layer_active(id) && layer_visible(i)) {
            return (struct layer_name_state){.index = i};
        }
    }
    return (struct layer_name_state){.index = 0};
}

// Accent colour per layer, matched by display name; anything else is white.
static lv_color_t layer_color(const char *name) {
    static const struct {
        const char *name;
        uint32_t rgb;
    } colors[] = {
        {"Base", 0xFFFFFF},   {"Cursor", 0xFF7C4D}, {"Number", 0xFFD900}, {"Fn", 0x00FFFF},
        {"Symbol", 0x00FF80}, {"Mouse", 0x99F5FF},  {"Emoji", 0xFF80BF},  {"World", 0x80FF00},
        {"Lower", 0x7A00FF},  {"Magic", 0xFF0000},  {"WM", 0x3F7FFF},     {"Typing", 0xFFFF00},
        {"Gaming", 0xFF8000}, {"macOS", 0xD0D0D0},
    };
    for (size_t i = 0; i < ARRAY_SIZE(colors); i++) {
        if (name && strcmp(name, colors[i].name) == 0) {
            return lv_color_hex(colors[i].rgb);
        }
    }
    return lv_color_hex(0xFFFFFF);
}

static void layer_name_show(struct zmk_widget_layer_name *w, uint8_t index) {
    if (w->shown == index) {
        return;
    }
    const char *name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(index));
    char fallback[8];
    if (!name || !*name) {
        snprintf(fallback, sizeof(fallback), "%d", index);
        name = fallback;
    }
    lv_color_t color = layer_color(name);
    uint8_t next = w->cur ^ 1;

    lv_label_set_text(w->label[next], name);
    if (w->shown < 0 || CONFIG_PROSPECTOR_LAYER_FADE_MS == 0) {
        lv_obj_set_style_opa(w->label[w->cur], LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_opa(w->label[next], LV_OPA_COVER, LV_PART_MAIN);
    } else {
        lv_obj_fade_out(w->label[w->cur], CONFIG_PROSPECTOR_LAYER_FADE_MS, 0);
        lv_obj_fade_in(w->label[next], CONFIG_PROSPECTOR_LAYER_FADE_MS, 0);
    }
    lv_obj_set_style_bg_color(w->bar, color, LV_PART_MAIN);
    w->cur = next;
    w->shown = index;
}

static void layer_name_update_cb(struct layer_name_state state) {
    struct zmk_widget_layer_name *w;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, w, node) { layer_name_show(w, state.index); }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_name, struct layer_name_state, layer_name_update_cb,
                            layer_name_get_state)
ZMK_SUBSCRIPTION(widget_layer_name, zmk_layer_state_changed);

int zmk_widget_layer_name_init(struct zmk_widget_layer_name *w, lv_obj_t *parent) {
    w->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(w->obj);
    lv_obj_remove_flag(w->obj, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 2; i++) {
        lv_obj_t *l = lv_label_create(w->obj);
        lv_obj_set_width(l, lv_pct(100));
        lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_font(l, &FR_Regular_48, LV_PART_MAIN);
        lv_obj_set_style_text_color(l, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_opa(l, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 0); /* 52 px line box (FR_Regular_48) */
        lv_label_set_text(l, "");
        w->label[i] = l;
    }

    w->bar = lv_obj_create(w->obj);
    lv_obj_remove_style_all(w->bar);
    lv_obj_set_size(w->bar, lv_pct(70), 6);
    lv_obj_set_style_radius(w->bar, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(w->bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(w->bar, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(w->bar, LV_ALIGN_TOP_MID, 0, 54); /* 2 px under the line box */

    w->cur = 0;
    w->shown = -1;

    sys_slist_append(&widgets, &w->node);
    widget_layer_name_init();
    return 0;
}

lv_obj_t *zmk_widget_layer_name_obj(struct zmk_widget_layer_name *w) { return w->obj; }
