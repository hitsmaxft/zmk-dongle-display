/* Copyright (c) 2026 The ZMK Contributors SPDX-License-Identifier: MIT */
#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_battle_battery_side {
    uint8_t level;
};

struct zmk_widget_battle_battery {
    sys_snode_t node;
    lv_obj_t *obj;
    struct zmk_widget_battle_battery_side sides[2];
};

int zmk_widget_battle_battery_init(struct zmk_widget_battle_battery *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_battle_battery_obj(struct zmk_widget_battle_battery *widget);
