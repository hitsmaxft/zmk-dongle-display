/*
 * Copyright (c) 2026 The ZMK Contributors
 *
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

struct dongle_animation_wpm_state {
    uint8_t wpm;
};

static size_t current_pack_index;
static size_t current_band_index = SIZE_MAX;
static uint8_t latest_wpm;
static atomic_t next_requests;
static bool registry_initialized;

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_ROTATE_ON_WAKE)
static bool activity_was_inactive;
#endif

static int validate_registry(void) {
    const struct zmk_dongle_animation_registry *registry = &zmk_dongle_animation_registry;

    if (registry->abi_version != ZMK_DONGLE_ANIMATION_PROVIDER_ABI_VERSION) {
        LOG_ERR("Animation provider ABI %u is unsupported", registry->abi_version);
        return -EPROTONOSUPPORT;
    }
    if (registry->canvas_width == 0 || registry->canvas_height == 0 || registry->packs == NULL ||
        registry->pack_count == 0) {
        LOG_ERR("Animation registry has an invalid canvas or pack table");
        return -EINVAL;
    }

    for (size_t pack_index = 0; pack_index < registry->pack_count; pack_index++) {
        const struct zmk_dongle_animation_pack *pack = registry->packs[pack_index];

        if (pack == NULL || pack->name == NULL || pack->actions == NULL ||
            pack->action_count == 0 || pack->bands == NULL || pack->band_count == 0) {
            LOG_ERR("Animation pack %u is invalid", (unsigned int)pack_index);
            return -EINVAL;
        }

        uint8_t previous_min_wpm = 0;
        for (size_t band_index = 0; band_index < pack->band_count; band_index++) {
            const struct zmk_dongle_animation_band *band = &pack->bands[band_index];
            if ((band_index == 0 && band->min_wpm != 0) ||
                (band_index > 0 && band->min_wpm <= previous_min_wpm) ||
                band->action_index >= pack->action_count) {
                LOG_ERR("Animation pack %s has an invalid WPM band", pack->name);
                return -EINVAL;
            }
            previous_min_wpm = band->min_wpm;
        }

        for (size_t action_index = 0; action_index < pack->action_count; action_index++) {
            const struct zmk_dongle_animation_action *action = pack->actions[action_index];
            if (action == NULL || action->name == NULL || action->frames == NULL ||
                action->frame_count == 0 ||
                action->frame_count > ZMK_DONGLE_ANIMATION_MAX_FRAMES ||
                action->duration_ms == 0) {
                LOG_ERR("Animation pack %s action %u is invalid", pack->name,
                        (unsigned int)action_index);
                return -EINVAL;
            }
            for (size_t frame_index = 0; frame_index < action->frame_count; frame_index++) {
                if (action->frames[frame_index] == NULL) {
                    LOG_ERR("Animation pack %s action %s has a null frame", pack->name,
                            action->name);
                    return -EINVAL;
                }
            }
        }
    }

    return 0;
}

static size_t band_index_for_wpm(const struct zmk_dongle_animation_pack *pack, uint8_t wpm) {
    size_t selected = 0;

    for (size_t index = 1; index < pack->band_count; index++) {
        if (wpm < pack->bands[index].min_wpm) {
            break;
        }
        selected = index;
    }

    return selected;
}

static const struct zmk_dongle_animation_pack *current_pack(void) {
    return zmk_dongle_animation_registry.packs[current_pack_index];
}

static void start_animation(lv_obj_t *animimg, size_t band_index) {
    const struct zmk_dongle_animation_pack *pack = current_pack();
    const struct zmk_dongle_animation_band *band = &pack->bands[band_index];
    const struct zmk_dongle_animation_action *action = pack->actions[band->action_index];

    lv_animimg_set_src(animimg, (const void **)action->frames, action->frame_count);
    lv_animimg_set_duration(animimg, action->duration_ms);
    lv_animimg_set_repeat_count(animimg, 0);
    current_band_index = band_index;
    lv_animimg_start(animimg);
}

static bool consume_next_request(void) {
    atomic_val_t pending;

    do {
        pending = atomic_get(&next_requests);
    } while (pending > 0 && !atomic_cas(&next_requests, pending, pending - 1));

    return pending > 0;
}

static void select_next_pack(void) {
    size_t pack_count = zmk_dongle_animation_registry.pack_count;
    current_pack_index = zmk_dongle_animation_next_index(current_pack_index, pack_count);
}

static void animation_completed_cb(lv_anim_t *animation) {
    lv_obj_t *animimg = lv_anim_get_user_data(animation);

    if (consume_next_request()) {
        select_next_pack();
    }

    start_animation(animimg, band_index_for_wpm(current_pack(), latest_wpm));
}

void zmk_widget_dongle_animation_request_next(void) { atomic_inc(&next_requests); }

static void set_animation(lv_obj_t *animimg, struct dongle_animation_wpm_state state) {
    size_t target_band;

    latest_wpm = state.wpm;
    target_band = band_index_for_wpm(current_pack(), latest_wpm);

    if (current_band_index == SIZE_MAX || target_band > current_band_index) {
        lv_anim_delete(animimg, NULL);
        start_animation(animimg, target_band);
    }
}

static struct dongle_animation_wpm_state get_wpm_state(const zmk_event_t *eh) {
    struct zmk_wpm_state_changed *event = as_zmk_wpm_state_changed(eh);
    return (struct dongle_animation_wpm_state){.wpm = event->state};
}

static void update_wpm_cb(struct dongle_animation_wpm_state state) {
    struct zmk_widget_dongle_animation *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_animation(widget->obj, state); }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_dongle_animation, struct dongle_animation_wpm_state,
                            update_wpm_cb, get_wpm_state)
ZMK_SUBSCRIPTION(widget_dongle_animation, zmk_wpm_state_changed);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_ROTATE_ON_WAKE)
static int animation_activity_event_handler(const zmk_event_t *eh) {
    struct zmk_activity_state_changed *event = as_zmk_activity_state_changed(eh);
    if (event == NULL) {
        return -ENOTSUP;
    }

    if (event->state == ZMK_ACTIVITY_ACTIVE) {
        if (activity_was_inactive) {
            zmk_widget_dongle_animation_request_next();
        }
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
                                     lv_obj_t *parent) {
    if (!registry_initialized) {
        int err = validate_registry();
        if (err < 0) {
            return err;
        }
        current_pack_index = zmk_dongle_animation_start_index(
            k_cycle_get_32(), zmk_dongle_animation_registry.pack_count);
        registry_initialized = true;
    }

    widget->obj = lv_animimg_create(parent);
    lv_obj_set_size(widget->obj, zmk_dongle_animation_registry.canvas_width,
                    zmk_dongle_animation_registry.canvas_height);
    lv_obj_center(widget->obj);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget->obj, 0, LV_PART_MAIN);

    lv_anim_t *animation = lv_animimg_get_anim(widget->obj);
    lv_anim_set_user_data(animation, widget->obj);
    lv_anim_set_completed_cb(animation, animation_completed_cb);

    sys_slist_append(&widgets, &widget->node);
    widget_dongle_animation_init();

    return 0;
}

lv_obj_t *zmk_widget_dongle_animation_obj(struct zmk_widget_dongle_animation *widget) {
    return widget->obj;
}
