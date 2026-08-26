/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "custom_status_screen.h"
#include "widgets/battery_status.h"
#include "widgets/modifiers.h"
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_EXTENSION)
#include "widgets/animation.h"
#include "widgets/battle_battery.h"
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_CHARGE_MODE)
#include "widgets/fighter_charge.h"
#endif
#else
#include "widgets/bongo_cat.h"
#endif
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
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_EXTENSION)
static struct zmk_widget_dongle_animation animation_widget;
static struct zmk_widget_battle_battery battle_battery_widget;
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_CHARGE_MODE)
static struct zmk_widget_fighter_charge fighter_charge_widget;
#endif
#else
static struct zmk_widget_bongo_cat bongo_cat_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
static struct zmk_widget_hid_indicators hid_indicators_widget;
#endif

lv_style_t global_style;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen;
    lv_obj_t *normal_layer;
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_EXTENSION)
    lv_obj_t *battle_hud_layer;
#endif
    lv_obj_t *status_panel;

    lv_style_init(&global_style);

    screen = lv_obj_create(NULL);

    lv_style_set_bg_color(&global_style, lv_color_white());
    lv_style_set_bg_opa(&global_style, LV_OPA_COVER);
    lv_style_set_text_color(&global_style, lv_color_black());
    lv_style_set_text_font(&global_style, &lv_font_unscii_8);

    lv_style_set_text_letter_space(&global_style, 1);
    lv_style_set_text_line_space(&global_style, 1);
    
    lv_obj_add_style(screen, &global_style, LV_PART_MAIN);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_EXTENSION)
    /* apply_mode() enforces bottom-to-top: normal status, battle HUD, character ink. */
    /* The widget itself is the top-only HUD layer; avoid an extra LVGL wrapper object. */
    zmk_widget_battle_battery_init(&battle_battery_widget, screen);
    battle_hud_layer = zmk_widget_battle_battery_obj(&battle_battery_widget);
    lv_obj_add_flag(battle_hud_layer, LV_OBJ_FLAG_HIDDEN);
#endif

    normal_layer = lv_obj_create(screen);
    lv_obj_remove_style_all(normal_layer);
    lv_obj_set_size(normal_layer, 128, 64);
    lv_obj_align(normal_layer, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(normal_layer, LV_OBJ_FLAG_SCROLLABLE);

    /* Keep every status widget clipped to the left half of the 128x64 panel. */
    status_panel = lv_obj_create(normal_layer);
    lv_obj_remove_style_all(status_panel);
    lv_obj_set_size(status_panel, 64, 64);
    lv_obj_align(status_panel, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(status_panel, LV_OBJ_FLAG_SCROLLABLE);

    zmk_widget_output_status_init(&output_status_widget, status_panel);
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget), LV_ALIGN_TOP_LEFT,
                 PADDING_LEFT, 0);
    
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_EXTENSION)
    int animation_err = zmk_widget_dongle_animation_init(
        &animation_widget, screen, normal_layer, battle_hud_layer);
    if (animation_err < 0) {
        LOG_ERR("Failed to initialize dongle animation: %d", animation_err);
    }
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_CHARGE_MODE)
    int charge_err = zmk_widget_fighter_charge_init(&fighter_charge_widget, screen);
    if (charge_err < 0) {
        LOG_ERR("Failed to initialize fighter charge HUD: %d", charge_err);
    }
#endif
    if (animation_err >= 0) {
        lv_obj_move_foreground(zmk_widget_dongle_animation_obj(&animation_widget));
    }
#else
    zmk_widget_bongo_cat_init(&bongo_cat_widget, normal_layer);
    lv_obj_align(zmk_widget_bongo_cat_obj(&bongo_cat_widget), LV_ALIGN_BOTTOM_RIGHT,
                 -PADDING_RIGHT, -7);
#endif

    zmk_widget_modifiers_init(&modifiers_widget, status_panel);
    lv_obj_align(zmk_widget_modifiers_obj(&modifiers_widget), LV_ALIGN_TOP_LEFT, PADDING_LEFT,
                 37);

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
    zmk_widget_hid_indicators_init(&hid_indicators_widget, status_panel);
    lv_obj_align(zmk_widget_hid_indicators_obj(&hid_indicators_widget), LV_ALIGN_TOP_RIGHT, 0,
                 56);
#endif

    zmk_widget_layer_status_init(&layer_status_widget, status_panel);
    lv_obj_set_width(zmk_widget_layer_status_obj(&layer_status_widget), 32);
    lv_obj_align(zmk_widget_layer_status_obj(&layer_status_widget), LV_ALIGN_TOP_LEFT,
                 PADDING_LEFT, 56);

#if IS_ENABLED(CONFIG_ZMK_BATTERY)
    zmk_widget_dongle_battery_status_init(&dongle_battery_status_widget, normal_layer);
    lv_obj_align(zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget),
                 LV_ALIGN_TOP_RIGHT, -PADDING_RIGHT, 0);
#endif

    return screen;
}
