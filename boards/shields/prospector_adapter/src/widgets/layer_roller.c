#include "layer_roller.h"

#include <ctype.h>
#include <zmk/display.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>

#include <fonts.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static char layer_names_buffer[512] = {0}; // Buffer for concatenated layer names

// Roller row for each layer index, or -1 for layers hidden from the roller.
static int8_t roller_rows[ZMK_KEYMAP_LAYERS_LEN];

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct layer_roller_state {
    uint8_t row;
};

static void layer_roller_set_sel(lv_obj_t *roller, struct layer_roller_state state) {
    LOG_DBG("Layer roller: selected %d, want %d", lv_roller_get_selected(roller), state.row);
    if (lv_roller_get_selected(roller) != state.row) {
        lv_roller_set_selected(roller, state.row, LV_ANIM_ON);
    }
}

static void layer_roller_update_cb(struct layer_roller_state state) {
    struct zmk_widget_layer_roller *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        layer_roller_set_sel(widget->obj, state);
    }
}

// Show the highest active layer that has a roller row. Hidden layers (e.g. home-row-mod helper
// layers that flash on and off while typing) fall through to the visible layer beneath them.
static struct layer_roller_state layer_roller_get_state(const zmk_event_t *eh) {
    for (int i = ZMK_KEYMAP_LAYERS_LEN - 1; i >= 0; i--) {
        zmk_keymap_layer_id_t id = zmk_keymap_layer_index_to_id(i);
        if (id == ZMK_KEYMAP_LAYER_ID_INVAL || roller_rows[i] < 0) {
            continue;
        }
        if (zmk_keymap_layer_active(id)) {
            LOG_DBG("Layer roller: layer index %d -> row %d", i, roller_rows[i]);
            return (struct layer_roller_state){.row = roller_rows[i]};
        }
    }
    LOG_DBG("Layer roller: no named layer active -> row 0");
    return (struct layer_roller_state){.row = 0};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_roller, struct layer_roller_state, layer_roller_update_cb,
                            layer_roller_get_state)
ZMK_SUBSCRIPTION(widget_layer_roller, zmk_layer_state_changed);

static void mask_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);

    static int16_t mask_top_id = -1;
    static int16_t mask_bottom_id = -1;

    if(code == LV_EVENT_COVER_CHECK) {
        lv_event_set_cover_res(e, LV_COVER_RES_MASKED);

    }
    else if(code == LV_EVENT_DRAW_MAIN_BEGIN) {
        /* add mask */
        const lv_font_t * font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);
        lv_coord_t line_space = lv_obj_get_style_text_line_space(obj, LV_PART_MAIN);
        lv_coord_t font_h = lv_font_get_line_height(font);

        lv_area_t roller_coords;
        lv_obj_get_coords(obj, &roller_coords);

        lv_area_t rect_area;
        rect_area.x1 = roller_coords.x1;
        rect_area.x2 = roller_coords.x2;
        rect_area.y1 = roller_coords.y1;
        rect_area.y2 = roller_coords.y1 + (lv_obj_get_height(obj) - font_h - line_space) / 2;

        lv_draw_mask_fade_param_t * fade_mask_top = lv_mem_buf_get(sizeof(lv_draw_mask_fade_param_t));
        lv_draw_mask_fade_init(fade_mask_top, &rect_area, LV_OPA_TRANSP, rect_area.y1, LV_OPA_COVER, rect_area.y2);
        mask_top_id = lv_draw_mask_add(fade_mask_top, NULL);

        rect_area.y1 = rect_area.y2 + font_h + line_space - 1;
        rect_area.y2 = roller_coords.y2;

        lv_draw_mask_fade_param_t * fade_mask_bottom = lv_mem_buf_get(sizeof(lv_draw_mask_fade_param_t));
        lv_draw_mask_fade_init(fade_mask_bottom, &rect_area, LV_OPA_COVER, rect_area.y1, LV_OPA_TRANSP, rect_area.y2);
        mask_bottom_id = lv_draw_mask_add(fade_mask_bottom, NULL);

    }
    else if(code == LV_EVENT_DRAW_POST_END) {
        lv_draw_mask_fade_param_t * fade_mask_top = lv_draw_mask_remove_id(mask_top_id);
        lv_draw_mask_fade_param_t * fade_mask_bottom = lv_draw_mask_remove_id(mask_bottom_id);
        lv_draw_mask_free_param(fade_mask_top);
        lv_draw_mask_free_param(fade_mask_bottom);
        lv_mem_buf_release(fade_mask_top);
        lv_mem_buf_release(fade_mask_bottom);
        mask_top_id = -1;
        mask_bottom_id = -1;
    }
}

int zmk_widget_layer_roller_init(struct zmk_widget_layer_roller *widget, lv_obj_t *parent) {
    widget->obj = lv_roller_create(parent);

    layer_names_buffer[0] = '\0';
    size_t len = 0;
    int8_t rows = 0;
    for (int i = 0; i < ZMK_KEYMAP_LAYERS_LEN; i++) {
        roller_rows[i] = -1;
    }

    for (int i = 0; i < ZMK_KEYMAP_LAYERS_LEN; i++) {
        const char *layer_name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(i));
        bool unnamed = !layer_name || !*layer_name;
#if IS_ENABLED(CONFIG_PROSPECTOR_LAYER_ROLLER_HIDE_UNNAMED)
        if (unnamed) {
            continue;
        }
#endif
        char name[32];
        if (unnamed) {
            snprintf(name, sizeof(name), "%d", i); // just use the number for unnamed layers
        } else {
            snprintf(name, sizeof(name), "%s", layer_name);
        }
#if IS_ENABLED(CONFIG_PROSPECTOR_LAYER_ROLLER_ALL_CAPS)
        for (char *c = name; *c; c++) {
            *c = toupper((unsigned char)*c);
        }
#endif
        int n = snprintf(layer_names_buffer + len, sizeof(layer_names_buffer) - len, "%s%s",
                         rows > 0 ? "\n" : "", name);
        if (n < 0 || len + n >= sizeof(layer_names_buffer)) {
            LOG_WRN("Layer roller: names buffer full, layer %d and up not shown", i);
            layer_names_buffer[len] = '\0';
            break;
        }
        len += n;
        LOG_DBG("Layer roller: layer %d '%s' -> row %d", i, name, rows);
        roller_rows[i] = rows++;
    }

    lv_roller_set_options(widget->obj, layer_names_buffer,
                          IS_ENABLED(CONFIG_PROSPECTOR_LAYER_ROLLER_INFINITE)
                              ? LV_ROLLER_MODE_INFINITE
                              : LV_ROLLER_MODE_NORMAL);

    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_bg_color(&style, lv_color_black());
    lv_style_set_text_color(&style, lv_color_white());
    // lv_style_set_text_letter_space(&style, 2);
    lv_style_set_border_width(&style, 0);
    lv_style_set_pad_all(&style, 0);
    // lv_obj_add_style(lv_scr_act(), &style, 0);

    lv_obj_add_style(widget->obj, &style, 0);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, LV_PART_SELECTED);
    lv_obj_set_style_text_font(widget->obj, &FRAC_Regular_48, LV_PART_SELECTED);
    lv_obj_set_style_text_color(widget->obj, lv_color_hex(0xffffff), LV_PART_SELECTED);
    // lv_obj_set_style_text_line_space(widget->obj, 20, LV_PART_SELECTED);
    // lv_obj_set_style_text_line_space(widget->obj, 20, LV_PART_MAIN);
    lv_obj_set_style_text_font(widget->obj, &FRAC_Thin_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(widget->obj, lv_color_hex(0x909090), LV_PART_MAIN);
    // lv_obj_set_style_text_align(widget->obj, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_add_event_cb(widget->obj, mask_event_cb, LV_EVENT_ALL, NULL);

    // static lv_style_t style_roller;
    // lv_style_init(&style_roller);
    // lv_style_set_text_font(&style_roller, &SF_Compact_Text_Light_24);
    // lv_style_set_text_letter_space(&style_roller, -0.5);
    // lv_style_set_text_line_space(&style_roller, 20);
    // lv_style_set_text_color(&style_roller, lv_color_hex(0x6b6b6b));
    // lv_style_set_bg_color(&style_roller, lv_color_hex(0x050505));
    // lv_style_set_bg_opa(&style_roller, 255);

    // static lv_style_t style_roller_sel;
    // lv_style_init(&style_roller_sel);
    // lv_style_set_text_font(&style_roller_sel, &SF_Compact_Text_Semibold_28);
    // lv_style_set_text_letter_space(&style_roller_sel, -0.5);
    // lv_style_set_text_line_space(&style_roller_sel, 16);
    // lv_style_set_text_color(&style_roller_sel, lv_color_hex(0xffffff));
    // lv_style_set_bg_color(&style_roller_sel, lv_color_hex(0x1c1c1c));
    // lv_style_set_bg_opa(&style_roller_sel, 255);

    // lv_obj_set_style_text_align(widget->obj, LV_TEXT_ALIGN_CENTER, 0);
    // lv_obj_add_style(widget->obj, &style_roller, LV_PART_MAIN);
    // lv_obj_add_style(widget->obj, &style_roller_sel, LV_PART_SELECTED);
    // lv_obj_set_style_radius(widget->obj, 20, LV_PART_MAIN);

    lv_obj_set_style_anim_time(widget->obj, CONFIG_PROSPECTOR_LAYER_ROLLER_ANIM_MS, 0);

    sys_slist_append(&widgets, &widget->node);

    widget_layer_roller_init();
    return 0;
}

lv_obj_t *zmk_widget_layer_roller_obj(struct zmk_widget_layer_roller *widget) {
    return widget->obj;
}