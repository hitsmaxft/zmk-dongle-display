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

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_FIGHTER_PACK)
#include "fighter_images.h"
#endif

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

ZMK_DONGLE_ANIMATION_ACTION_DEFINE(bongo_idle, bongo_idle_frames, 10000);
ZMK_DONGLE_ANIMATION_ACTION_DEFINE(bongo_slow, bongo_slow_frames, 2000);
ZMK_DONGLE_ANIMATION_ACTION_DEFINE(bongo_mid, bongo_mid_frames, 500);
ZMK_DONGLE_ANIMATION_ACTION_DEFINE(bongo_fast, bongo_fast_frames, 200);
ZMK_DONGLE_ANIMATION_PACK_WPM4_DEFINE(bongo_pack, "Bongo Cat", bongo_idle, bongo_slow,
                                      bongo_mid, bongo_fast, 5, 30, 70);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_FIGHTER_PACK)
static const void *const fighter_idle_frames[] = {
    &fighter_idle_0, &fighter_idle_1, &fighter_idle_2, &fighter_idle_3,
};
static const void *const fighter_slow_frames[] = {
    &fighter_kick_l_0, &fighter_kick_l_1, &fighter_kick_l_2,
    &fighter_kick_l_1, &fighter_kick_l_0, &fighter_kick_l_1,
};
static const void *const fighter_mid_frames[] = {
    &fighter_roll_f_0, &fighter_roll_f_1, &fighter_roll_f_2, &fighter_roll_f_3,
    &fighter_roll_f_4, &fighter_roll_f_3, &fighter_roll_f_2, &fighter_roll_f_1,
};
static const void *const fighter_fast_frames[] = {
    &fighter_oni_yaki_l_0, &fighter_oni_yaki_l_1, &fighter_oni_yaki_l_2,
    &fighter_oni_yaki_l_3, &fighter_oni_yaki_l_4, &fighter_oni_yaki_l_5,
    &fighter_oni_yaki_l_6, &fighter_oni_yaki_l_5,
};

ZMK_DONGLE_ANIMATION_ACTION_DEFINE(fighter_idle, fighter_idle_frames, 800);
ZMK_DONGLE_ANIMATION_ACTION_DEFINE(fighter_slow, fighter_slow_frames, 600);
ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_CADENCE_DEFINE(
    fighter_mid, fighter_mid_frames, 200, 500, ZMK_DONGLE_ANIMATION_MOTION_LEFT_SCREEN_THIRD,
    ZMK_DONGLE_ANIMATION_FLAG_FULLSCREEN | ZMK_DONGLE_ANIMATION_FLAG_BATTLE_HUD);
ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_CADENCE_DEFINE(
    fighter_fast, fighter_fast_frames, 200, 500, ZMK_DONGLE_ANIMATION_MOTION_LEFT_EDGE,
    ZMK_DONGLE_ANIMATION_FLAG_FULLSCREEN | ZMK_DONGLE_ANIMATION_FLAG_BATTLE_HUD);
ZMK_DONGLE_ANIMATION_PACK_WPM4_DEFINE(fighter_pack, "Fighter", fighter_idle, fighter_slow,
                                      fighter_mid, fighter_fast, 5, 30, 70);
ZMK_DONGLE_ANIMATION_REGISTRY_DEFINE(50, 26, bongo_pack, fighter_pack);
#else
ZMK_DONGLE_ANIMATION_REGISTRY_DEFINE(50, 26, bongo_pack);
#endif

#endif
