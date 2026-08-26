/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <limits.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/activity.h>
#include <zmk/display.h>
#include <zmk/dongle_display/animation.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/events/wpm_state_changed.h>
#include "animation.h"
#include "animation_math.h"
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_CHARGE_MODE)
#include "fighter_charge.h"
#endif

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
struct dongle_animation_wpm_state { uint8_t wpm; };
static size_t current_pack_index;
static __noinit volatile uint32_t boot_nonce;
static uint8_t latest_wpm;
#if !IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_DEMO_MODE)
static atomic_t next_requests;
#endif
static bool registry_initialized;
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_ROTATE_ON_WAKE)
static bool activity_was_inactive;
#endif

static int validate_registry(void) {
    const struct zmk_dongle_animation_registry *registry = &zmk_dongle_animation_registry;
    if (registry->abi_version != ZMK_DONGLE_ANIMATION_PROVIDER_ABI_VERSION) {
        LOG_ERR("Animation provider ABI %u is unsupported; rebuild for ABI %u",
                registry->abi_version, ZMK_DONGLE_ANIMATION_PROVIDER_ABI_VERSION);
        return -EPROTONOSUPPORT;
    }
    if (registry->canvas_width == 0 || registry->canvas_height == 0 || registry->packs == NULL ||
        registry->pack_count == 0) {
        LOG_ERR("Animation registry has an invalid canvas or pack table");
        return -EINVAL;
    }
    return 0;
}

static int validate_pack(const struct zmk_dongle_animation_pack *pack) {
    if (pack == NULL || pack->name == NULL || pack->actions == NULL || pack->action_count == 0 ||
        pack->bands == NULL || pack->band_count == 0) {
        LOG_ERR("Animation pack is invalid");
        return -EINVAL;
    }
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_CHARGE_MODE)
    if (pack->band_count <= ZMK_DONGLE_ANIMATION_FAST_BAND) {
        LOG_ERR("Animation charge mode requires four bands; pack %s has %u", pack->name,
                (unsigned int)pack->band_count);
        return -EINVAL;
    }
#endif
    uint8_t previous_min_wpm = 0;
    for (size_t index = 0; index < pack->band_count; index++) {
        const struct zmk_dongle_animation_band *band = &pack->bands[index];
        if ((index == 0 && band->min_wpm != 0) ||
            (index > 0 && band->min_wpm <= previous_min_wpm) ||
            band->action_index >= pack->action_count) {
            LOG_ERR("Animation pack %s has an invalid WPM band", pack->name);
            return -EINVAL;
        }
        previous_min_wpm = band->min_wpm;
    }
    return 0;
}

static int validate_action(const struct zmk_widget_dongle_animation *widget,
                           const struct zmk_dongle_animation_action *action) {
    if (action == NULL || action->name == NULL || action->frames == NULL ||
        action->frame_count == 0 || action->frame_count > ZMK_DONGLE_ANIMATION_MAX_FRAMES ||
        action->duration_ms < action->frame_count) {
        LOG_ERR("Animation action is invalid or has a sub-millisecond frame");
        return -EINVAL;
    }
    if (action->frame_roles != NULL) {
        if (action->frame_roles[0] != ZMK_DONGLE_ANIMATION_FRAME_ROLE_CHARACTER) {
            LOG_ERR("Animation action %s projectile track must start with a character frame",
                    action->name);
            return -EINVAL;
        }
        for (size_t index = 0; index < action->frame_count; index++) {
            if (action->frame_roles[index] > ZMK_DONGLE_ANIMATION_FRAME_ROLE_PROJECTILE) {
                LOG_ERR("Animation action %s frame %u has an invalid role", action->name,
                        (unsigned int)index);
                return -EINVAL;
            }
        }
        if (widget->projectile_obj == NULL) {
            LOG_ERR("Animation action %s requires a projectile image object", action->name);
            return -ENOMEM;
        }
    }
    uint32_t endpoint_duration = 2U * action->endpoint_hold_ms;
    if (action->frame_durations_ms != NULL) {
        uint32_t duration_sum = 0;
        if (action->endpoint_hold_ms != 0) {
            LOG_ERR("Animation action %s mixes explicit timing with endpoint holds", action->name);
            return -EINVAL;
        }
        for (size_t index = 0; index < action->frame_count; index++) {
            uint16_t frame_duration = action->frame_durations_ms[index];
            if (frame_duration == 0 || UINT32_MAX - duration_sum < frame_duration) {
                LOG_ERR("Animation action %s frame %u has an invalid duration", action->name,
                        (unsigned int)index);
                return -EINVAL;
            }
            duration_sum += frame_duration;
        }
        if (duration_sum != action->duration_ms) {
            LOG_ERR("Animation action %s frame durations total %u, expected %u", action->name,
                    duration_sum, action->duration_ms);
            return -EINVAL;
        }
    }
    if (action->endpoint_hold_ms > 0 &&
        (action->frame_count < 2 || action->duration_ms < endpoint_duration ||
         (action->frame_count == 2 && action->duration_ms != endpoint_duration) ||
         (action->frame_count > 2 &&
          action->duration_ms - endpoint_duration < action->frame_count - 2U))) {
        LOG_ERR("Animation action %s has invalid endpoint hold timing", action->name);
        return -EINVAL;
    }
    if (action->motion > ZMK_DONGLE_ANIMATION_MOTION_LEFT_EDGE ||
        (action->flags & ~ZMK_DONGLE_ANIMATION_FLAGS_MASK) != 0) {
        LOG_ERR("Animation action %s has unknown motion or flags", action->name);
        return -EINVAL;
    }
    const lv_image_dsc_t *first_frame = action->frames[0];
    if (first_frame == NULL || first_frame->header.w == 0 || first_frame->header.h == 0) {
        LOG_ERR("Animation action %s has an invalid first frame", action->name);
        return -EINVAL;
    }
    int32_t origin_x = zmk_dongle_animation_origin_x(widget->screen_width, first_frame->header.w);
    int32_t target_x = zmk_dongle_animation_target_x(
        widget->screen_width, first_frame->header.w, action->motion);
    int32_t distance = origin_x - target_x;
    uint8_t movement_count =
        zmk_dongle_animation_movement_count(action->movement_steps, action->frame_count);
    if (action->movement_steps != NULL) {
        if (action->movement_steps[0] != 0) {
            LOG_ERR("Animation action %s must keep its first movement step fixed", action->name);
            return -EINVAL;
        }
        for (size_t index = 1; index < action->frame_count; index++) {
            if (action->movement_steps[index] > 1) {
                LOG_ERR("Animation action %s movement step %u is invalid", action->name,
                        (unsigned int)index);
                return -EINVAL;
            }
        }
    }
    if (action->frame_x_offsets != NULL &&
        (action->movement_steps != NULL || action->motion != ZMK_DONGLE_ANIMATION_MOTION_NONE ||
         action->return_step != ZMK_DONGLE_ANIMATION_NO_RETURN_STEP)) {
        LOG_ERR("Animation action %s mixes explicit offsets with computed motion", action->name);
        return -EINVAL;
    }
    if ((action->motion == ZMK_DONGLE_ANIMATION_MOTION_NONE &&
         action->movement_steps != NULL) ||
        (action->motion != ZMK_DONGLE_ANIMATION_MOTION_NONE && movement_count == 0)) {
        LOG_ERR("Animation action %s has an incompatible movement table", action->name);
        return -EINVAL;
    }
    if (action->return_step != ZMK_DONGLE_ANIMATION_NO_RETURN_STEP) {
        if (action->movement_steps == NULL || action->return_step == 0 ||
            action->return_step >= action->frame_count ||
            zmk_dongle_animation_movement_count_range(
                action->movement_steps, 1, action->return_step) == 0 ||
            zmk_dongle_animation_movement_count_range(
                action->movement_steps, action->return_step, action->frame_count) == 0) {
            LOG_ERR("Animation action %s has an invalid return step", action->name);
            return -EINVAL;
        }
        movement_count = MAX(
            zmk_dongle_animation_movement_count_range(
                action->movement_steps, 1, action->return_step),
            zmk_dongle_animation_movement_count_range(
                action->movement_steps, action->return_step, action->frame_count));
    }
    if (action->motion != ZMK_DONGLE_ANIMATION_MOTION_NONE &&
        (action->frame_count < 2 || distance < movement_count)) {
        LOG_ERR("Animation action %s cannot move %u steps through %dpx", action->name,
                movement_count, distance);
        return -ERANGE;
    }
    for (size_t index = 0; index < action->frame_count; index++) {
        const lv_image_dsc_t *frame = action->frames[index];
        if (frame == NULL || frame->header.w == 0 || frame->header.h == 0 ||
            frame->header.w > zmk_dongle_animation_registry.canvas_width ||
            frame->header.h > zmk_dongle_animation_registry.canvas_height ||
            frame->header.w != first_frame->header.w ||
            frame->header.h != first_frame->header.h) {
            LOG_ERR("Animation action %s frame %u has an invalid or inconsistent size for %ux%u",
                    action->name,
                    (unsigned int)index, zmk_dongle_animation_registry.canvas_width,
                    zmk_dongle_animation_registry.canvas_height);
            return -EINVAL;
        }
        if (action->frame_x_offsets != NULL) {
            int32_t frame_x = origin_x + action->frame_x_offsets[index];
            if (frame_x < 0 || frame_x + frame->header.w > widget->screen_width) {
                LOG_ERR("Animation action %s frame %u offset leaves the screen", action->name,
                        (unsigned int)index);
                return -ERANGE;
            }
        }
    }
    return 0;
}

static size_t band_index_for_wpm(const struct zmk_dongle_animation_pack *pack, uint8_t wpm) {
    size_t selected = 0;
    for (size_t index = 1; index < pack->band_count; index++) {
        if (wpm < pack->bands[index].min_wpm) { break; }
        selected = index;
    }
    return selected;
}

static size_t playback_band_for_wpm(const struct zmk_dongle_animation_pack *pack, uint8_t wpm) {
    size_t selected = band_index_for_wpm(pack, wpm);
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_CHARGE_MODE)
    selected = zmk_dongle_animation_charge_wpm_band(selected);
#endif
    return selected;
}

static const struct zmk_dongle_animation_pack *current_pack(void) {
    return zmk_dongle_animation_registry.packs[current_pack_index];
}

static bool registry_uses_projectile_track(void) {
    for (size_t pack_index = 0; pack_index < zmk_dongle_animation_registry.pack_count;
         pack_index++) {
        const struct zmk_dongle_animation_pack *pack =
            zmk_dongle_animation_registry.packs[pack_index];
        if (pack == NULL || pack->actions == NULL) {
            continue;
        }
        for (size_t action_index = 0; action_index < pack->action_count; action_index++) {
            const struct zmk_dongle_animation_action *action = pack->actions[action_index];
            if (action != NULL && action->frame_roles != NULL) {
                return true;
            }
        }
    }
    return false;
}

static void apply_mode(struct zmk_widget_dongle_animation *widget,
                       const struct zmk_dongle_animation_action *action,
                       bool force_battle_mode) {
    bool fullscreen = force_battle_mode ||
                      (action->flags & ZMK_DONGLE_ANIMATION_FLAG_FULLSCREEN) != 0;
    bool battle_hud = force_battle_mode ||
                      (action->flags & ZMK_DONGLE_ANIMATION_FLAG_BATTLE_HUD) != 0;

    /* Hidden is the safe default. Only an action that explicitly owns the HUD may reveal it. */
    lv_obj_add_flag(widget->battle_hud_layer, LV_OBJ_FLAG_HIDDEN);
    if (fullscreen) {
        lv_obj_add_flag(widget->normal_layer, LV_OBJ_FLAG_HIDDEN);
        const lv_image_dsc_t *frame = action->frames[0];
        widget->origin_y = widget->screen_height - frame->header.h;
    } else {
        lv_obj_clear_flag(widget->normal_layer, LV_OBJ_FLAG_HIDDEN);
        widget->origin_y = 0;
    }
    if (battle_hud) {
        lv_obj_clear_flag(widget->battle_hud_layer, LV_OBJ_FLAG_HIDDEN);
    }

    /* Bottom to top: normal status, HUD, character ink. */
    lv_obj_move_background(widget->normal_layer);
    lv_obj_move_foreground(widget->battle_hud_layer);
    lv_obj_move_foreground(widget->obj);
    if (widget->projectile_obj != NULL) {
        lv_obj_move_foreground(widget->projectile_obj);
    }
}

static uint32_t action_frame_period(const struct zmk_widget_dongle_animation *widget,
                                    uint8_t frame_index) {
    return zmk_dongle_animation_playback_frame_period(
        widget->action->frame_durations_ms, widget->action->duration_ms,
        widget->action->frame_count, frame_index, widget->action->endpoint_hold_ms);
}

static void arm_frame_timer_until(struct zmk_widget_dongle_animation *widget,
                                  uint64_t deadline_ms) {
    uint64_t now_ms = (uint64_t)k_uptime_get();
    uint64_t remaining_ms = deadline_ms > now_ms ? deadline_ms - now_ms : 1U;
    uint32_t timer_period =
        remaining_ms > UINT32_MAX ? UINT32_MAX : (uint32_t)remaining_ms;
    widget->frame_deadline_ms = deadline_ms;
    lv_timer_set_period(widget->timer, timer_period);
    lv_timer_reset(widget->timer);
    lv_timer_resume(widget->timer);
}

static void arm_current_frame_from_now(struct zmk_widget_dongle_animation *widget) {
    arm_frame_timer_until(widget,
                          (uint64_t)k_uptime_get() +
                              action_frame_period(widget, widget->frame_index));
}

static void animation_painted_cb(lv_event_t *event) {
    struct zmk_widget_dongle_animation *widget = lv_event_get_user_data(event);
    if (!widget->waiting_for_paint) {
        return;
    }
    lv_obj_t *expected =
        widget->waiting_role == ZMK_DONGLE_ANIMATION_FRAME_ROLE_PROJECTILE
            ? widget->projectile_obj
            : widget->obj;
    if (lv_event_get_target(event) != expected) {
        return;
    }
    widget->waiting_for_paint = false;
    arm_current_frame_from_now(widget);
}

static uint8_t action_frame_role(const struct zmk_widget_dongle_animation *widget,
                                 uint8_t frame_index) {
    return widget->action->frame_roles != NULL
               ? widget->action->frame_roles[frame_index]
               : ZMK_DONGLE_ANIMATION_FRAME_ROLE_CHARACTER;
}

static void action_frame_position(const struct zmk_widget_dongle_animation *widget,
                                  uint8_t frame_index, int32_t *x, int32_t *y) {
    int32_t frame_x =
        widget->action->frame_x_offsets != NULL
            ? widget->origin_x + widget->action->frame_x_offsets[frame_index]
            : zmk_dongle_animation_frame_x_action(
                  widget->origin_x, widget->target_x, widget->action->movement_steps,
                  widget->action->frame_count, frame_index, widget->action->return_step);
    int32_t frame_y = widget->origin_y;
    if (widget->action->frame_y_offsets != NULL) {
        frame_y += widget->action->frame_y_offsets[frame_index];
    }
    *x = frame_x;
    *y = frame_y;
}

static void paint_frame_range(struct zmk_widget_dongle_animation *widget,
                              uint8_t first_frame, uint8_t last_frame,
                              bool reset_tracks) {
    struct zmk_dongle_animation_track_indices tracks = {
        .character = reset_tracks ? UINT8_MAX : widget->character_frame_index,
        .projectile = reset_tracks ? UINT8_MAX : widget->projectile_frame_index,
        .projectile_visible = reset_tracks ? false : widget->projectile_visible,
    };
    zmk_dongle_animation_resolve_tracks(widget->action->frame_roles, first_frame,
                                        last_frame, &tracks);
    if (tracks.character == UINT8_MAX) {
        LOG_ERR("Animation %s lost its character track", widget->action->name);
        lv_timer_pause(widget->timer);
        return;
    }

    const void *character_frame = widget->action->frames[tracks.character];
    int32_t character_x;
    int32_t character_y;
    action_frame_position(widget, tracks.character, &character_x, &character_y);
    const void *projectile_frame = tracks.projectile != UINT8_MAX
                                       ? widget->action->frames[tracks.projectile]
                                       : NULL;
    int32_t projectile_x = widget->projectile_x;
    int32_t projectile_y = widget->projectile_y;
    if (tracks.projectile != UINT8_MAX) {
        action_frame_position(widget, tracks.projectile, &projectile_x, &projectile_y);
    }
    bool projectile_visible = tracks.projectile_visible;

    if (character_frame != widget->character_frame) {
        lv_image_set_src(widget->obj, character_frame);
    }
    if (character_x != widget->character_x || character_y != widget->character_y) {
        lv_obj_set_pos(widget->obj, character_x, character_y);
    }

    if (widget->projectile_obj != NULL) {
        if (projectile_frame != NULL && projectile_frame != widget->projectile_frame) {
            lv_image_set_src(widget->projectile_obj, projectile_frame);
        }
        if (projectile_x != widget->projectile_x || projectile_y != widget->projectile_y) {
            lv_obj_set_pos(widget->projectile_obj, projectile_x, projectile_y);
        }
        if (projectile_visible) {
            lv_obj_clear_flag(widget->projectile_obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(widget->projectile_obj, LV_OBJ_FLAG_HIDDEN);
        }
    }

    widget->character_frame = character_frame;
    widget->projectile_frame = projectile_frame;
    widget->character_x = character_x;
    widget->character_y = character_y;
    widget->projectile_x = projectile_x;
    widget->projectile_y = projectile_y;
    widget->projectile_visible = projectile_visible;
    widget->character_frame_index = tracks.character;
    widget->projectile_frame_index = tracks.projectile;
    widget->frame_index = last_frame;
    widget->waiting_role = action_frame_role(widget, last_frame);
    widget->waiting_for_paint = zmk_dongle_animation_waits_for_paint(
        widget->action->frame_durations_ms != NULL, widget->action->endpoint_hold_ms,
        widget->action->frame_count, last_frame);
    if (widget->waiting_for_paint) {
        lv_timer_pause(widget->timer);
        lv_obj_t *expected =
            widget->waiting_role == ZMK_DONGLE_ANIMATION_FRAME_ROLE_PROJECTILE
                ? widget->projectile_obj
                : widget->obj;
        lv_obj_invalidate(expected);
    }
}

static void show_frame_from_now(struct zmk_widget_dongle_animation *widget,
                                uint8_t frame_index) {
    paint_frame_range(widget, 0, frame_index, true);
    if (!widget->waiting_for_paint) {
        arm_current_frame_from_now(widget);
    }
}

static void show_frame_until(struct zmk_widget_dongle_animation *widget,
                             uint8_t first_frame, uint8_t last_frame,
                             uint64_t deadline_ms) {
    paint_frame_range(widget, first_frame, last_frame, false);
    if (!widget->waiting_for_paint) {
        arm_frame_timer_until(widget, deadline_ms);
    }
}

static int start_animation_with_mode(struct zmk_widget_dongle_animation *widget,
                                     size_t band_index, bool force_battle_mode) {
    const struct zmk_dongle_animation_pack *pack = current_pack();
    int err = validate_pack(pack);
    if (err < 0) { return err; }
    if (band_index >= pack->band_count) {
        LOG_ERR("Animation pack %s has no requested band %u", pack->name,
                (unsigned int)band_index);
        return -ERANGE;
    }
    const struct zmk_dongle_animation_band *band = &pack->bands[band_index];
    const struct zmk_dongle_animation_action *action = pack->actions[band->action_index];
    err = validate_action(widget, action);
    if (err < 0) { return err; }
    widget->action = action;
    widget->current_band_index = band_index;
    const lv_image_dsc_t *first_frame = action->frames[0];
    widget->origin_x = zmk_dongle_animation_origin_x(widget->screen_width, first_frame->header.w);
    widget->target_x = zmk_dongle_animation_target_x(
        widget->screen_width, first_frame->header.w, action->motion);
    lv_obj_set_size(widget->obj, first_frame->header.w, first_frame->header.h);
    if (widget->projectile_obj != NULL) {
        lv_obj_set_size(widget->projectile_obj, first_frame->header.w, first_frame->header.h);
    }
    apply_mode(widget, action, force_battle_mode);
    show_frame_from_now(widget, 0);
    LOG_INF("Animation %s/%s: %u frames/%ums, x=%d..%d, motion=%u flags=0x%02x", pack->name,
            action->name, action->frame_count, action->duration_ms, widget->origin_x,
            widget->target_x, action->motion, action->flags);
    return 0;
}

static int start_animation(struct zmk_widget_dongle_animation *widget, size_t band_index) {
    return start_animation_with_mode(widget, band_index, false);
}

static void restart_animation(struct zmk_widget_dongle_animation *widget) {
    show_frame_from_now(widget, 0);
}

#if !IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_DEMO_MODE)
static bool consume_next_request(void) {
    atomic_val_t pending;
    do { pending = atomic_get(&next_requests); }
    while (pending > 0 && !atomic_cas(&next_requests, pending, pending - 1));
    return pending > 0;
}
#endif

static void animation_timer_cb(lv_timer_t *timer) {
    struct zmk_widget_dongle_animation *widget = lv_timer_get_user_data(timer);
    uint64_t now_ms = (uint64_t)k_uptime_get();
    if (now_ms < widget->frame_deadline_ms) {
        arm_frame_timer_until(widget, widget->frame_deadline_ms);
        return;
    }
    if (widget->frame_index + 1 < widget->action->frame_count) {
        uint64_t next_deadline_ms = widget->frame_deadline_ms;
        uint8_t next_frame = zmk_dongle_animation_coalesced_frame(
            widget->action->frame_durations_ms, widget->action->duration_ms,
            widget->action->frame_count, widget->frame_index,
            widget->action->endpoint_hold_ms, now_ms, &next_deadline_ms);
        show_frame_until(widget, widget->frame_index + 1U, next_frame, next_deadline_ms);
        return;
    }
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_DEMO_MODE)
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_CHARGE_MODE)
    size_t completed_band = widget->current_band_index;
    size_t expected_band = zmk_dongle_animation_charge_demo_band(widget->demo_phase);
    if (completed_band != expected_band) {
        LOG_ERR("Charged demo phase %u expected band %u, completed %u", widget->demo_phase,
                (unsigned int)expected_band, (unsigned int)completed_band);
        lv_timer_pause(widget->timer);
        return;
    }

    uint8_t gain = zmk_dongle_animation_charge_gain(completed_band, true);
    widget->charge_level = zmk_dongle_animation_charge_add(widget->charge_level, gain);
    if (gain > 0) {
        zmk_widget_fighter_charge_set(widget->charge_level);
        LOG_INF("Fighter demo charge +%u => %u", gain, widget->charge_level);
    }

    uint8_t next_phase = widget->demo_phase + 1;
    bool pack_changed = false;
    if (next_phase >= ZMK_DONGLE_ANIMATION_CHARGE_DEMO_PHASE_COUNT) {
        size_t next_pack_index = zmk_dongle_animation_next_index(
            current_pack_index, zmk_dongle_animation_registry.pack_count);
        pack_changed = next_pack_index != current_pack_index;
        current_pack_index = next_pack_index;
        next_phase = 0;
    }

    size_t next_band = zmk_dongle_animation_charge_demo_band(next_phase);
    if (next_band == ZMK_DONGLE_ANIMATION_FAST_BAND &&
        !zmk_dongle_animation_charge_ready(widget->charge_level)) {
        LOG_ERR("Charged demo cannot enter fast with charge %u", widget->charge_level);
        lv_timer_pause(widget->timer);
        return;
    }
    bool full_charge_idle = next_phase == 5;
    int err = 0;
    if (zmk_dongle_animation_should_restart_current(
            pack_changed, full_charge_idle, widget->current_band_index, next_band)) {
        restart_animation(widget);
    } else {
        err = start_animation_with_mode(widget, next_band, full_charge_idle);
    }
    if (err < 0) {
        LOG_ERR("Failed to advance charged animation demo: %d", err);
        lv_timer_pause(widget->timer);
        return;
    }
    widget->demo_phase = next_phase;
    if (next_band == ZMK_DONGLE_ANIMATION_FAST_BAND) {
        widget->charge_level = 0;
        zmk_widget_fighter_charge_set(0);
        LOG_INF("Fighter demo charge spent; fast animation started");
    }
#else
    lv_timer_pause(widget->timer);
#endif
    return;
#else
    size_t completed_band = widget->current_band_index;
    bool pack_changed = false;
    if (consume_next_request()) {
        size_t next_pack_index = zmk_dongle_animation_next_index(
            current_pack_index, zmk_dongle_animation_registry.pack_count);
        pack_changed = next_pack_index != current_pack_index;
        current_pack_index = next_pack_index;
    }
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_CHARGE_MODE)
    uint8_t gain = zmk_dongle_animation_charge_gain(completed_band, false);
    widget->charge_level =
        zmk_dongle_animation_charge_add(widget->charge_level, gain);
    if (gain > 0) {
        zmk_widget_fighter_charge_set(widget->charge_level);
        LOG_INF("Fighter charge +%u => %u", gain, widget->charge_level);
    }
    if (zmk_dongle_animation_charge_ready(widget->charge_level)) {
        int err = start_animation(widget, ZMK_DONGLE_ANIMATION_FAST_BAND);
        if (err < 0) {
            LOG_ERR("Failed to start charged fast animation: %d", err);
            lv_timer_pause(widget->timer);
            return;
        }
        widget->charge_level = 0;
        zmk_widget_fighter_charge_set(0);
        LOG_INF("Fighter charge spent; fast animation started");
        return;
    }
#endif
    size_t next_band = playback_band_for_wpm(current_pack(), latest_wpm);
    int err = 0;
    if (zmk_dongle_animation_should_restart_current(
            pack_changed, false, widget->current_band_index, next_band)) {
        restart_animation(widget);
    } else {
        err = start_animation(widget, next_band);
    }
    if (err < 0) {
        LOG_ERR("Failed to advance dongle animation: %d", err);
        lv_timer_pause(widget->timer);
    }
#endif
}

void zmk_widget_dongle_animation_request_next(void) {
#if !IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_DEMO_MODE)
    atomic_inc(&next_requests);
#endif
}

static void set_animation(struct zmk_widget_dongle_animation *widget,
                          struct dongle_animation_wpm_state state) {
    latest_wpm = state.wpm;
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_DEMO_MODE)
    return;
#endif
    size_t target_band = playback_band_for_wpm(current_pack(), latest_wpm);
    if (widget->action == NULL || target_band > widget->current_band_index) {
        int err = start_animation(widget, target_band);
        if (err < 0) {
            LOG_ERR("Failed to select WPM animation: %d", err);
            lv_timer_pause(widget->timer);
        }
    }
}

static struct dongle_animation_wpm_state get_wpm_state(const zmk_event_t *eh) {
    struct zmk_wpm_state_changed *event = as_zmk_wpm_state_changed(eh);
    return (struct dongle_animation_wpm_state){.wpm = event->state};
}
static void update_wpm_cb(struct dongle_animation_wpm_state state) {
    struct zmk_widget_dongle_animation *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_animation(widget, state); }
}
ZMK_DISPLAY_WIDGET_LISTENER(widget_dongle_animation, struct dongle_animation_wpm_state,
                            update_wpm_cb, get_wpm_state)
ZMK_SUBSCRIPTION(widget_dongle_animation, zmk_wpm_state_changed);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_DEMO_MODE) &&                         \
    !IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_CHARGE_MODE)
static void animation_demo_timer_cb(lv_timer_t *timer) {
    struct zmk_widget_dongle_animation *widget = lv_timer_get_user_data(timer);

    if (widget->demo_phase >= 3) {
        current_pack_index = zmk_dongle_animation_next_index(
            current_pack_index, zmk_dongle_animation_registry.pack_count);
        widget->demo_phase = 0;
    }

    const struct zmk_dongle_animation_pack *pack = current_pack();
    if (pack->band_count < 4) {
        LOG_ERR("Animation demo requires four WPM bands; pack %s has %u", pack->name,
                (unsigned int)pack->band_count);
        lv_timer_pause(timer);
        return;
    }

    size_t band_index = widget->demo_phase + 1;
    widget->demo_phase++;
    int err = start_animation(widget, band_index);
    if (err < 0) {
        LOG_ERR("Failed to advance animation demo: %d", err);
        lv_timer_pause(timer);
        return;
    }
    uint32_t demo_period = widget->action->duration_ms;
    lv_timer_set_period(timer, demo_period);
    LOG_INF("Animation demo %s: phase %u/3 for %u ms", pack->name, widget->demo_phase,
            demo_period);
}
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_ROTATE_ON_WAKE) &&                         \
    !IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_DEMO_MODE)
static int animation_activity_event_handler(const zmk_event_t *eh) {
    struct zmk_activity_state_changed *event = as_zmk_activity_state_changed(eh);
    if (event == NULL) { return -ENOTSUP; }
    if (event->state == ZMK_ACTIVITY_ACTIVE) {
        if (activity_was_inactive) { zmk_widget_dongle_animation_request_next(); }
        activity_was_inactive = false;
    } else {
        activity_was_inactive = true;
    }
    return 0;
}
ZMK_LISTENER(dongle_animation_activity, animation_activity_event_handler);
ZMK_SUBSCRIPTION(dongle_animation_activity, zmk_activity_state_changed);
#endif

int zmk_widget_dongle_animation_init(struct zmk_widget_dongle_animation *widget,
                                     lv_obj_t *parent, lv_obj_t *normal_layer,
                                     lv_obj_t *battle_hud_layer) {
    if (!registry_initialized) {
        int err = validate_registry();
        if (err < 0) { return err; }
        boot_nonce += UINT32_C(0x9e3779b9);
        uint32_t start_seed =
            zmk_dongle_animation_mix_start_seed(k_cycle_get_32(), boot_nonce);
        current_pack_index = zmk_dongle_animation_start_index(
            start_seed, zmk_dongle_animation_registry.pack_count);
        registry_initialized = true;
    }
    widget->normal_layer = normal_layer;
    widget->battle_hud_layer = battle_hud_layer;
    widget->screen_width = lv_display_get_horizontal_resolution(lv_obj_get_display(parent));
    widget->screen_height = lv_display_get_vertical_resolution(lv_obj_get_display(parent));
    widget->origin_x = zmk_dongle_animation_origin_x(
        widget->screen_width, zmk_dongle_animation_registry.canvas_width);
    widget->current_band_index = SIZE_MAX;
    widget->action = NULL;
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_CHARGE_MODE)
    widget->charge_level = 0;
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_DEMO_MODE)
    widget->demo_phase = 0;
#endif
    widget->waiting_for_paint = false;
    widget->projectile_visible = false;
    widget->character_frame_index = UINT8_MAX;
    widget->projectile_frame_index = UINT8_MAX;
    widget->character_frame = NULL;
    widget->projectile_frame = NULL;
    widget->character_x = INT32_MIN;
    widget->character_y = INT32_MIN;
    widget->projectile_x = INT32_MIN;
    widget->projectile_y = INT32_MIN;
    widget->obj = lv_image_create(parent);
    if (widget->obj == NULL) {
        LOG_ERR("Failed to allocate animation image object");
        return -ENOMEM;
    }
    lv_obj_set_size(widget->obj, zmk_dongle_animation_registry.canvas_width,
                    zmk_dongle_animation_registry.canvas_height);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget->obj, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(widget->obj, animation_painted_cb, LV_EVENT_DRAW_POST, widget);
    widget->projectile_obj = NULL;
    if (registry_uses_projectile_track()) {
        widget->projectile_obj = lv_image_create(parent);
        if (widget->projectile_obj == NULL) {
            LOG_ERR("Failed to allocate projectile image object");
            lv_obj_delete(widget->obj);
            widget->obj = NULL;
            return -ENOMEM;
        }
        lv_obj_set_size(widget->projectile_obj, zmk_dongle_animation_registry.canvas_width,
                        zmk_dongle_animation_registry.canvas_height);
        lv_obj_set_style_bg_opa(widget->projectile_obj, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_pad_all(widget->projectile_obj, 0, LV_PART_MAIN);
        lv_obj_add_flag(widget->projectile_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(widget->projectile_obj, animation_painted_cb,
                            LV_EVENT_DRAW_POST, widget);
    }
    widget->timer = lv_timer_create(animation_timer_cb, 1, widget);
    if (widget->timer == NULL) {
        LOG_ERR("Failed to allocate animation frame timer");
        if (widget->projectile_obj != NULL) {
            lv_obj_delete(widget->projectile_obj);
            widget->projectile_obj = NULL;
        }
        lv_obj_delete(widget->obj);
        widget->obj = NULL;
        return -ENOMEM;
    }
    lv_timer_pause(widget->timer);
    size_t initial_band = playback_band_for_wpm(current_pack(), latest_wpm);
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_DEMO_MODE) &&                         \
    IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_CHARGE_MODE)
    initial_band = zmk_dongle_animation_charge_demo_band(widget->demo_phase);
#endif
    int err = start_animation(widget, initial_band);
    if (err < 0) {
        lv_timer_delete(widget->timer);
        if (widget->projectile_obj != NULL) {
            lv_obj_delete(widget->projectile_obj);
            widget->projectile_obj = NULL;
        }
        lv_obj_delete(widget->obj);
        widget->timer = NULL;
        widget->obj = NULL;
        return err;
    }
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_DEMO_MODE) &&                         \
    !IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_CHARGE_MODE)
    widget->demo_timer = lv_timer_create(animation_demo_timer_cb, 1, widget);
    if (widget->demo_timer == NULL) {
        LOG_ERR("Failed to allocate animation demo timer");
        lv_timer_delete(widget->timer);
        if (widget->projectile_obj != NULL) {
            lv_obj_delete(widget->projectile_obj);
            widget->projectile_obj = NULL;
        }
        lv_obj_delete(widget->obj);
        widget->timer = NULL;
        widget->obj = NULL;
        return -ENOMEM;
    }
#endif
    sys_slist_append(&widgets, &widget->node);
    widget_dongle_animation_init();
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_DEMO_MODE) &&                         \
    !IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_CHARGE_MODE)
    animation_demo_timer_cb(widget->demo_timer);
#endif
    return 0;
}

lv_obj_t *zmk_widget_dongle_animation_obj(struct zmk_widget_dongle_animation *widget) {
    return widget->obj;
}

lv_obj_t *zmk_widget_dongle_animation_projectile_obj(
    struct zmk_widget_dongle_animation *widget) {
    return widget->projectile_obj;
}
