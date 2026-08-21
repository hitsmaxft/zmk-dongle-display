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

ZMK_DONGLE_ANIMATION_ACTION_DEFINE(bongo_idle, bongo_idle_frames, 10000);
ZMK_DONGLE_ANIMATION_ACTION_DEFINE(bongo_slow, bongo_slow_frames, 2000);
ZMK_DONGLE_ANIMATION_ACTION_DEFINE(bongo_mid, bongo_mid_frames, 500);
ZMK_DONGLE_ANIMATION_ACTION_DEFINE(bongo_fast, bongo_fast_frames, 200);
ZMK_DONGLE_ANIMATION_PACK_WPM4_DEFINE(bongo_pack, "Bongo Cat", bongo_idle, bongo_slow,
                                      bongo_mid, bongo_fast, 5, 30, 70);
ZMK_DONGLE_ANIMATION_REGISTRY_DEFINE(50, 26, bongo_pack);

#endif
