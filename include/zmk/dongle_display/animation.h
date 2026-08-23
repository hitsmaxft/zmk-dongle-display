/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <zephyr/sys/util.h>
#include <zephyr/sys/util_macro.h>

#define ZMK_DONGLE_ANIMATION_PROVIDER_ABI_VERSION 8
#define ZMK_DONGLE_ANIMATION_MAX_FRAMES 127
#define ZMK_DONGLE_ANIMATION_NO_RETURN_STEP UINT8_MAX

enum zmk_dongle_animation_motion {
    ZMK_DONGLE_ANIMATION_MOTION_NONE = 0,
    ZMK_DONGLE_ANIMATION_MOTION_LEFT_SCREEN_THIRD = 1,
    ZMK_DONGLE_ANIMATION_MOTION_LEFT_EDGE = 2,
};

#define ZMK_DONGLE_ANIMATION_FLAG_FULLSCREEN BIT(0)
#define ZMK_DONGLE_ANIMATION_FLAG_BATTLE_HUD BIT(1)
#define ZMK_DONGLE_ANIMATION_FLAGS_MASK                                                      \
    (ZMK_DONGLE_ANIMATION_FLAG_FULLSCREEN | ZMK_DONGLE_ANIMATION_FLAG_BATTLE_HUD)

struct zmk_dongle_animation_action {
    const char *name;
    const void *const *frames;
    const uint8_t *movement_steps;
    const int8_t *frame_x_offsets;
    const int8_t *frame_y_offsets;
    const uint16_t *frame_durations_ms;
    uint8_t frame_count;
    uint32_t duration_ms;
    uint8_t motion;
    uint8_t flags;
    uint8_t return_step;
    uint16_t endpoint_hold_ms;
};

struct zmk_dongle_animation_band {
    uint8_t min_wpm;
    uint8_t action_index;
};

struct zmk_dongle_animation_pack {
    const char *name;
    const struct zmk_dongle_animation_action *const *actions;
    size_t action_count;
    const struct zmk_dongle_animation_band *bands;
    size_t band_count;
};

struct zmk_dongle_animation_registry {
    uint16_t abi_version;
    uint16_t canvas_width;
    uint16_t canvas_height;
    const struct zmk_dongle_animation_pack *const *packs;
    size_t pack_count;
};

extern const struct zmk_dongle_animation_registry zmk_dongle_animation_registry;

void zmk_widget_dongle_animation_request_next(void);

#define ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_Y_OFFSETS_DEFINE(                                  \
    _id, _frames, _movement_steps, _frame_x_offsets, _frame_y_offsets,                        \
    _frame_durations_ms, _return_step, _duration_ms, _endpoint_hold_ms, _motion, _flags)      \
    _Static_assert(ARRAY_SIZE(_frames) > 0, #_id " must contain at least one frame");        \
    _Static_assert(ARRAY_SIZE(_frames) <= ZMK_DONGLE_ANIMATION_MAX_FRAMES,                     \
                   #_id " exceeds LVGL pic_count");                                          \
    _Static_assert(ARRAY_SIZE(_frame_y_offsets) == ARRAY_SIZE(_frames),                        \
                   #_id " frame Y offset table must match its frame count");                 \
    _Static_assert((_duration_ms) >= ARRAY_SIZE(_frames),                                      \
                   #_id " duration is shorter than its frame count");                         \
    _Static_assert((_endpoint_hold_ms) <= UINT16_MAX,                                         \
                   #_id " endpoint hold exceeds the provider ABI");                          \
    _Static_assert((_endpoint_hold_ms) == 0 || ARRAY_SIZE(_frames) >= 2,                       \
                   #_id " endpoint hold requires at least two frames");                      \
    _Static_assert((_motion) >= ZMK_DONGLE_ANIMATION_MOTION_NONE &&                            \
                       (_motion) <= ZMK_DONGLE_ANIMATION_MOTION_LEFT_EDGE,                     \
                   #_id " has an unknown motion");                                            \
    _Static_assert(((_flags) & ~ZMK_DONGLE_ANIMATION_FLAGS_MASK) == 0,                         \
                   #_id " has unknown flags");                                                \
    static const struct zmk_dongle_animation_action _id = {                                   \
        .name = #_id,                                                                          \
        .frames = (_frames),                                                                   \
        .movement_steps = (_movement_steps),                                                   \
        .frame_x_offsets = (_frame_x_offsets),                                                 \
        .frame_y_offsets = (_frame_y_offsets),                                                 \
        .frame_durations_ms = (_frame_durations_ms),                                           \
        .frame_count = ARRAY_SIZE(_frames),                                                    \
        .duration_ms = (_duration_ms),                                                         \
        .motion = (_motion),                                                                   \
        .flags = (_flags),                                                                     \
        .return_step = (_return_step),                                                         \
        .endpoint_hold_ms = (_endpoint_hold_ms),                                               \
    }

#define ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_MOVEMENT_RETURN_TIMING_DEFINE_COUNT(                  \
    _id, _frames, _movement_steps, _return_step, _count, _duration_ms, _endpoint_hold_ms,        \
    _motion, _flags)                                                                              \
    _Static_assert((_count) > 0, #_id " must contain at least one frame");                       \
    _Static_assert((_count) <= ZMK_DONGLE_ANIMATION_MAX_FRAMES,                                  \
                   #_id " exceeds LVGL pic_count");                                             \
    _Static_assert((_duration_ms) > 0, #_id " must have a nonzero duration");                    \
    _Static_assert((_endpoint_hold_ms) <= UINT16_MAX,                                             \
                   #_id " endpoint hold exceeds the provider ABI");                             \
    _Static_assert((_endpoint_hold_ms) == 0 || (_count) >= 2,                                     \
                   #_id " endpoint hold requires at least two frames");                         \
    _Static_assert((_endpoint_hold_ms) == 0 ||                                                    \
                       (_duration_ms) >= 2U * (_endpoint_hold_ms),                                 \
                   #_id " duration is shorter than its endpoint holds");                        \
    _Static_assert((_endpoint_hold_ms) == 0 || (_count) != 2 ||                                   \
                       (_duration_ms) == 2U * (_endpoint_hold_ms),                                 \
                   #_id " two-frame duration must equal its endpoint holds");                   \
    _Static_assert((_endpoint_hold_ms) == 0 || (_count) <= 2 ||                                   \
                       (_duration_ms) - 2U * (_endpoint_hold_ms) >= (_count) - 2U,                 \
                   #_id " middle animation duration is shorter than its frame count");          \
    _Static_assert((_motion) >= ZMK_DONGLE_ANIMATION_MOTION_NONE &&                               \
                       (_motion) <= ZMK_DONGLE_ANIMATION_MOTION_LEFT_EDGE,                         \
                   #_id " has an unknown motion");                                               \
    _Static_assert(((_flags) & ~ZMK_DONGLE_ANIMATION_FLAGS_MASK) == 0,                             \
                   #_id " has unknown flags");                                                    \
    _Static_assert((_motion) == ZMK_DONGLE_ANIMATION_MOTION_NONE || (_count) >= 2,                \
                   #_id " moving animation requires at least two frames");                       \
    _Static_assert((_motion) == ZMK_DONGLE_ANIMATION_MOTION_NONE ||                               \
                       (_duration_ms) >= (_count),                                                  \
                   #_id " moving animation duration is shorter than its frame count");           \
    static const struct zmk_dongle_animation_action _id = {                                      \
        .name = #_id,                                                                            \
        .frames = (_frames),                                                                      \
        .movement_steps = (_movement_steps),                                                       \
        .frame_x_offsets = NULL,                                                                   \
        .frame_durations_ms = NULL,                                                                \
        .frame_count = (_count),                                                                  \
        .duration_ms = (_duration_ms),                                                            \
        .motion = (_motion),                                                                       \
        .flags = (_flags),                                                                         \
        .return_step = (_return_step),                                                             \
        .endpoint_hold_ms = (_endpoint_hold_ms),                                                   \
    }

#define ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_CADENCE_OFFSETS_DEFINE(                                \
    _id, _frames, _frame_x_offsets, _middle_frame_ms, _endpoint_hold_ms, _flags)                  \
    _Static_assert(ARRAY_SIZE(_frame_x_offsets) == ARRAY_SIZE(_frames),                           \
                   #_id " frame offset table must match its frame count");                      \
    _Static_assert(ARRAY_SIZE(_frames) >= 2, #_id " offset cadence needs endpoint frames");      \
    _Static_assert(ARRAY_SIZE(_frames) <= ZMK_DONGLE_ANIMATION_MAX_FRAMES,                        \
                   #_id " exceeds LVGL pic_count");                                             \
    static const struct zmk_dongle_animation_action _id = {                                      \
        .name = #_id,                                                                             \
        .frames = (_frames),                                                                      \
        .movement_steps = NULL,                                                                   \
        .frame_x_offsets = (_frame_x_offsets),                                                     \
        .frame_durations_ms = NULL,                                                                \
        .frame_count = ARRAY_SIZE(_frames),                                                        \
        .duration_ms = 2U * (_endpoint_hold_ms) +                                                 \
                       (ARRAY_SIZE(_frames) - 2U) * (_middle_frame_ms),                            \
        .motion = ZMK_DONGLE_ANIMATION_MOTION_NONE,                                               \
        .flags = (_flags),                                                                        \
        .return_step = ZMK_DONGLE_ANIMATION_NO_RETURN_STEP,                                       \
        .endpoint_hold_ms = (_endpoint_hold_ms),                                                   \
    }

#define ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_TIMED_OFFSETS_DEFINE(                                 \
    _id, _frames, _frame_x_offsets, _frame_durations_ms, _duration_ms, _flags)                   \
    _Static_assert(ARRAY_SIZE(_frame_x_offsets) == ARRAY_SIZE(_frames),                           \
                   #_id " frame offset table must match its frame count");                      \
    _Static_assert(ARRAY_SIZE(_frame_durations_ms) == ARRAY_SIZE(_frames),                        \
                   #_id " frame duration table must match its frame count");                    \
    _Static_assert(ARRAY_SIZE(_frames) > 0, #_id " must contain at least one frame");            \
    _Static_assert(ARRAY_SIZE(_frames) <= ZMK_DONGLE_ANIMATION_MAX_FRAMES,                        \
                   #_id " exceeds LVGL pic_count");                                             \
    _Static_assert((_duration_ms) >= ARRAY_SIZE(_frames),                                         \
                   #_id " duration is shorter than its frame count");                           \
    static const struct zmk_dongle_animation_action _id = {                                      \
        .name = #_id,                                                                             \
        .frames = (_frames),                                                                      \
        .movement_steps = NULL,                                                                   \
        .frame_x_offsets = (_frame_x_offsets),                                                     \
        .frame_durations_ms = (_frame_durations_ms),                                              \
        .frame_count = ARRAY_SIZE(_frames),                                                        \
        .duration_ms = (_duration_ms),                                                             \
        .motion = ZMK_DONGLE_ANIMATION_MOTION_NONE,                                               \
        .flags = (_flags),                                                                        \
        .return_step = ZMK_DONGLE_ANIMATION_NO_RETURN_STEP,                                       \
        .endpoint_hold_ms = 0,                                                                    \
    }

#define ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_TIMED_MOVEMENT_RETURN_DEFINE_COUNT(                  \
    _id, _frames, _movement_steps, _frame_durations_ms, _return_step, _count,                  \
    _duration_ms, _motion, _flags)                                                               \
    _Static_assert((_count) > 0, #_id " must contain at least one frame");                    \
    _Static_assert((_count) <= ZMK_DONGLE_ANIMATION_MAX_FRAMES,                                 \
                   #_id " exceeds LVGL pic_count");                                          \
    _Static_assert((_duration_ms) >= (_count),                                                   \
                   #_id " duration is shorter than its frame count");                        \
    _Static_assert((_motion) >= ZMK_DONGLE_ANIMATION_MOTION_NONE &&                              \
                       (_motion) <= ZMK_DONGLE_ANIMATION_MOTION_LEFT_EDGE,                       \
                   #_id " has an unknown motion");                                            \
    _Static_assert(((_flags) & ~ZMK_DONGLE_ANIMATION_FLAGS_MASK) == 0,                           \
                   #_id " has unknown flags");                                                 \
    static const struct zmk_dongle_animation_action _id = {                                     \
        .name = #_id,                                                                            \
        .frames = (_frames),                                                                     \
        .movement_steps = (_movement_steps),                                                     \
        .frame_x_offsets = NULL,                                                                 \
        .frame_durations_ms = (_frame_durations_ms),                                             \
        .frame_count = (_count),                                                                 \
        .duration_ms = (_duration_ms),                                                           \
        .motion = (_motion),                                                                     \
        .flags = (_flags),                                                                       \
        .return_step = (_return_step),                                                           \
        .endpoint_hold_ms = 0,                                                                   \
    }

#define ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_TIMED_DEFINE(                                        \
    _id, _frames, _frame_durations_ms, _duration_ms, _motion, _flags)                           \
    _Static_assert(ARRAY_SIZE(_frame_durations_ms) == ARRAY_SIZE(_frames),                       \
                   #_id " frame duration table must match its frame count");                 \
    ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_TIMED_MOVEMENT_RETURN_DEFINE_COUNT(                       \
        _id, _frames, NULL, _frame_durations_ms, ZMK_DONGLE_ANIMATION_NO_RETURN_STEP,           \
        ARRAY_SIZE(_frames), _duration_ms, _motion, _flags)

#define ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_TIMED_MOVEMENT_DEFINE(                               \
    _id, _frames, _movement_steps, _frame_durations_ms, _duration_ms, _motion, _flags)          \
    _Static_assert(ARRAY_SIZE(_movement_steps) == ARRAY_SIZE(_frames),                           \
                   #_id " movement table must match its frame count");                       \
    _Static_assert(ARRAY_SIZE(_frame_durations_ms) == ARRAY_SIZE(_frames),                       \
                   #_id " frame duration table must match its frame count");                 \
    ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_TIMED_MOVEMENT_RETURN_DEFINE_COUNT(                       \
        _id, _frames, _movement_steps, _frame_durations_ms,                                     \
        ZMK_DONGLE_ANIMATION_NO_RETURN_STEP, ARRAY_SIZE(_frames), _duration_ms, _motion, _flags)

#define ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_TIMED_MOVEMENT_RETURN_DEFINE(                        \
    _id, _frames, _movement_steps, _frame_durations_ms, _return_step, _duration_ms,             \
    _motion, _flags)                                                                              \
    _Static_assert(ARRAY_SIZE(_movement_steps) == ARRAY_SIZE(_frames),                           \
                   #_id " movement table must match its frame count");                       \
    _Static_assert(ARRAY_SIZE(_frame_durations_ms) == ARRAY_SIZE(_frames),                       \
                   #_id " frame duration table must match its frame count");                 \
    _Static_assert((_return_step) > 0 && (_return_step) < ARRAY_SIZE(_frames),                   \
                   #_id " return step must be inside its frame table");                      \
    ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_TIMED_MOVEMENT_RETURN_DEFINE_COUNT(                       \
        _id, _frames, _movement_steps, _frame_durations_ms, _return_step,                       \
        ARRAY_SIZE(_frames), _duration_ms, _motion, _flags)

#define ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_MOVEMENT_TIMING_DEFINE_COUNT(                         \
    _id, _frames, _movement_steps, _count, _duration_ms, _endpoint_hold_ms, _motion, _flags)     \
    ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_MOVEMENT_RETURN_TIMING_DEFINE_COUNT(                       \
        _id, _frames, _movement_steps, ZMK_DONGLE_ANIMATION_NO_RETURN_STEP, _count,              \
        _duration_ms, _endpoint_hold_ms, _motion, _flags)

#define ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_TIMING_DEFINE_COUNT(                                  \
    _id, _frames, _count, _duration_ms, _endpoint_hold_ms, _motion, _flags)                       \
    ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_MOVEMENT_TIMING_DEFINE_COUNT(                              \
        _id, _frames, NULL, _count, _duration_ms, _endpoint_hold_ms, _motion, _flags)

#define ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_DEFINE_COUNT(_id, _frames, _count, _duration_ms,       \
                                                         _motion, _flags)                           \
    ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_TIMING_DEFINE_COUNT(                                       \
        _id, _frames, _count, _duration_ms, 0, _motion, _flags)

#define ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_CADENCE_DEFINE_COUNT(                                  \
    _id, _frames, _count, _middle_frame_ms, _endpoint_hold_ms, _motion, _flags)                   \
    ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_TIMING_DEFINE_COUNT(                                       \
        _id, _frames, _count,                                                                     \
        2U * (_endpoint_hold_ms) + ((_count) - 2U) * (_middle_frame_ms),                          \
        _endpoint_hold_ms, _motion, _flags)

#define ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_CADENCE_MOVEMENT_DEFINE_COUNT(                         \
    _id, _frames, _movement_steps, _count, _middle_frame_ms, _endpoint_hold_ms, _motion, _flags)  \
    _Static_assert(ARRAY_SIZE(_movement_steps) == (_count),                                       \
                   #_id " movement table must match its frame count");                          \
    ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_MOVEMENT_TIMING_DEFINE_COUNT(                              \
        _id, _frames, _movement_steps, _count,                                                    \
        2U * (_endpoint_hold_ms) + ((_count) - 2U) * (_middle_frame_ms),                          \
        _endpoint_hold_ms, _motion, _flags)

#define ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_CADENCE_MOVEMENT_RETURN_DEFINE_COUNT(                 \
    _id, _frames, _movement_steps, _return_step, _count, _middle_frame_ms,                       \
    _endpoint_hold_ms, _motion, _flags)                                                           \
    _Static_assert(ARRAY_SIZE(_movement_steps) == (_count),                                       \
                   #_id " movement table must match its frame count");                          \
    _Static_assert((_return_step) > 0 && (_return_step) < (_count),                              \
                   #_id " return step must be inside its frame table");                         \
    ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_MOVEMENT_RETURN_TIMING_DEFINE_COUNT(                       \
        _id, _frames, _movement_steps, _return_step, _count,                                     \
        2U * (_endpoint_hold_ms) + ((_count) - 2U) * (_middle_frame_ms),                         \
        _endpoint_hold_ms, _motion, _flags)

#define ZMK_DONGLE_ANIMATION_ACTION_DEFINE_COUNT(_id, _frames, _count, _duration_ms)              \
    ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_DEFINE_COUNT(                                               \
        _id, _frames, _count, _duration_ms, ZMK_DONGLE_ANIMATION_MOTION_NONE, 0)

#define ZMK_DONGLE_ANIMATION_ACTION_DEFINE(_id, _frames, _duration_ms)                            \
    ZMK_DONGLE_ANIMATION_ACTION_DEFINE_COUNT(_id, _frames, ARRAY_SIZE(_frames), _duration_ms)

#define ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_DEFINE(_id, _frames, _duration_ms, _motion, _flags)    \
    ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_DEFINE_COUNT(                                               \
        _id, _frames, ARRAY_SIZE(_frames), _duration_ms, _motion, _flags)

#define ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_CADENCE_DEFINE(                                        \
    _id, _frames, _middle_frame_ms, _endpoint_hold_ms, _motion, _flags)                           \
    ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_CADENCE_DEFINE_COUNT(                                      \
        _id, _frames, ARRAY_SIZE(_frames), _middle_frame_ms, _endpoint_hold_ms, _motion, _flags)

#define ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_CADENCE_MOVEMENT_DEFINE(                               \
    _id, _frames, _movement_steps, _middle_frame_ms, _endpoint_hold_ms, _motion, _flags)          \
    ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_CADENCE_MOVEMENT_DEFINE_COUNT(                             \
        _id, _frames, _movement_steps, ARRAY_SIZE(_frames), _middle_frame_ms,                     \
        _endpoint_hold_ms, _motion, _flags)

#define ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_CADENCE_MOVEMENT_RETURN_DEFINE(                        \
    _id, _frames, _movement_steps, _return_step, _middle_frame_ms, _endpoint_hold_ms,            \
    _motion, _flags)                                                                               \
    ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_CADENCE_MOVEMENT_RETURN_DEFINE_COUNT(                      \
        _id, _frames, _movement_steps, _return_step, ARRAY_SIZE(_frames), _middle_frame_ms,      \
        _endpoint_hold_ms, _motion, _flags)

#define ZMK_DONGLE_ANIMATION_PACK_DEFINE(_id, _label, _actions, _bands)                           \
    _Static_assert(ARRAY_SIZE(_actions) > 0, #_id " must contain at least one action");           \
    _Static_assert(ARRAY_SIZE(_bands) > 0, #_id " must contain at least one WPM band");           \
    static const struct zmk_dongle_animation_pack _id = {                                        \
        .name = (_label),                                                                         \
        .actions = (_actions),                                                                    \
        .action_count = ARRAY_SIZE(_actions),                                                      \
        .bands = (_bands),                                                                        \
        .band_count = ARRAY_SIZE(_bands),                                                          \
    }

#define ZMK_DONGLE_ANIMATION_PACK_WPM4_DEFINE(_id, _label, _idle, _slow, _mid, _fast,             \
                                               _slow_wpm, _mid_wpm, _fast_wpm)                     \
    _Static_assert((_slow_wpm) > 0 && (_slow_wpm) < (_mid_wpm) &&                                \
                       (_mid_wpm) < (_fast_wpm) && (_fast_wpm) <= UINT8_MAX,                       \
                   #_id " WPM thresholds must be strictly increasing");                          \
    static const struct zmk_dongle_animation_action *const _id##_actions[] = {                    \
        &(_idle), &(_slow), &(_mid), &(_fast)};                                                    \
    static const struct zmk_dongle_animation_band _id##_bands[] = {                               \
        {.min_wpm = 0, .action_index = 0},                                                        \
        {.min_wpm = (_slow_wpm), .action_index = 1},                                              \
        {.min_wpm = (_mid_wpm), .action_index = 2},                                               \
        {.min_wpm = (_fast_wpm), .action_index = 3},                                              \
    };                                                                                             \
    ZMK_DONGLE_ANIMATION_PACK_DEFINE(_id, _label, _id##_actions, _id##_bands)

#define ZMK_DONGLE_ANIMATION_REGISTRY_ARRAY_DEFINE(_width, _height, _packs)                       \
    _Static_assert((_width) > 0 && (_height) > 0, "animation canvas must be nonzero");            \
    _Static_assert((_width) <= UINT16_MAX && (_height) <= UINT16_MAX,                              \
                   "animation canvas exceeds the provider ABI");                                 \
    _Static_assert(ARRAY_SIZE(_packs) > 0, "animation registry must contain at least one pack"); \
    const struct zmk_dongle_animation_registry zmk_dongle_animation_registry = {                  \
        .abi_version = ZMK_DONGLE_ANIMATION_PROVIDER_ABI_VERSION,                                 \
        .canvas_width = (_width),                                                                  \
        .canvas_height = (_height),                                                                \
        .packs = (_packs),                                                                         \
        .pack_count = ARRAY_SIZE(_packs),                                                          \
    }

#define ZMK_DONGLE_ANIMATION_PACK_ADDRESS(_pack) &(_pack)

#define ZMK_DONGLE_ANIMATION_REGISTRY_DEFINE(_width, _height, ...)                                \
    static const struct zmk_dongle_animation_pack *const zmk_dongle_animation_provider_packs[] = {\
        FOR_EACH(ZMK_DONGLE_ANIMATION_PACK_ADDRESS, (,), __VA_ARGS__)};                            \
    ZMK_DONGLE_ANIMATION_REGISTRY_ARRAY_DEFINE(_width, _height,                                   \
                                                zmk_dongle_animation_provider_packs)
