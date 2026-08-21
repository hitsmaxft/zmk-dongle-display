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
