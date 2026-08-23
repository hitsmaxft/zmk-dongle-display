/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

static inline size_t zmk_dongle_animation_start_index(uint32_t random_value,
                                                       size_t pack_count) {
    return pack_count > 0 ? random_value % pack_count : 0;
}

static inline size_t zmk_dongle_animation_next_index(size_t current_index, size_t pack_count) {
    if (pack_count <= 1) {
        return 0;
    }

    return (current_index + 1) % pack_count;
}

static inline uint32_t zmk_dongle_animation_frame_period(uint32_t duration_ms,
                                                          uint8_t frame_count,
                                                          uint8_t frame_index,
                                                          uint16_t endpoint_hold_ms) {
    if (frame_count == 0 || frame_index >= frame_count) {
        return 0;
    }

    if (endpoint_hold_ms > 0) {
        uint32_t endpoint_duration = 2U * endpoint_hold_ms;
        if (frame_count < 2 || duration_ms < endpoint_duration ||
            (frame_count == 2 && duration_ms != endpoint_duration)) {
            return 0;
        }
        if (frame_index == 0 || frame_index == frame_count - 1) {
            return endpoint_hold_ms;
        }

        uint8_t middle_count = frame_count - 2;
        uint8_t middle_index = frame_index - 1;
        uint32_t middle_duration = duration_ms - endpoint_duration;
        uint32_t base = middle_duration / middle_count;
        uint32_t remainder = middle_duration % middle_count;
        return base + (middle_index < remainder ? 1U : 0U);
    }

    uint32_t base = duration_ms / frame_count;
    uint32_t remainder = duration_ms % frame_count;
    return base + (frame_index < remainder ? 1U : 0U);
}

static inline int32_t zmk_dongle_animation_origin_x(int32_t screen_width,
                                                     int32_t canvas_width) {
    return screen_width > canvas_width ? screen_width - canvas_width : 0;
}

static inline int32_t zmk_dongle_animation_target_x(int32_t screen_width,
                                                     int32_t canvas_width,
                                                     uint8_t motion) {
    int32_t origin = zmk_dongle_animation_origin_x(screen_width, canvas_width);

    if (motion == 1) {
        int32_t distance = (screen_width + 1) / 3;
        return origin > distance ? origin - distance : 0;
    }
    if (motion == 2) {
        return 0;
    }
    return origin;
}

static inline int32_t zmk_dongle_animation_frame_x(int32_t origin_x, int32_t target_x,
                                                    uint8_t frame_count,
                                                    uint8_t frame_index) {
    if (frame_count <= 1 || frame_index == 0 || origin_x <= target_x) {
        return origin_x;
    }
    if (frame_index >= frame_count - 1) {
        return target_x;
    }

    int32_t distance = origin_x - target_x;
    int32_t intervals = frame_count - 1;
    return origin_x - ((distance * frame_index + intervals / 2) / intervals);
}
