/* SPDX-License-Identifier: MIT */

ASSERT(_flash_used <= CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_ROM_MAX_BYTES,
       "Dongle display total linked ROM exceeds the configured startup-safety budget; a prior Cornix build above this empirical budget hit the Bluetooth controller prepare-latency assertion; set CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_ROM_MAX_BYTES=0 to disable this optional guard")
