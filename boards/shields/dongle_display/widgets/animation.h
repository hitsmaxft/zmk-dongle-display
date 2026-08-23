/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_dongle_animation {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *normal_layer;
    lv_obj_t *battle_hud_layer;
    lv_timer_t *timer;
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_DEMO_MODE)
    lv_timer_t *demo_timer;
    uint8_t demo_phase;
#endif
    const struct zmk_dongle_animation_action *action;
    size_t current_band_index;
    uint8_t frame_index;
    bool waiting_for_paint;
    int32_t screen_width;
    int32_t screen_height;
    int32_t origin_x;
    int32_t origin_y;
    int32_t target_x;
};

int zmk_widget_dongle_animation_init(struct zmk_widget_dongle_animation *widget,
                                     lv_obj_t *parent, lv_obj_t *normal_layer,
                                     lv_obj_t *battle_hud_layer);
lv_obj_t *zmk_widget_dongle_animation_obj(struct zmk_widget_dongle_animation *widget);
