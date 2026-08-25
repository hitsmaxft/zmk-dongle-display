/* Copyright (c) 2026 The ZMK Contributors SPDX-License-Identifier: MIT */

#include <zephyr/kernel.h>
#include <string.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include "battle_battery.h"

#define BAR_WIDTH 58
#define BAR_HEIGHT 5
#define BAR_FILL_HEIGHT 3
#define NUMBER_WIDTH 11
#define NUMBER_HEIGHT 7

static const uint8_t digit_rows[10][5] = {
    {0x7, 0x5, 0x5, 0x5, 0x7}, /* 0 */
    {0x2, 0x6, 0x2, 0x2, 0x7}, /* 1 */
    {0x7, 0x1, 0x7, 0x4, 0x7}, /* 2 */
    {0x7, 0x1, 0x7, 0x1, 0x7}, /* 3 */
    {0x5, 0x5, 0x7, 0x1, 0x1}, /* 4 */
    {0x7, 0x4, 0x7, 0x1, 0x7}, /* 5 */
    {0x7, 0x4, 0x7, 0x5, 0x7}, /* 6 */
    {0x7, 0x1, 0x2, 0x2, 0x2}, /* 7 */
    {0x7, 0x5, 0x7, 0x5, 0x7}, /* 8 */
    {0x7, 0x5, 0x7, 0x1, 0x7}, /* 9 */
};

LV_DRAW_BUF_DEFINE_STATIC(battle_battery_buf, 126, 13, LV_COLOR_FORMAT_I1);

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
struct battle_battery_state { uint8_t source; uint8_t level; };

static void clear_bitmap(lv_draw_buf_t *draw_buf) {
    uint8_t *pixels = lv_draw_buf_goto_xy(draw_buf, 0, 0);
    memset(pixels, 0xff, draw_buf->header.stride * draw_buf->header.h);
}

static void set_black_px(lv_draw_buf_t *draw_buf, int32_t x, int32_t y) {
    uint8_t *byte = lv_draw_buf_goto_xy(draw_buf, x, y);
    *byte &= ~BIT(7 - (x & 0x7));
}

static void draw_number(lv_draw_buf_t *draw_buf, uint8_t level, int32_t origin) {
    for (int32_t x = origin; x < origin + NUMBER_WIDTH; x++) {
        set_black_px(draw_buf, x, NUMBER_HEIGHT + 5);
    }
    uint8_t digits[3];
    uint8_t count;
    if (level >= 100) {
        digits[0] = 1;
        digits[1] = 0;
        digits[2] = 0;
        count = 3;
    } else if (level >= 10) {
        digits[0] = level / 10;
        digits[1] = level % 10;
        count = 2;
    } else {
        digits[0] = level;
        count = 1;
    }

    int32_t width = count * 3 + count - 1;
    int32_t origin_x = origin + (NUMBER_WIDTH - width) / 2;
    for (uint8_t digit = 0; digit < count; digit++) {
        for (uint8_t y = 0; y < 5; y++) {
            uint8_t row = digit_rows[digits[digit]][y];
            for (uint8_t x = 0; x < 3; x++) {
                if ((row & BIT(2 - x)) == 0) {
                    continue;
                }
                set_black_px(draw_buf, origin_x + digit * 4 + x, 6 + y);
            }
        }
    }
}

static void redraw(struct zmk_widget_battle_battery *widget) {
    lv_obj_t *canvas = widget->obj;
    lv_draw_buf_t *draw_buf = lv_canvas_get_draw_buf(canvas);
    clear_bitmap(draw_buf);

    for (uint8_t side = 0; side < 2; side++) {
        int32_t bar_x = side == 0 ? 0 : 68;
        for (int32_t x = bar_x; x < bar_x + BAR_WIDTH; x++) {
            set_black_px(draw_buf, x, 0);
            set_black_px(draw_buf, x, BAR_HEIGHT - 1);
        }

        int32_t fill_width = ((uint32_t)widget->sides[side].level * BAR_WIDTH) / 100U;
        int32_t fill_x = side == 0 ? bar_x + BAR_WIDTH - fill_width : bar_x;
        for (int32_t y = 1; y <= BAR_FILL_HEIGHT; y++) {
            for (int32_t x = fill_x; x < fill_x + fill_width; x++) {
                if (((x + y) & 1) == 0) {
                    set_black_px(draw_buf, x, y);
                }
            }
        }
        draw_number(draw_buf, widget->sides[side].level, side == 0 ? 0 : 115);
    }
    lv_obj_invalidate(canvas);
}

static void update_cb(struct battle_battery_state state) {
    int side_index = -1;
    if (state.source == CONFIG_ZMK_DONGLE_DISPLAY_BATTLE_BATTERY_LEFT_SOURCE) { side_index = 0; }
    else if (state.source == CONFIG_ZMK_DONGLE_DISPLAY_BATTLE_BATTERY_RIGHT_SOURCE) { side_index = 1; }
    if (side_index < 0) { return; }
    struct zmk_widget_battle_battery *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        uint8_t level = MIN(state.level, 100);
        if (widget->sides[side_index].level == level) {
            continue;
        }
        widget->sides[side_index].level = level;
        redraw(widget);
    }
}

static struct battle_battery_state get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *event =
        as_zmk_peripheral_battery_state_changed(eh);
    return (struct battle_battery_state){.source = event->source,
                                         .level = event->state_of_charge};
}
ZMK_DISPLAY_WIDGET_LISTENER(widget_battle_battery, struct battle_battery_state, update_cb,
                            get_state)
ZMK_SUBSCRIPTION(widget_battle_battery, zmk_peripheral_battery_state_changed);

int zmk_widget_battle_battery_init(struct zmk_widget_battle_battery *widget, lv_obj_t *parent) {
    widget->sides[0].level = 0;
    widget->sides[1].level = 0;
    widget->obj = lv_canvas_create(parent);
    LV_DRAW_BUF_INIT_STATIC(battle_battery_buf);
    lv_canvas_set_draw_buf(widget->obj, &battle_battery_buf);
    lv_canvas_set_palette(widget->obj, 0, lv_color_to_32(lv_color_black(), LV_OPA_COVER));
    lv_canvas_set_palette(widget->obj, 1, lv_color_to_32(lv_color_white(), LV_OPA_COVER));
    /* Keep the health-bar top rail flush with the first screen row. */
    lv_obj_align(widget->obj, LV_ALIGN_TOP_LEFT, 1, 0);
    redraw(widget);
    sys_slist_append(&widgets, &widget->node);
    widget_battle_battery_init();
    return 0;
}

lv_obj_t *zmk_widget_battle_battery_obj(struct zmk_widget_battle_battery *widget) {
    return widget->obj;
}
