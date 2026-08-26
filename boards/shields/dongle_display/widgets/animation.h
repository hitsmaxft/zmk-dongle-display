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
    lv_obj_t *projectile_obj;
    lv_obj_t *normal_layer;
    lv_obj_t *battle_hud_layer;
    lv_timer_t *timer;
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_DEMO_MODE)
    uint8_t demo_phase;
#if !IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_CHARGE_MODE)
    lv_timer_t *demo_timer;
#endif
#endif
    const struct zmk_dongle_animation_action *action;
    size_t current_band_index;
    uint64_t frame_deadline_ms;
    uint8_t frame_index;
    uint8_t waiting_role;
    uint8_t character_frame_index;
    uint8_t projectile_frame_index;
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_CHARGE_MODE)
    uint8_t charge_level;
#endif
    bool waiting_for_paint;
    bool projectile_visible;
    const void *character_frame;
    const void *projectile_frame;
    int32_t character_x;
    int32_t character_y;
    int32_t projectile_x;
    int32_t projectile_y;
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
lv_obj_t *zmk_widget_dongle_animation_projectile_obj(
    struct zmk_widget_dongle_animation *widget);
