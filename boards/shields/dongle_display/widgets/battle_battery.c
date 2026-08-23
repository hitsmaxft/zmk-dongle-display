/* Copyright (c) 2026 The ZMK Contributors SPDX-License-Identifier: MIT */

#include <zephyr/kernel.h>
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

/* A fixed 2x2 I1 checkerboard makes a 50% gray bar on the monochrome OLED. */
static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t gray_pixels_map[] = {
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0xff,
    0x80, 0x40,
};
static const lv_image_dsc_t gray_pixels = {
    .header.cf = LV_COLOR_FORMAT_I1,
    .header.w = 2,
    .header.h = 2,
    .data_size = sizeof(gray_pixels_map),
    .data = gray_pixels_map,
};

/* Solid white top/bottom rails keep the slot visible; its sides remain open. */
static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t slot_rails_map[] = {
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0xff,
    0xc0, 0x00, 0x00, 0x00, 0xc0,
};
static const lv_image_dsc_t slot_rails = {
    .header.cf = LV_COLOR_FORMAT_I1,
    .header.w = 2,
    .header.h = BAR_HEIGHT,
    .data_size = sizeof(slot_rails_map),
    .data = slot_rails_map,
};

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
struct battle_battery_state { uint8_t source; uint8_t level; };

static void draw_number_part(lv_layer_t *layer, const lv_area_t *area) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    /* The OLED uses inversion-on: logical black is the visible white ink. */
    dsc.bg_color = lv_color_black();
    dsc.bg_opa = LV_OPA_COVER;
    lv_draw_rect(layer, &dsc, area);
}

static void draw_number_cb(lv_event_t *event) {
    struct zmk_widget_battle_battery_side *side = lv_event_get_user_data(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t coords;
    lv_obj_get_coords(side->number, &coords);

    lv_area_t underline = {
        .x1 = coords.x1,
        .y1 = coords.y2,
        .x2 = coords.x2,
        .y2 = coords.y2,
    };
    draw_number_part(layer, &underline);

    uint8_t digits[3];
    uint8_t count;
    if (side->level >= 100) {
        digits[0] = 1;
        digits[1] = 0;
        digits[2] = 0;
        count = 3;
    } else if (side->level >= 10) {
        digits[0] = side->level / 10;
        digits[1] = side->level % 10;
        count = 2;
    } else {
        digits[0] = side->level;
        count = 1;
    }

    int32_t width = count * 3 + count - 1;
    int32_t origin_x = coords.x1 + (NUMBER_WIDTH - width) / 2;
    for (uint8_t digit = 0; digit < count; digit++) {
        for (uint8_t y = 0; y < 5; y++) {
            uint8_t row = digit_rows[digits[digit]][y];
            for (uint8_t x = 0; x < 3; x++) {
                if ((row & BIT(2 - x)) == 0) {
                    continue;
                }
                lv_area_t pixel = {
                    .x1 = origin_x + digit * 4 + x,
                    .y1 = coords.y1 + y,
                    .x2 = origin_x + digit * 4 + x,
                    .y2 = coords.y1 + y,
                };
                draw_number_part(layer, &pixel);
            }
        }
    }
}

static void set_side(struct zmk_widget_battle_battery_side *side, uint8_t level, bool right) {
    level = MIN(level, 100);
    side->level = level;
    lv_obj_invalidate(side->number);
    lv_obj_set_size(side->fill, ((uint32_t)level * BAR_WIDTH) / 100U, BAR_FILL_HEIGHT);
    /* Preserve health toward the screen centre, as in the original battle HUD. */
    lv_obj_align(side->fill, right ? LV_ALIGN_LEFT_MID : LV_ALIGN_RIGHT_MID, 0, 0);
}

static void update_cb(struct battle_battery_state state) {
    int side_index = -1;
    if (state.source == CONFIG_ZMK_DONGLE_DISPLAY_BATTLE_BATTERY_LEFT_SOURCE) { side_index = 0; }
    else if (state.source == CONFIG_ZMK_DONGLE_DISPLAY_BATTLE_BATTERY_RIGHT_SOURCE) { side_index = 1; }
    if (side_index < 0) { return; }
    struct zmk_widget_battle_battery *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_side(&widget->sides[side_index], state.level, side_index == 1);
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

static void init_side(struct zmk_widget_battle_battery_side *side, lv_obj_t *parent, bool right) {
    side->level = 0;
    side->number = lv_obj_create(parent);
    lv_obj_remove_style_all(side->number);
    lv_obj_set_size(side->number, NUMBER_WIDTH, NUMBER_HEIGHT);
    lv_obj_align(side->number, LV_ALIGN_TOP_LEFT, right ? 115 : 0, 6);
    lv_obj_clear_flag(side->number, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(side->number, draw_number_cb, LV_EVENT_DRAW_MAIN, side);
    side->bar = lv_obj_create(parent);
    lv_obj_remove_style_all(side->bar);
    lv_obj_set_style_bg_color(side->bar, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(side->bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_image_src(side->bar, &slot_rails, LV_PART_MAIN);
    lv_obj_set_style_bg_image_opa(side->bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_image_tiled(side->bar, true, LV_PART_MAIN);
    lv_obj_set_size(side->bar, BAR_WIDTH, BAR_HEIGHT);
    lv_obj_align(side->bar, LV_ALIGN_TOP_LEFT, right ? 68 : 0, 0);
    lv_obj_clear_flag(side->bar, LV_OBJ_FLAG_SCROLLABLE);
    side->fill = lv_obj_create(side->bar);
    lv_obj_remove_style_all(side->fill);
    lv_obj_set_style_bg_color(side->fill, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(side->fill, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_image_src(side->fill, &gray_pixels, LV_PART_MAIN);
    lv_obj_set_style_bg_image_opa(side->fill, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_image_tiled(side->fill, true, LV_PART_MAIN);
    lv_obj_set_size(side->fill, 0, BAR_FILL_HEIGHT);
    lv_obj_align(side->fill, right ? LV_ALIGN_LEFT_MID : LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_clear_flag(side->fill, LV_OBJ_FLAG_SCROLLABLE);
}

int zmk_widget_battle_battery_init(struct zmk_widget_battle_battery *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(widget->obj);
    lv_obj_set_size(widget->obj, 126, 13);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_margin_all(widget->obj, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_top(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(widget->obj, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_left(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_right(widget->obj, 0, LV_PART_MAIN);
    /* Keep the health-bar top rail flush with the first screen row. */
    lv_obj_align(widget->obj, LV_ALIGN_TOP_LEFT, 1, 0);
    lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_SCROLLABLE);
    init_side(&widget->sides[0], widget->obj, false);
    init_side(&widget->sides[1], widget->obj, true);
    sys_slist_append(&widgets, &widget->node);
    widget_battle_battery_init();
    return 0;
}

lv_obj_t *zmk_widget_battle_battery_obj(struct zmk_widget_battle_battery *widget) {
    return widget->obj;
}
