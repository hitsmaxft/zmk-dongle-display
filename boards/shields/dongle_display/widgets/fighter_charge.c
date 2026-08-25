/* Copyright (c) 2026 The ZMK Contributors SPDX-License-Identifier: MIT */

#include <zephyr/kernel.h>
#include <string.h>

#include "fighter_charge.h"

#define CHARGE_WIDTH 29
#define CHARGE_HEIGHT 5
#define CHARGE_X 84

static struct zmk_widget_fighter_charge *charge_widget;
LV_DRAW_BUF_DEFINE_STATIC(fighter_charge_buf, CHARGE_WIDTH, CHARGE_HEIGHT, LV_COLOR_FORMAT_I1);

static void set_black_px(lv_draw_buf_t *draw_buf, int32_t x, int32_t y) {
    uint8_t *byte = lv_draw_buf_goto_xy(draw_buf, x, y);
    *byte &= ~BIT(7 - (x & 0x7));
}

static void redraw(struct zmk_widget_fighter_charge *widget) {
    lv_obj_t *canvas = widget->obj;
    lv_draw_buf_t *draw_buf = lv_canvas_get_draw_buf(canvas);
    /* The screen's logical white is the inverted OLED's black backdrop. */
    uint8_t *pixels = lv_draw_buf_goto_xy(draw_buf, 0, 0);
    memset(pixels, 0xff, draw_buf->header.stride * draw_buf->header.h);
    for (int32_t x = 0; x < CHARGE_WIDTH; x++) {
        set_black_px(draw_buf, x, 1);
        set_black_px(draw_buf, x, CHARGE_HEIGHT - 1);
    }

    uint8_t fill_width = ((uint16_t)widget->level * CHARGE_WIDTH) / 100U;
    for (uint8_t x = 0; x < fill_width; x += 2) {
        set_black_px(draw_buf, x, 2);
        set_black_px(draw_buf, x, 3);
    }
    lv_obj_invalidate(canvas);
}

int zmk_widget_fighter_charge_init(struct zmk_widget_fighter_charge *widget,
                                   lv_obj_t *parent) {
    widget->level = 0;
    widget->obj = lv_canvas_create(parent);
    if (widget->obj == NULL) {
        return -ENOMEM;
    }
    LV_DRAW_BUF_INIT_STATIC(fighter_charge_buf);
    lv_canvas_set_draw_buf(widget->obj, &fighter_charge_buf);
    lv_canvas_set_palette(widget->obj, 0, lv_color_to_32(lv_color_black(), LV_OPA_COVER));
    lv_canvas_set_palette(widget->obj, 1, lv_color_to_32(lv_color_white(), LV_OPA_COVER));
    lv_obj_align(widget->obj, LV_ALIGN_BOTTOM_LEFT, CHARGE_X, 0);
    redraw(widget);
    charge_widget = widget;
    return 0;
}

void zmk_widget_fighter_charge_set(uint8_t level) {
    if (charge_widget == NULL) {
        return;
    }
    level = MIN(level, 100);
    if (charge_widget->level == level) {
        return;
    }
    charge_widget->level = level;
    redraw(charge_widget);
}
