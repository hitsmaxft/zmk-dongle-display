/* Copyright (c) 2026 The ZMK Contributors SPDX-License-Identifier: MIT */

#include <zephyr/kernel.h>

#include "fighter_charge.h"

#define CHARGE_WIDTH 29
#define CHARGE_HEIGHT 5
#define CHARGE_X 84

static struct zmk_widget_fighter_charge *charge_widget;

static void draw_rect(lv_layer_t *layer, const lv_area_t *area, lv_color_t color) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.bg_opa = LV_OPA_COVER;
    lv_draw_rect(layer, &dsc, area);
}

static void draw_charge_cb(lv_event_t *event) {
    struct zmk_widget_fighter_charge *widget = lv_event_get_user_data(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t coords;
    lv_obj_get_coords(widget->obj, &coords);

    /* The screen's logical white is the inverted OLED's black backdrop. */
    draw_rect(layer, &coords, lv_color_white());

    lv_area_t rail = {
        .x1 = coords.x1,
        .y1 = coords.y1 + 1,
        .x2 = coords.x2,
        .y2 = coords.y1 + 1,
    };
    draw_rect(layer, &rail, lv_color_black());
    rail.y1 = coords.y2;
    rail.y2 = coords.y2;
    draw_rect(layer, &rail, lv_color_black());

    uint8_t fill_width = ((uint16_t)widget->level * CHARGE_WIDTH) / 100U;
    for (uint8_t x = 0; x < fill_width; x += 2) {
        lv_area_t stripe = {
            .x1 = coords.x1 + x,
            .y1 = coords.y1 + 2,
            .x2 = coords.x1 + x,
            .y2 = coords.y2 - 1,
        };
        draw_rect(layer, &stripe, lv_color_black());
    }
}

int zmk_widget_fighter_charge_init(struct zmk_widget_fighter_charge *widget,
                                   lv_obj_t *parent) {
    widget->level = 0;
    widget->obj = lv_obj_create(parent);
    if (widget->obj == NULL) {
        return -ENOMEM;
    }
    lv_obj_remove_style_all(widget->obj);
    lv_obj_set_size(widget->obj, CHARGE_WIDTH, CHARGE_HEIGHT);
    lv_obj_align(widget->obj, LV_ALIGN_BOTTOM_LEFT, CHARGE_X, 0);
    lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(widget->obj, draw_charge_cb, LV_EVENT_DRAW_MAIN, widget);
    charge_widget = widget;
    return 0;
}

void zmk_widget_fighter_charge_set(uint8_t level) {
    if (charge_widget == NULL) {
        return;
    }
    charge_widget->level = MIN(level, 100);
    lv_obj_invalidate(charge_widget->obj);
}
