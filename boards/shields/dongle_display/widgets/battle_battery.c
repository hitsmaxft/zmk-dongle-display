/* Copyright (c) 2026 The ZMK Contributors SPDX-License-Identifier: MIT */

#include <zephyr/kernel.h>
#include <string.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/endpoint_changed.h>
#include "battle_battery.h"

#define HUD_WIDTH 128
#define HUD_HEIGHT 5
#define BAR_WIDTH 45
#define BAR_HEIGHT 5
#define BAR_FILL_HEIGHT 3
#define NUMBER_WIDTH 11
#define NUMBER_HEIGHT 5
#define TRANSPORT_GLYPH_WIDTH 3
#define TRANSPORT_TEXT_WIDTH 11
#define LEFT_NUMBER_X 0
#define LEFT_BAR_X 12
#define TRANSPORT_X 57
#define TRANSPORT_WIDTH 14
#define RIGHT_BAR_X 71
#define RIGHT_NUMBER_X 117

BUILD_ASSERT(LEFT_NUMBER_X + NUMBER_WIDTH <= LEFT_BAR_X, "left HUD fields overlap");
BUILD_ASSERT(LEFT_BAR_X + BAR_WIDTH == TRANSPORT_X, "left bar must meet transport field");
BUILD_ASSERT(TRANSPORT_TEXT_WIDTH <= TRANSPORT_WIDTH, "transport label does not fit");
BUILD_ASSERT(TRANSPORT_WIDTH - TRANSPORT_TEXT_WIDTH >= 2,
             "transport label needs transparent side gaps");
BUILD_ASSERT(TRANSPORT_X + TRANSPORT_WIDTH == RIGHT_BAR_X,
             "transport field must meet right bar");
BUILD_ASSERT(RIGHT_BAR_X + BAR_WIDTH <= RIGHT_NUMBER_X, "right HUD fields overlap");
BUILD_ASSERT(RIGHT_NUMBER_X + NUMBER_WIDTH == HUD_WIDTH, "right HUD edge is misaligned");
BUILD_ASSERT(BAR_HEIGHT == HUD_HEIGHT && NUMBER_HEIGHT == HUD_HEIGHT,
             "HUD elements must share one compact row");

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

enum transport_glyph {
    TRANSPORT_GLYPH_U,
    TRANSPORT_GLYPH_S,
    TRANSPORT_GLYPH_B,
    TRANSPORT_GLYPH_L,
    TRANSPORT_GLYPH_E,
};

static const uint8_t transport_glyph_rows[][5] = {
    [TRANSPORT_GLYPH_U] = {0x5, 0x5, 0x5, 0x5, 0x7},
    [TRANSPORT_GLYPH_S] = {0x7, 0x4, 0x7, 0x1, 0x7},
    [TRANSPORT_GLYPH_B] = {0x6, 0x5, 0x6, 0x5, 0x6},
    [TRANSPORT_GLYPH_L] = {0x4, 0x4, 0x4, 0x4, 0x7},
    [TRANSPORT_GLYPH_E] = {0x7, 0x4, 0x6, 0x4, 0x7},
};

LV_DRAW_BUF_DEFINE_STATIC(battle_battery_buf, HUD_WIDTH, HUD_HEIGHT, LV_COLOR_FORMAT_I1);

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

enum battle_hud_state_kind {
    BATTLE_HUD_STATE_BATTERY,
    BATTLE_HUD_STATE_ENDPOINT,
};

struct battle_hud_state {
    enum battle_hud_state_kind kind;
    uint8_t source;
    uint8_t value;
};

static void clear_bitmap(lv_draw_buf_t *draw_buf) {
    uint8_t *pixels = lv_draw_buf_goto_xy(draw_buf, 0, 0);
    memset(pixels, 0xff, draw_buf->header.stride * draw_buf->header.h);
}

static void set_black_px(lv_draw_buf_t *draw_buf, int32_t x, int32_t y) {
    uint8_t *byte = lv_draw_buf_goto_xy(draw_buf, x, y);
    *byte &= ~BIT(7 - (x & 0x7));
}

static void draw_number(lv_draw_buf_t *draw_buf, uint8_t level, int32_t origin) {
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
        for (uint8_t y = 0; y < NUMBER_HEIGHT; y++) {
            uint8_t row = digit_rows[digits[digit]][y];
            for (uint8_t x = 0; x < 3; x++) {
                if ((row & BIT(2 - x)) == 0) {
                    continue;
                }
                set_black_px(draw_buf, origin_x + digit * 4 + x, y);
            }
        }
    }
}

static void draw_transport(lv_draw_buf_t *draw_buf, uint8_t transport) {
    static const uint8_t usb[] = {
        TRANSPORT_GLYPH_U,
        TRANSPORT_GLYPH_S,
        TRANSPORT_GLYPH_B,
    };
    static const uint8_t ble[] = {
        TRANSPORT_GLYPH_B,
        TRANSPORT_GLYPH_L,
        TRANSPORT_GLYPH_E,
    };
    const uint8_t *glyphs;

    switch (transport) {
    case ZMK_TRANSPORT_USB:
        glyphs = usb;
        break;
    case ZMK_TRANSPORT_BLE:
        glyphs = ble;
        break;
    default:
        return;
    }

    int32_t origin_x = TRANSPORT_X + (TRANSPORT_WIDTH - TRANSPORT_TEXT_WIDTH) / 2;
    for (uint8_t glyph = 0; glyph < 3; glyph++) {
        for (uint8_t y = 0; y < HUD_HEIGHT; y++) {
            uint8_t row = transport_glyph_rows[glyphs[glyph]][y];
            for (uint8_t x = 0; x < TRANSPORT_GLYPH_WIDTH; x++) {
                if ((row & BIT(TRANSPORT_GLYPH_WIDTH - 1 - x)) != 0) {
                    set_black_px(draw_buf, origin_x + glyph * 4 + x, y);
                }
            }
        }
    }
}

static void redraw(struct zmk_widget_battle_battery *widget) {
    lv_obj_t *canvas = widget->obj;
    lv_draw_buf_t *draw_buf = lv_canvas_get_draw_buf(canvas);
    clear_bitmap(draw_buf);

    for (uint8_t side = 0; side < 2; side++) {
        int32_t bar_x = side == 0 ? LEFT_BAR_X : RIGHT_BAR_X;
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
        draw_number(draw_buf, widget->sides[side].level,
                    side == 0 ? LEFT_NUMBER_X : RIGHT_NUMBER_X);
    }
    draw_transport(draw_buf, widget->transport);
    lv_obj_invalidate(canvas);
}

static void update_cb(struct battle_hud_state state) {
    if (state.kind == BATTLE_HUD_STATE_ENDPOINT) {
        struct zmk_widget_battle_battery *widget;
        SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
            if (widget->transport != state.value) {
                widget->transport = state.value;
                redraw(widget);
            }
        }
        return;
    }

    int side_index = -1;
    if (state.source == CONFIG_ZMK_DONGLE_DISPLAY_BATTLE_BATTERY_LEFT_SOURCE) { side_index = 0; }
    else if (state.source == CONFIG_ZMK_DONGLE_DISPLAY_BATTLE_BATTERY_RIGHT_SOURCE) { side_index = 1; }
    if (side_index < 0) { return; }
    struct zmk_widget_battle_battery *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        uint8_t level = MIN(state.value, 100);
        if (widget->sides[side_index].level == level) {
            continue;
        }
        widget->sides[side_index].level = level;
        redraw(widget);
    }
}

static struct battle_hud_state get_state(const zmk_event_t *eh) {
    const struct zmk_endpoint_changed *endpoint = as_zmk_endpoint_changed(eh);
    if (endpoint != NULL) {
        return (struct battle_hud_state){.kind = BATTLE_HUD_STATE_ENDPOINT,
                                         .value = endpoint->endpoint.transport};
    }

    const struct zmk_peripheral_battery_state_changed *battery =
        as_zmk_peripheral_battery_state_changed(eh);
    return (struct battle_hud_state){.kind = BATTLE_HUD_STATE_BATTERY,
                                     .source = battery->source,
                                     .value = battery->state_of_charge};
}
ZMK_DISPLAY_WIDGET_LISTENER(widget_battle_battery, struct battle_hud_state, update_cb,
                            get_state)
ZMK_SUBSCRIPTION(widget_battle_battery, zmk_peripheral_battery_state_changed);
ZMK_SUBSCRIPTION(widget_battle_battery, zmk_endpoint_changed);

int zmk_widget_battle_battery_init(struct zmk_widget_battle_battery *widget, lv_obj_t *parent) {
    widget->sides[0].level = 0;
    widget->sides[1].level = 0;
    widget->transport = zmk_endpoints_selected().transport;
    widget->obj = lv_canvas_create(parent);
    LV_DRAW_BUF_INIT_STATIC(battle_battery_buf);
    lv_canvas_set_draw_buf(widget->obj, &battle_battery_buf);
    lv_canvas_set_palette(widget->obj, 0, lv_color_to_32(lv_color_black(), LV_OPA_COVER));
    lv_canvas_set_palette(widget->obj, 1, lv_color_to_32(lv_color_white(), LV_OPA_TRANSP));
    lv_obj_align(widget->obj, LV_ALIGN_TOP_LEFT, 0, 0);
    redraw(widget);
    sys_slist_append(&widgets, &widget->node);
    widget_battle_battery_init();
    return 0;
}

lv_obj_t *zmk_widget_battle_battery_obj(struct zmk_widget_battle_battery *widget) {
    return widget->obj;
}
