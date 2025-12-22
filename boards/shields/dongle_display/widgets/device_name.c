/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include "device_name.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

int zmk_widget_device_name_init(struct zmk_widget_device_name *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);

    // Set transparent background for LVGL 9.0 compatibility
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->obj, 0, LV_PART_MAIN);

    lv_obj_set_size(widget->obj, 64, 11);
    
    widget->label = lv_label_create(widget->obj);
    lv_label_set_text_static(widget->label, CONFIG_ZMK_DONGLE_DISPLAY_NAME);
    lv_obj_center(widget->label);
    
    sys_slist_append(&widgets, &widget->node);
    
    return 0;
}

lv_obj_t *zmk_widget_device_name_obj(struct zmk_widget_device_name *widget) {
    return widget->obj;
}