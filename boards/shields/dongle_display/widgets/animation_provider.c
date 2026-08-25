/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <lvgl.h>

#include <zmk/dongle_display/animation.h>

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_CUSTOM_ANIMATION_PROVIDER)

#include CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_PROVIDER_HEADER

#else

LV_IMG_DECLARE(bongo_cat_none);
LV_IMG_DECLARE(bongo_cat_left1);
LV_IMG_DECLARE(bongo_cat_left2);
LV_IMG_DECLARE(bongo_cat_right1);
LV_IMG_DECLARE(bongo_cat_right2);
LV_IMG_DECLARE(bongo_cat_both1);
LV_IMG_DECLARE(bongo_cat_both1_open);
LV_IMG_DECLARE(bongo_cat_both2);

static const void *const bongo_idle_frames[] = {
    &bongo_cat_both1_open,
    &bongo_cat_both1_open,
    &bongo_cat_both1_open,
    &bongo_cat_both1,
};

static const void *const bongo_slow_frames[] = {
    &bongo_cat_left1, &bongo_cat_both1, &bongo_cat_both1,
    &bongo_cat_right1, &bongo_cat_both1, &bongo_cat_both1,
    &bongo_cat_left1, &bongo_cat_both1, &bongo_cat_both1,
};

static const void *const bongo_mid_frames[] = {
    &bongo_cat_left2, &bongo_cat_left1, &bongo_cat_none,
    &bongo_cat_right2, &bongo_cat_right1, &bongo_cat_none,
};

static const void *const bongo_fast_frames[] = {
    &bongo_cat_both2,
    &bongo_cat_both1,
    &bongo_cat_none,
    &bongo_cat_none,
};

/* Match the original 128x64 widget's bottom-right alignment: 64 - 26 - 7 = 31. */
static const int8_t bongo_idle_y_offsets[] = {31, 31, 31, 31};
static const int8_t bongo_slow_y_offsets[] = {31, 31, 31, 31, 31, 31, 31, 31, 31};
static const int8_t bongo_mid_y_offsets[] = {31, 31, 31, 31, 31, 31};
static const int8_t bongo_fast_y_offsets[] = {31, 31, 31, 31};

ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_Y_OFFSETS_DEFINE(
    bongo_idle, bongo_idle_frames, NULL, NULL, bongo_idle_y_offsets, NULL,
    ZMK_DONGLE_ANIMATION_NO_RETURN_STEP, 10000, 0, ZMK_DONGLE_ANIMATION_MOTION_NONE, 0);
ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_Y_OFFSETS_DEFINE(
    bongo_slow, bongo_slow_frames, NULL, NULL, bongo_slow_y_offsets, NULL,
    ZMK_DONGLE_ANIMATION_NO_RETURN_STEP, 2000, 0, ZMK_DONGLE_ANIMATION_MOTION_NONE, 0);
ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_Y_OFFSETS_DEFINE(
    bongo_mid, bongo_mid_frames, NULL, NULL, bongo_mid_y_offsets, NULL,
    ZMK_DONGLE_ANIMATION_NO_RETURN_STEP, 500, 0, ZMK_DONGLE_ANIMATION_MOTION_NONE, 0);
ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_Y_OFFSETS_DEFINE(
    bongo_fast, bongo_fast_frames, NULL, NULL, bongo_fast_y_offsets, NULL,
    ZMK_DONGLE_ANIMATION_NO_RETURN_STEP, 200, 0, ZMK_DONGLE_ANIMATION_MOTION_NONE, 0);
ZMK_DONGLE_ANIMATION_PACK_WPM4_DEFINE(bongo_pack, "Bongo Cat", bongo_idle, bongo_slow,
                                      bongo_mid, bongo_fast, 5, 30, 70);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_FIGHTER_PACK)
/* Fighter theme demo: reuse the already linked Bongo Cat descriptors. */
static const void *const fighter_demo_idle_frames[] = {
    &bongo_cat_both1_open, &bongo_cat_both1_open, &bongo_cat_both1,
};
static const void *const fighter_demo_slow_frames[] = {
    &bongo_cat_left1, &bongo_cat_both1, &bongo_cat_right1, &bongo_cat_both1,
};
static const void *const fighter_demo_mid_frames[] = {
    &bongo_cat_both1_open, &bongo_cat_left1, &bongo_cat_both1, &bongo_cat_right1,
    &bongo_cat_none, &bongo_cat_left2, &bongo_cat_right2, &bongo_cat_both2,
};
static const uint8_t fighter_demo_mid_movement[] = {0, 0, 0, 0, 1, 1, 1, 1};
static const void *const fighter_demo_fast_frames[] = {
    &bongo_cat_both1_open, &bongo_cat_both1, &bongo_cat_both1_open, &bongo_cat_both1,
    &bongo_cat_both1_open, &bongo_cat_both1, &bongo_cat_both1_open, &bongo_cat_none,
    &bongo_cat_left2, &bongo_cat_right2, &bongo_cat_both2, &bongo_cat_both1,
};
static const uint8_t fighter_demo_fast_movement[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1};

ZMK_DONGLE_ANIMATION_ACTION_DEFINE(fighter_demo_idle, fighter_demo_idle_frames, 1500);
ZMK_DONGLE_ANIMATION_ACTION_DEFINE(fighter_demo_slow, fighter_demo_slow_frames, 600);
ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_CADENCE_MOVEMENT_DEFINE(
    fighter_demo_mid, fighter_demo_mid_frames, fighter_demo_mid_movement, 200, 500,
    ZMK_DONGLE_ANIMATION_MOTION_LEFT_SCREEN_THIRD,
    ZMK_DONGLE_ANIMATION_FLAG_FULLSCREEN | ZMK_DONGLE_ANIMATION_FLAG_BATTLE_HUD);
ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_CADENCE_MOVEMENT_DEFINE(
    fighter_demo_fast, fighter_demo_fast_frames, fighter_demo_fast_movement, 200, 500,
    ZMK_DONGLE_ANIMATION_MOTION_LEFT_EDGE,
    ZMK_DONGLE_ANIMATION_FLAG_FULLSCREEN | ZMK_DONGLE_ANIMATION_FLAG_BATTLE_HUD);
ZMK_DONGLE_ANIMATION_PACK_WPM4_DEFINE(fighter_demo_pack, "Fighter Demo", fighter_demo_idle,
                                      fighter_demo_slow, fighter_demo_mid, fighter_demo_fast,
                                      5, 30, 70);
ZMK_DONGLE_ANIMATION_REGISTRY_DEFINE(50, 26, bongo_pack, fighter_demo_pack);
#else
ZMK_DONGLE_ANIMATION_REGISTRY_DEFINE(50, 26, bongo_pack);
#endif

#endif
