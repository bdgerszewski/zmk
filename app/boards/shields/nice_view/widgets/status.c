/*
 *
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/display.h>
#include "status.h"
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/usb.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>
#include <zmk/wpm.h>

LV_IMG_DECLARE(reaction_diffusion);

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct output_status_state {
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
};

struct layer_status_state {
    uint8_t index;
    const char *label;
};

struct wpm_status_state {
    uint8_t wpm;
};

static void draw_top(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);
    lv_draw_label_dsc_t label_dsc_center;
    init_label_dsc(&label_dsc_center, LVGL_FOREGROUND, &lv_font_montserrat_16,
                   LV_TEXT_ALIGN_CENTER);
    lv_draw_label_dsc_t label_dsc_wpm;
    init_label_dsc(&label_dsc_wpm, LVGL_FOREGROUND, &lv_font_unscii_8, LV_TEXT_ALIGN_RIGHT);
    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_rect_dsc_t rect_white_dsc;
    init_rect_dsc(&rect_white_dsc, LVGL_FOREGROUND);
    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 1);

    // Fill background
    lv_canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);
    // lv_canvas_draw_rect(canvas, 0, 18, CANVAS_SIZE, CANVAS_SIZE, &rect_white_dsc);

    // Draw battery
    draw_battery(canvas, state);

    // Draw output status
    char output_text[10] = {};

    switch (state->selected_endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        strcat(output_text, LV_SYMBOL_USB);
        break;
    case ZMK_TRANSPORT_BLE:
        if (state->active_profile_bonded) {
            if (state->active_profile_connected) {
                strcat(output_text, LV_SYMBOL_BLUETOOTH);
            } else {
                strcat(output_text, LV_SYMBOL_CLOSE);
            }
        } else {
            strcat(output_text, LV_SYMBOL_SETTINGS);
        }
        break;
    }
    char name[4] = {'B', 'E', 'N', 0};
    // compute x offset to center text
    // int text_width =
    //     lv_txt_get_width(output_text, strlen(name), &lv_font_montserrat_16, 0,
    //     LV_TEXT_FLAG_NONE);
    // int offset = -1 * (CANVAS_SIZE - text_width) / 2;
    lv_canvas_draw_text(canvas, 0, 0, CANVAS_SIZE, &label_dsc, output_text);
    lv_canvas_draw_text(canvas, 0, 18, CANVAS_SIZE, &label_dsc_center, name);
    // End draw output status

    // ~~Draw WPM~~ Disabled, surround this in an ifdef and control via config?
    // lv_canvas_draw_rect(canvas, 0, 21, 68, 42, &rect_white_dsc);
    // lv_canvas_draw_rect(canvas, 1, 22, 66, 40, &rect_black_dsc);

    // char wpm_text[6] = {};
    // snprintf(wpm_text, sizeof(wpm_text), "%d", state->wpm[9]);
    // lv_canvas_draw_text(canvas, 42, 52, 24, &label_dsc_wpm, wpm_text);

    // int max = 0;
    // int min = 256;

    // for (int i = 0; i < 10; i++) {
    //     if (state->wpm[i] > max) {
    //         max = state->wpm[i];
    //     }
    //     if (state->wpm[i] < min) {
    //         min = state->wpm[i];
    //     }
    // }

    // int range = max - min;
    // if (range == 0) {
    //     range = 1;
    // }

    // lv_point_t points[10];
    // for (int i = 0; i < 10; i++) {
    //     points[i].x = 2 + i * 7;
    //     points[i].y = 60 - (state->wpm[i] - min) * 36 / range;
    // }
    // lv_canvas_draw_line(canvas, points, 10, &line_dsc);

    // Rotate canvas
    rotate_canvas(canvas, cbuf);
}

static void draw_middle(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 1);

    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_rect_dsc_t rect_white_dsc;
    init_rect_dsc(&rect_white_dsc, LVGL_FOREGROUND);
    lv_draw_arc_dsc_t arc_dsc;
    init_arc_dsc(&arc_dsc, LVGL_FOREGROUND, 2);
    lv_draw_arc_dsc_t arc_dsc_filled;
    init_arc_dsc(&arc_dsc_filled, LVGL_FOREGROUND, 9);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_18, LV_TEXT_ALIGN_CENTER);
    lv_draw_label_dsc_t label_dsc_black;
    init_label_dsc(&label_dsc_black, LVGL_BACKGROUND, &lv_font_montserrat_18, LV_TEXT_ALIGN_CENTER);

    // Fill background
    // lv_canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_white_dsc);
    lv_obj_t *art = lv_img_create(canvas);
    lv_img_set_src(art, &reaction_diffusion);
    // lv_obj_align(art, LV_ALIGN_TOP_RIGHT, 0, 0);
    // lv_canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);

    // Draw circles
    // int circle_offsets[5][2] = {
    //     {13, 13}, {55, 13}, {34, 34}, {13, 55}, {55, 55},
    // };

    // for (int i = 0; i < 5; i++) {
    //     bool selected = i == state->active_profile_index;

    //     lv_canvas_draw_arc(canvas, circle_offsets[i][0], circle_offsets[i][1], 13, 0, 360,
    //                        &arc_dsc);

    //     if (selected) {
    //         lv_canvas_draw_arc(canvas, circle_offsets[i][0], circle_offsets[i][1], 9, 0, 359,
    //                            &arc_dsc_filled);
    //     }

    //     char label[2];
    //     snprintf(label, sizeof(label), "%d", i + 1);
    //     lv_canvas_draw_text(canvas, circle_offsets[i][0] - 8, circle_offsets[i][1] - 10, 16,
    //                         (selected ? &label_dsc_black : &label_dsc), label);
    // }

    // Rotate canvas
    rotate_canvas(canvas, cbuf);
}

static void draw_bottom(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 2);

    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);

    // Fill background
    lv_canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);

    // Calculate the y-position for each line of text
    int line_height = lv_font_get_line_height(&lv_font_montserrat_14);
    int first_line_y = 5;   // CANVAS_SIZE - 2 * line_height; // Adjust position for two lines
    int second_line_y = 21; // CANVAS_SIZE - line_height;    // Position for the second line
    // int first_line_y = 2 * line_height; // Adjust position for two lines
    // int second_line_y = line_height;    // Position for the second line

    // Draw layer label or index
    if (state->layer_label == NULL) {
        char layer_text[10];
        sprintf(layer_text, "LAYER %i", state->layer_index);
        lv_canvas_draw_text(canvas, 0, first_line_y, CANVAS_SIZE, &label_dsc, layer_text);
    } else {
        lv_canvas_draw_text(canvas, 0, first_line_y, CANVAS_SIZE, &label_dsc, state->layer_label);
    }

    // Draw Bluetooth profile line
    char profile_text[20];
    sprintf(profile_text, "PRFILE %d", state->active_profile_index + 1);
    lv_canvas_draw_text(canvas, 0, second_line_y, CANVAS_SIZE, &label_dsc, profile_text);

    // lv_draw_rect_dsc_t rect_white_dsc;
    // init_rect_dsc(&rect_white_dsc, LVGL_FOREGROUND);
    // lv_canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_white_dsc);

    // Rotate canvas
    rotate_canvas(canvas, cbuf);
}

static void set_battery_status(struct zmk_widget_status *widget,
                               struct battery_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging = state.usb_present;
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

    widget->state.battery = state.level;

    draw_top(widget->obj, widget->cbuf, &widget->state);
}

static void battery_status_update_cb(struct battery_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_status(widget, state); }
}

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);

    return (struct battery_status_state){
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
                            battery_status_update_cb, battery_status_get_state)

ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

static void set_output_status(struct zmk_widget_status *widget,
                              const struct output_status_state *state) {
    widget->state.selected_endpoint = state->selected_endpoint;
    widget->state.active_profile_index = state->active_profile_index;
    widget->state.active_profile_connected = state->active_profile_connected;
    widget->state.active_profile_bonded = state->active_profile_bonded;

    draw_top(widget->obj, widget->cbuf, &widget->state);
    draw_middle(widget->obj, widget->cbuf2, &widget->state);
}

static void output_status_update_cb(struct output_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_output_status(widget, &state); }
}

static struct output_status_state output_status_get_state(const zmk_event_t *_eh) {
    return (struct output_status_state){
        .selected_endpoint = zmk_endpoints_selected(),
        .active_profile_index = zmk_ble_active_profile_index(),
        .active_profile_connected = zmk_ble_active_profile_is_connected(),
        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, output_status_get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);
#endif
#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#endif

static void set_layer_status(struct zmk_widget_status *widget, struct layer_status_state state) {
    widget->state.layer_index = state.index;
    widget->state.layer_label = state.label;

    draw_bottom(widget->obj, widget->cbuf3, &widget->state);
}

static void layer_status_update_cb(struct layer_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_layer_status(widget, state); }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_status_state){.index = index, .label = zmk_keymap_layer_name(index)};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state, layer_status_update_cb,
                            layer_status_get_state)

ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

static void set_wpm_status(struct zmk_widget_status *widget, struct wpm_status_state state) {
    for (int i = 0; i < 9; i++) {
        widget->state.wpm[i] = widget->state.wpm[i + 1];
    }
    widget->state.wpm[9] = state.wpm;

    draw_top(widget->obj, widget->cbuf, &widget->state);
}

static void wpm_status_update_cb(struct wpm_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_wpm_status(widget, state); }
}

struct wpm_status_state wpm_status_get_state(const zmk_event_t *eh) {
    return (struct wpm_status_state){.wpm = zmk_wpm_get_state()};
};

ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm_status, struct wpm_status_state, wpm_status_update_cb,
                            wpm_status_get_state)
ZMK_SUBSCRIPTION(widget_wpm_status, zmk_wpm_state_changed);

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 160, 68);

    lv_obj_t *top = lv_canvas_create(widget->obj);
    lv_obj_align(top, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_canvas_set_buffer(top, widget->cbuf, CANVAS_SIZE, CANVAS_SIZE, LV_IMG_CF_TRUE_COLOR);
    // draw_bounding_box(top);

    lv_obj_t *middle = lv_canvas_create(widget->obj);
    // x offset adjusts nice_view in the y direction. -x does down, +x goes up
    // y offset adjusts nice_view in the x direction. -y goes left, +y goes right
    // middle should be 18 from top
    lv_obj_align(middle, LV_ALIGN_TOP_RIGHT, -38, 0);
    // lv_obj_align(middle, LV_ALIGN_TOP_LEFT, 24, 0);
    // using w or h greater than canvas size seems to cause corruption. Maybe CANVAS_SIZE is used
    // to malloc, but it's using CANVAS_SIZE directly, not the usage in set buffer?
    lv_canvas_set_buffer(middle, widget->cbuf2, CANVAS_SIZE, CANVAS_SIZE, LV_IMG_CF_TRUE_COLOR);
    // draw_bounding_box(middle);

    lv_obj_t *bottom = lv_canvas_create(widget->obj);
    // set the starting points of the buffers on screen
    lv_obj_align(bottom, LV_ALIGN_TOP_LEFT, -24, 0);
    lv_canvas_set_buffer(bottom, widget->cbuf3, CANVAS_SIZE, CANVAS_SIZE, LV_IMG_CF_TRUE_COLOR);
    // draw_bounding_box(bottom);

    sys_slist_append(&widgets, &widget->node);
    widget_battery_status_init();
    widget_output_status_init();
    widget_layer_status_init();
    widget_wpm_status_init();

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) { return widget->obj; }

#ifndef LV_ATTRIBUTE_IMG_REACTION_DIFFUSION
#define LV_ATTRIBUTE_IMG_REACTION_DIFFUSION
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_REACTION_DIFFUSION uint8_t
    reaction_diffusion_map[] = {
        0xff, 0xff, 0xff, 0xff, /*Color of index 0*/
        0x00, 0x00, 0x00, 0xff, /*Color of index 1*/

        0xe1, 0xe0, 0xfc, 0x3f, 0xff, 0xff, 0x07, 0xff, 0xf0, 0xc3, 0xe1, 0xf8, 0x7f, 0xff, 0x3f,
        0x83, 0xff, 0xf0, 0xc3, 0xc1, 0xf0, 0x7c, 0xfe, 0x0f, 0xc1, 0xff, 0xf0, 0xc3, 0xc3, 0xe0,
        0xf8, 0x7c, 0x07, 0xc1, 0xf0, 0x30, 0xc3, 0xc3, 0xc1, 0xf0, 0x3c, 0x03, 0xe1, 0xe0, 0x00,
        0xc7, 0xc7, 0xc1, 0xf0, 0x3c, 0x03, 0xe1, 0xe0, 0x00, 0xff, 0x87, 0x83, 0xe0, 0x7e, 0x01,
        0xe3, 0xe0, 0x00, 0xff, 0x87, 0x83, 0xe1, 0xff, 0x01, 0xe3, 0xe0, 0xf0, 0xff, 0x0f, 0x87,
        0xc3, 0xff, 0xc3, 0xe3, 0xc1, 0xf0, 0xfe, 0x0f, 0x8f, 0x83, 0xff, 0xff, 0xe3, 0xc1, 0xf0,
        0xfc, 0x3f, 0xff, 0x87, 0xc7, 0xff, 0xc3, 0xc3, 0xf0, 0x00, 0x7f, 0xff, 0x07, 0x83, 0xff,
        0xc3, 0xe3, 0xf0, 0x00, 0x7f, 0xff, 0x07, 0x83, 0xff, 0xc3, 0xff, 0xc0, 0x00, 0x7c, 0x7f,
        0x0f, 0x87, 0xc3, 0x03, 0xff, 0xc0, 0x3c, 0x30, 0x1f, 0x0f, 0x07, 0x80, 0x07, 0xff, 0x80,
        0xff, 0x00, 0x0f, 0xdf, 0x0f, 0x80, 0x0f, 0xff, 0x00, 0xff, 0x81, 0x07, 0xff, 0x0f, 0x80,
        0x1f, 0xc4, 0x00, 0xff, 0xc7, 0x87, 0xff, 0x0f, 0x81, 0xff, 0x00, 0x00, 0xf7, 0xc7, 0xc3,
        0xff, 0x87, 0xff, 0xfe, 0x00, 0x30, 0xc3, 0xc7, 0xc0, 0xff, 0x83, 0xff, 0xfc, 0x00, 0xf0,
        0xc3, 0xe3, 0xe0, 0x07, 0xc3, 0xff, 0xf0, 0x0f, 0xf0, 0xc1, 0xe3, 0xe0, 0x03, 0xe1, 0xff,
        0xc0, 0x3f, 0xf0, 0xe1, 0xe1, 0xf8, 0x03, 0xe1, 0xf0, 0x00, 0xff, 0xf0, 0xe1, 0xe0, 0xff,
        0xc1, 0xe1, 0xf0, 0x03, 0xff, 0x80, 0xe1, 0xe0, 0xff, 0xe1, 0xe1, 0xf0, 0x07, 0xfe, 0x00,
        0xe1, 0xe0, 0xff, 0xf1, 0xf1, 0xf8, 0x1f, 0xf0, 0x00, 0xe1, 0xe0, 0xff, 0xf0, 0xf0, 0xfc,
        0x3f, 0xc0, 0x00, 0xe1, 0xe0, 0xf1, 0xf0, 0xf0, 0x7f, 0xff, 0x00, 0x30, 0xe1, 0xe0, 0xf0,
        0xf0, 0xf0, 0x3f, 0xfe, 0x00, 0xf0, 0xe1, 0xe1, 0xf0, 0xf0, 0xf8, 0x1f, 0xf8, 0x03, 0xf0,
        0xe1, 0xf1, 0xf0, 0xf0, 0xfc, 0x07, 0xf0, 0x0f, 0xf0, 0xe1, 0xff, 0xe0, 0xf0, 0xff, 0x01,
        0x80, 0x7f, 0xe0, 0xc1, 0xff, 0xe1, 0xf0, 0xff, 0xc0, 0x01, 0xff, 0xc0, 0xc3, 0xff, 0xe1,
        0xe1, 0xff, 0xf0, 0x07, 0xff, 0x00, 0xc7, 0xef, 0xc3, 0xe1, 0xf7, 0xf8, 0x0f, 0xfc, 0x00,
        0x87, 0xc0, 0x03, 0xe3, 0xe1, 0xfc, 0x1f, 0xe0, 0x00, 0x87, 0xc0, 0x07, 0xc3, 0xc0, 0x7e,
        0x1f, 0x00, 0x10, 0x87, 0xc0, 0x0f, 0xc3, 0xc0, 0x3e, 0x3e, 0x00, 0x30, 0x87, 0xe0, 0x1f,
        0x83, 0xe0, 0x1e, 0x3e, 0x01, 0xf0, 0x83, 0xfe, 0x7f, 0x03, 0xff, 0x1f, 0x1e, 0x0f, 0xf0,
        0xc1, 0xff, 0xfe, 0x07, 0xff, 0x0f, 0x1f, 0xff, 0xf0, 0xe0, 0xff, 0xfc, 0x0f, 0xff, 0x8f,
        0x1f, 0xff, 0xf0, 0xe0, 0x3f, 0xf8, 0x0f, 0xff, 0x0f, 0x0f, 0xff, 0xf0, 0xf0, 0x07, 0xf8,
        0x1f, 0x06, 0x1f, 0x07, 0xfc, 0x30, 0xf8, 0x40, 0x7f, 0xfe, 0x00, 0x1e, 0x01, 0xe0, 0x10,
        0x7f, 0xe0, 0x3f, 0xfe, 0x00, 0x3e, 0x00, 0x00, 0x10, 0x7f, 0xe0, 0x3f, 0xfe, 0x0e, 0x7c,
        0x00, 0x00, 0x30, 0x7f, 0xe0, 0x3f, 0xfe, 0x1f, 0xf8, 0x78, 0x00, 0x70, 0xff, 0xe0, 0x7e,
        0x3f, 0x1f, 0xf0, 0xff, 0xff, 0xf0, 0xf8, 0x01, 0xf8, 0x1f, 0x1f, 0xf0, 0xff, 0xff, 0xf0,
        0xf0, 0x1f, 0xf8, 0x0f, 0x8f, 0xf0, 0xff, 0xff, 0xf0, 0xe0, 0x3f, 0xfc, 0x07, 0xc1, 0xf0,
        0xff, 0xff, 0xf0, 0xc0, 0xff, 0xfe, 0x07, 0xc0, 0xf8, 0x78, 0x0f, 0x80, 0xc1, 0xff, 0xff,
        0x83, 0xe0, 0xf8, 0x78, 0x00, 0x00, 0xc3, 0xf8, 0x1f, 0xc1, 0xe0, 0x7c, 0x7c, 0x00, 0x00,
        0xc7, 0xe0, 0x0f, 0xe1, 0xf0, 0x7c, 0x7e, 0x00, 0x00, 0xc7, 0xc0, 0x07, 0xe1, 0xf0, 0x7c,
        0x3f, 0xe7, 0xf0, 0xc7, 0xc0, 0x01, 0xf0, 0xf0, 0x7c, 0x3f, 0xff, 0xf0, 0xc3, 0xc7, 0xc1,
        0xf0, 0xf8, 0xfc, 0x1f, 0xff, 0xf0, 0xc3, 0xff, 0xe0, 0xf8, 0xfd, 0xf8, 0x03, 0xff, 0xf0,
        0xe1, 0xff, 0xf0, 0xf8, 0x7f, 0xf0, 0x00, 0x7f, 0x00, 0xe1, 0xff, 0xf0, 0x78, 0x7f, 0xe0,
        0x00, 0x00, 0x00, 0xe1, 0xf9, 0xf0, 0x78, 0x3f, 0xc1, 0xf8, 0x00, 0x00, 0xe1, 0xf0, 0xf0,
        0x7c, 0x1f, 0x07, 0xfe, 0x00, 0x00, 0xe1, 0xf0, 0xf8, 0x78, 0x1f, 0x07, 0xff, 0x83, 0xf0,
        0xe1, 0xf0, 0x78, 0x78, 0x1e, 0x0f, 0xff, 0xff, 0xf0, 0xf1, 0xf0, 0x7f, 0xf8, 0x1e, 0x1f,
        0x0f, 0xff, 0xf0, 0xf1, 0xf0, 0x7f, 0xf8, 0x3e, 0x1e, 0x03, 0xff, 0xf0,
};

const lv_img_dsc_t reaction_diffusion = {
    .header.cf = LV_IMG_CF_INDEXED_1BIT,
    .header.always_zero = 0,
    .header.reserved = 0,
    .header.w = 68,
    .header.h = 68,
    .data_size = 620,
    .data = reaction_diffusion_map,
};
