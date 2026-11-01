/*
 * Status row across the top of the screen:
 *
 *   ⌃ Ctrl   ⌥ Alt   ⌘ Super   ⇧ Shift: white held, cyan Caps Word, amber sticky / ⇪ Caps Lock
 *
 * plus a separate OS icon (zmk_widget_os_icon_create), placed by the screen.
 *
 * Modifier icons light up while the modifier is held (explicit modifiers only, so a key that
 * merely sends e.g. Ctrl+F19 does not flash Ctrl). OS icon: macOS while a layer named
 * CONFIG_PROSPECTOR_OS_MACOS_LAYER is active, otherwise CONFIG_PROSPECTOR_DEFAULT_OS.
 *
 * All state is gathered in one place and the whole row is redrawn from the display work queue,
 * so bursts of events can't drop an update.
 */

#include "status_row.h"

#include <string.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>
#include <dt-bindings/zmk/modifiers.h>

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
#include <zmk/events/hid_indicators_changed.h>
#endif
#if IS_ENABLED(CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED)
#include <zmk/events/caps_word_state_changed.h>
#endif

#include <fonts.h>
#include <symbols.h>
#include "os_icons.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define COLOR_OFF 0x2A2A2A
#define COLOR_MOD 0xFFFFFF
#define COLOR_CAPS_LOCK 0xFFB802
#define COLOR_CAPS_WORD 0x00FFE5
#define COLOR_OS 0x909090

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
#include <dt-bindings/zmk/hid_indicators.h> /* HID_INDICATOR_CAPS_LOCK */
#endif

/* Shift doubles as the caps indicator: white = Shift held, cyan = Caps Word, amber = sticky Shift
 * (Shift active with no key physically down, e.g. &sk LSHFT) or Caps Lock (drawn as the caps lock
 * arrow). */
enum { ICON_CTRL, ICON_ALT, ICON_GUI, ICON_SHIFT, ICON_COUNT };

static const char *const icon_text[ICON_COUNT] = {
    SYMBOL_CONTROL,
    SYMBOL_OPTION,
    SYMBOL_COMMAND,
    SYMBOL_SHIFT,
};

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static bool initialized;

static lv_obj_t *os_label;
static lv_obj_t *icons[ICON_COUNT];

/* State written from event listeners, read on the display work queue. */
static atomic_t caps_lock;
static atomic_t caps_word;

/* Physically pressed key positions, to tell a held Shift from a latched (sticky) one. */
#define POSITION_WORDS 4
static atomic_t pressed[POSITION_WORDS];

static bool any_key_pressed(void) {
    for (int i = 0; i < POSITION_WORDS; i++) {
        if (atomic_get(&pressed[i])) {
            return true;
        }
    }
    return false;
}

static const char *os_icon(void) {
    for (int i = 0; i < ZMK_KEYMAP_LAYERS_LEN; i++) {
        zmk_keymap_layer_id_t id = zmk_keymap_layer_index_to_id(i);
        if (id == ZMK_KEYMAP_LAYER_ID_INVAL || !zmk_keymap_layer_active(id)) {
            continue;
        }
        const char *name = zmk_keymap_layer_name(id);
        if (name && strcmp(name, CONFIG_PROSPECTOR_OS_MACOS_LAYER) == 0) {
            return OS_ICON_APPLE;
        }
    }
#if IS_ENABLED(CONFIG_PROSPECTOR_DEFAULT_OS_MACOS)
    return OS_ICON_APPLE;
#elif IS_ENABLED(CONFIG_PROSPECTOR_DEFAULT_OS_WINDOWS)
    return OS_ICON_WINDOWS;
#else
    return OS_ICON_LINUX;
#endif
}

static void set_color(lv_obj_t *obj, uint32_t rgb) {
    lv_obj_set_style_text_color(obj, lv_color_hex(rgb), LV_PART_MAIN);
}

static void set_text(lv_obj_t *obj, const char *text) {
    if (strcmp(lv_label_get_text(obj), text) != 0) {
        lv_label_set_text(obj, text);
    }
}

static void render_work_cb(struct k_work *work) {
    if (!initialized) {
        return;
    }
    zmk_mod_flags_t mods = zmk_hid_get_explicit_mods();

    set_color(icons[ICON_CTRL], (mods & (MOD_LCTL | MOD_RCTL)) ? COLOR_MOD : COLOR_OFF);
    set_color(icons[ICON_ALT], (mods & (MOD_LALT | MOD_RALT)) ? COLOR_MOD : COLOR_OFF);
    set_color(icons[ICON_GUI], (mods & (MOD_LGUI | MOD_RGUI)) ? COLOR_MOD : COLOR_OFF);

    if (atomic_get(&caps_lock)) {
        set_text(icons[ICON_SHIFT], SYMBOL_CAPS_LOCK);
        set_color(icons[ICON_SHIFT], COLOR_CAPS_LOCK);
    } else {
        bool shift = mods & (MOD_LSFT | MOD_RSFT);
        set_text(icons[ICON_SHIFT], SYMBOL_SHIFT);
        set_color(icons[ICON_SHIFT], atomic_get(&caps_word) ? COLOR_CAPS_WORD
                                     : !shift               ? COLOR_OFF
                                     : any_key_pressed()    ? COLOR_MOD
                                                            : COLOR_CAPS_LOCK);
    }

    if (os_label) {
        set_text(os_label, os_icon());
    }
}

static K_WORK_DEFINE(render_work, render_work_cb);

static int status_row_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *pos = as_zmk_position_state_changed(eh);
    if (pos) {
        if (pos->position < POSITION_WORDS * ATOMIC_BITS) {
            atomic_set_bit_to(pressed, pos->position, pos->state);
        }
    }
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
    const struct zmk_hid_indicators_changed *ind = as_zmk_hid_indicators_changed(eh);
    if (ind) {
        atomic_set(&caps_lock, (ind->indicators & HID_INDICATOR_CAPS_LOCK) != 0);
    }
#endif
#if IS_ENABLED(CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED)
    const struct zmk_caps_word_state_changed *cw = as_zmk_caps_word_state_changed(eh);
    if (cw) {
        atomic_set(&caps_word, cw->active);
    }
#endif
    /* Modifiers and layers are read when rendering, after the event has been handled. */
    if (zmk_display_is_initialized()) {
        k_work_submit_to_queue(zmk_display_work_q(), &render_work);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(widget_status_row, status_row_listener);
ZMK_SUBSCRIPTION(widget_status_row, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(widget_status_row, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(widget_status_row, zmk_position_state_changed);
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
ZMK_SUBSCRIPTION(widget_status_row, zmk_hid_indicators_changed);
#endif
#if IS_ENABLED(CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED)
ZMK_SUBSCRIPTION(widget_status_row, zmk_caps_word_state_changed);
#endif

int zmk_widget_status_row_init(struct zmk_widget_status_row *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(widget->obj);
    lv_obj_remove_flag(widget->obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(widget->obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(widget->obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(widget->obj, 22, LV_PART_MAIN);

    for (int i = 0; i < ICON_COUNT; i++) {
        icons[i] = lv_label_create(widget->obj);
        lv_obj_set_style_text_font(icons[i], &Symbols_Semibold_32, LV_PART_MAIN);
        lv_label_set_text(icons[i], icon_text[i]);
        set_color(icons[i], COLOR_OFF);
    }

    sys_slist_append(&widgets, &widget->node);
    initialized = true;
    render_work_cb(NULL);
    return 0;
}

lv_obj_t *zmk_widget_status_row_obj(struct zmk_widget_status_row *widget) { return widget->obj; }

lv_obj_t *zmk_widget_os_icon_create(lv_obj_t *parent) {
    os_label = lv_label_create(parent);
    lv_obj_set_style_text_font(os_label, &OS_Icons_26, LV_PART_MAIN);
    set_color(os_label, COLOR_OS);
    lv_label_set_text(os_label, "");
    if (initialized) {
        render_work_cb(NULL);
    }
    return os_label;
}
