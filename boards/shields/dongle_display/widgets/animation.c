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

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
struct dongle_animation_wpm_state { uint8_t wpm; };
static size_t current_pack_index;
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
    uint32_t endpoint_duration = 2U * action->endpoint_hold_ms;
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
    if (action->motion != ZMK_DONGLE_ANIMATION_MOTION_NONE &&
        (action->frame_count < 2 || distance < action->frame_count - 1)) {
        LOG_ERR("Animation action %s cannot move %u frames through %dpx", action->name,
                action->frame_count, distance);
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

static const struct zmk_dongle_animation_pack *current_pack(void) {
    return zmk_dongle_animation_registry.packs[current_pack_index];
}

static void apply_mode(struct zmk_widget_dongle_animation *widget,
                       const struct zmk_dongle_animation_action *action) {
    bool fullscreen = (action->flags & ZMK_DONGLE_ANIMATION_FLAG_FULLSCREEN) != 0;
    bool battle_hud = (action->flags & ZMK_DONGLE_ANIMATION_FLAG_BATTLE_HUD) != 0;

    /* Hidden is the safe default. Only an action that explicitly owns the HUD may reveal it. */
    lv_obj_add_flag(widget->battle_hud_layer, LV_OBJ_FLAG_HIDDEN);
    if (fullscreen) {
        lv_obj_add_flag(widget->normal_layer, LV_OBJ_FLAG_HIDDEN);
        const lv_image_dsc_t *frame = action->frames[0];
        widget->origin_y = widget->screen_height - frame->header.h;
    } else {
        lv_obj_clear_flag(widget->normal_layer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(widget->normal_layer);
        widget->origin_y = 0;
    }
    if (battle_hud) {
        lv_obj_clear_flag(widget->battle_hud_layer, LV_OBJ_FLAG_HIDDEN);
    }
}

static void animation_painted_cb(lv_event_t *event) {
    struct zmk_widget_dongle_animation *widget = lv_event_get_user_data(event);
    if (!widget->waiting_for_paint) {
        return;
    }
    widget->waiting_for_paint = false;
    lv_timer_reset(widget->timer);
    lv_timer_resume(widget->timer);
}

static void show_frame(struct zmk_widget_dongle_animation *widget, uint8_t frame_index) {
    int32_t x = zmk_dongle_animation_frame_x(widget->origin_x, widget->target_x,
                                              widget->action->frame_count, frame_index);
    lv_image_set_src(widget->obj, widget->action->frames[frame_index]);
    lv_obj_set_pos(widget->obj, x, widget->origin_y);
    widget->frame_index = frame_index;
    uint32_t period = zmk_dongle_animation_frame_period(
        widget->action->duration_ms, widget->action->frame_count, frame_index,
        widget->action->endpoint_hold_ms);
    lv_timer_set_period(widget->timer, period);
    widget->waiting_for_paint = widget->action->endpoint_hold_ms > 0 &&
                                (frame_index == 0 ||
                                 frame_index == widget->action->frame_count - 1);
    if (widget->waiting_for_paint) {
        lv_timer_pause(widget->timer);
    } else {
        lv_timer_reset(widget->timer);
        lv_timer_resume(widget->timer);
    }
}

static int start_animation(struct zmk_widget_dongle_animation *widget, size_t band_index) {
    const struct zmk_dongle_animation_pack *pack = current_pack();
    int err = validate_pack(pack);
    if (err < 0) { return err; }
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
    apply_mode(widget, action);
    show_frame(widget, 0);
    LOG_INF("Animation %s/%s: %u frames/%ums, x=%d..%d, motion=%u flags=0x%02x", pack->name,
            action->name, action->frame_count, action->duration_ms, widget->origin_x,
            widget->target_x, action->motion, action->flags);
    return 0;
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
    if (widget->frame_index + 1 < widget->action->frame_count) {
        show_frame(widget, widget->frame_index + 1);
        return;
    }
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_DEMO_MODE)
    lv_timer_pause(widget->timer);
    return;
#else
    if (consume_next_request()) {
        current_pack_index = zmk_dongle_animation_next_index(
            current_pack_index, zmk_dongle_animation_registry.pack_count);
    }
    int err = start_animation(widget, band_index_for_wpm(current_pack(), latest_wpm));
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
    size_t target_band = band_index_for_wpm(current_pack(), latest_wpm);
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

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_DEMO_MODE)
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
        current_pack_index = zmk_dongle_animation_start_index(
            k_cycle_get_32(), zmk_dongle_animation_registry.pack_count);
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
    widget->waiting_for_paint = false;
    widget->obj = lv_image_create(parent);
    if (widget->obj == NULL) {
        LOG_ERR("Failed to allocate animation image object");
        return -ENOMEM;
    }
    lv_obj_move_background(widget->obj);
    lv_obj_set_size(widget->obj, zmk_dongle_animation_registry.canvas_width,
                    zmk_dongle_animation_registry.canvas_height);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget->obj, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(widget->obj, animation_painted_cb, LV_EVENT_DRAW_POST, widget);
    widget->timer = lv_timer_create(animation_timer_cb, 1, widget);
    if (widget->timer == NULL) {
        LOG_ERR("Failed to allocate animation frame timer");
        lv_obj_delete(widget->obj);
        widget->obj = NULL;
        return -ENOMEM;
    }
    lv_timer_pause(widget->timer);
    int err = start_animation(widget, band_index_for_wpm(current_pack(), latest_wpm));
    if (err < 0) {
        lv_timer_delete(widget->timer);
        lv_obj_delete(widget->obj);
        widget->timer = NULL;
        widget->obj = NULL;
        return err;
    }
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_DEMO_MODE)
    widget->demo_phase = 0;
    widget->demo_timer = lv_timer_create(animation_demo_timer_cb, 1, widget);
    if (widget->demo_timer == NULL) {
        LOG_ERR("Failed to allocate animation demo timer");
        lv_timer_delete(widget->timer);
        lv_obj_delete(widget->obj);
        widget->timer = NULL;
        widget->obj = NULL;
        return -ENOMEM;
    }
#endif
    sys_slist_append(&widgets, &widget->node);
    widget_dongle_animation_init();
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_DEMO_MODE)
    animation_demo_timer_cb(widget->demo_timer);
#endif
    return 0;
}

lv_obj_t *zmk_widget_dongle_animation_obj(struct zmk_widget_dongle_animation *widget) {
    return widget->obj;
}
