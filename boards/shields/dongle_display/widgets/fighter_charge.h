/* Copyright (c) 2026 The ZMK Contributors SPDX-License-Identifier: MIT */

#pragma once

#include <lvgl.h>

struct zmk_widget_fighter_charge {
    lv_obj_t *obj;
    uint8_t level;
};

int zmk_widget_fighter_charge_init(struct zmk_widget_fighter_charge *widget,
                                   lv_obj_t *parent);
void zmk_widget_fighter_charge_set(uint8_t level);
