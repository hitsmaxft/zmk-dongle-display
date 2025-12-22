/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "custom_status_screen.h"
#include "widgets/battery_status.h"
#include "widgets/modifiers.h"
#include "widgets/bongo_cat.h"
#include "widgets/layer_status.h"
#include "widgets/output_status.h"
#include "widgets/hid_indicators.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define PADDING_LEFT CONFIG_ZMK_DONGLE_DISPLAY_PADDING_LEFT
#define PADDING_RIGHT CONFIG_ZMK_DONGLE_DISPLAY_PADDING_RIGHT

static struct zmk_widget_output_status output_status_widget;

#if IS_ENABLED(CONFIG_ZMK_BATTERY)
static struct zmk_widget_dongle_battery_status dongle_battery_status_widget;
#endif

static struct zmk_widget_layer_status layer_status_widget;
static struct zmk_widget_modifiers modifiers_widget;
static struct zmk_widget_bongo_cat bongo_cat_widget;

#if IS_EMPTY(CONFIG_ZMK_DONGLE_DISPLAY_NAME) == false
#include "widgets/device_name.h"
static struct zmk_widget_device_name device_name_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
static struct zmk_widget_hid_indicators hid_indicators_widget;
#endif

lv_style_t global_style;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen;

    lv_style_init(&global_style);

    screen = lv_obj_create(NULL);

    lv_style_set_bg_color(&global_style, lv_color_white());
    lv_style_set_bg_opa(&global_style, LV_OPA_COVER);
    lv_style_set_text_color(&global_style, lv_color_black());
    lv_style_set_text_font(&global_style, &lv_font_unscii_8);

    lv_style_set_text_letter_space(&global_style, 1);
    lv_style_set_text_line_space(&global_style, 1);
    
    lv_obj_add_style(screen, &global_style, LV_PART_MAIN);

    // Also set the background color directly on the screen object
    // lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    // lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    
    zmk_widget_output_status_init(&output_status_widget, screen);
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget), LV_ALIGN_TOP_LEFT, PADDING_LEFT, 0);
    
    zmk_widget_bongo_cat_init(&bongo_cat_widget, screen);
    lv_obj_align(zmk_widget_bongo_cat_obj(&bongo_cat_widget), LV_ALIGN_BOTTOM_RIGHT, -PADDING_RIGHT, -7);

#if IS_EMPTY(CONFIG_ZMK_DONGLE_DISPLAY_NAME) == false
    zmk_widget_device_name_init(&device_name_widget, screen);
    lv_obj_align(zmk_widget_device_name_obj(&device_name_widget), LV_ALIGN_BOTTOM_RIGHT, -PADDING_RIGHT, 0);
#endif

    zmk_widget_modifiers_init(&modifiers_widget, screen);
    //lv_obj_align(zmk_widget_modifiers_obj(&modifiers_widget), LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_align(zmk_widget_modifiers_obj(&modifiers_widget), LV_ALIGN_BOTTOM_LEFT, PADDING_LEFT, -7);

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
    zmk_widget_hid_indicators_init(&hid_indicators_widget, screen);
    lv_obj_align_to(zmk_widget_hid_indicators_obj(&hid_indicators_widget), zmk_widget_modifiers_obj(&modifiers_widget), LV_ALIGN_OUT_TOP_LEFT, PADDING_LEFT, -2);
#endif

    zmk_widget_layer_status_init(&layer_status_widget, screen);
    lv_obj_align(zmk_widget_layer_status_obj(&layer_status_widget), LV_ALIGN_BOTTOM_LEFT, PADDING_LEFT, 0);
    // lv_obj_align_to(zmk_widget_layer_status_obj(&layer_status_widget), zmk_widget_bongo_cat_obj(&bongo_cat_widget), LV_ALIGN_BOTTOM_LEFT, 0, 5);

#if IS_ENABLED(CONFIG_ZMK_BATTERY)
    zmk_widget_dongle_battery_status_init(&dongle_battery_status_widget, screen);
    lv_obj_align(zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget), LV_ALIGN_TOP_RIGHT, -PADDING_RIGHT, 0);
#endif

    return screen;
}
