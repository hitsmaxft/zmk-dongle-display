# Dongle Display

This module repository provides a ZMK shield that replaces the built-in status screen with a custom screen designed for 128x64-pixel OLED displays.

## Usage

To use this module, first add it to your `config/west.yml` by adding a new entry to `remotes` and `projects`:

```yaml west.yml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: englmaxi
      url-base: https://github.com/englmaxi
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    - name: zmk-dongle-display
      remote: englmaxi
      revision: main
  self:
    path: config
```

Next, replace the built-in status screen by adding `dongle_display` to your `build.yaml`:

```yaml build.yaml
---
include:
  - board: seeeduino_xiao_ble
    shield: sweep_central_dongle dongle_display
```

This shield assumes that the [dongle](https://zmk.dev/docs/development/hardware-integration/dongle) is already set up and functioning with the built-in status screen.
For setup examples, refer to the shields in my [`zmk-config`](https://github.com/englmaxi/zmk-config/tree/master/boards/shields).
- If you are using the larger 1.3" OLED, replace `solomon,ssd1306fb` with `sinowealth,sh1106` and set `segment-offset = <2>`.
- If you are using a nice!nano, replace `xiao_i2c` with `pro_micro_i2c`.

## Widgets
- active hid indicators (CLCK, NLCK, SLCK)
- active modifiers
- bongo cat
- highest layer name
- output status
- peripheral battery levels

## Configuration

To also display the battery level of the dongle/central device, use the following configuration property:

```ini
CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY=y
```

If you want to use MacOS modifier symbols instead of the Windows modifier symbols, use the following configuration property:

```ini
CONFIG_ZMK_DONGLE_DISPLAY_MAC_MODIFIERS=y
```

### Custom animation provider

With `CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_EXTENSION=y`, the built-in registry contains Bongo Cat
and the optional Fighter pack. `animation-next` rotates between them; the legacy `fighter-next`
binding remains an alias. Fighter idle/slow retains the normal status screen, while mid/fast uses
frame-synchronous leftward motion and a two-peripheral battle battery HUD. Configure its mapping
with `CONFIG_ZMK_DONGLE_DISPLAY_BATTLE_BATTERY_LEFT_SOURCE` and
`CONFIG_ZMK_DONGLE_DISPLAY_BATTLE_BATTERY_RIGHT_SOURCE`.

To compile animation data from your ZMK config instead, add:

```ini
CONFIG_ZMK_DONGLE_DISPLAY_CUSTOM_ANIMATION_PROVIDER=y
CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_PROVIDER_HEADER="animations/my_provider.h"
CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_ROTATE_ON_WAKE=y
```

For a build-generated Provider, also configure
`CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_PROVIDER_GENERATED`, the generator path, a persistent cache
directory, and use the cached filename as `ANIMATION_PROVIDER_HEADER`. CMake invokes the generator
before compiling the Provider; input hashing and cache invalidation belong to that generator.

The custom-provider option selects `CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_EXTENSION` automatically.
When neither option is enabled, CMake compiles the original Bongo Cat widget directly; the generic
engine, provider registry, validation, random selection, and NEXT behavior have no linked symbols.

The header path is relative to `ZMK_CONFIG`. Include
`<zmk/dongle_display/animation.h>`, define actions with
`ZMK_DONGLE_ANIMATION_ACTION_DEFINE`, define packs with
`ZMK_DONGLE_ANIMATION_PACK_WPM4_DEFINE`, and finish with
`ZMK_DONGLE_ANIMATION_REGISTRY_DEFINE`. Static frame arrays derive their frame count automatically;
every action must contain 1 to 127 LVGL image descriptors. All packs share the registry canvas size.
The Provider ABI is version 2. Existing action macros remain source-compatible and default to no
motion; moving layouts use `ZMK_DONGLE_ANIMATION_ACTION_LAYOUT_DEFINE`. A custom Provider still
owns the complete registry, so the built-in Fighter pack is not linked into custom builds.

`config/animations/example_provider.h` in the consuming ZMK config is a complete one-frame example.
No CMake file or module-source change is needed.

To request the next pack from a keymap, define the zero-parameter
`zmk,behavior-dongle-animation-next` behavior and bind it as `&animation_next`. The current action
finishes before the request is applied. Startup selects a random pack; NEXT then advances in stable
Provider order.

## Demo
![output](https://github.com/englmaxi/zmk-config/assets/43675074/8d268f23-1a4f-44c3-817e-c36dc96a1f8b)
![mods](https://github.com/englmaxi/zmk-config/assets/43675074/af9ec3f5-8f61-4629-abed-14ba0047f0bd)

## Dongle Designs
- [case1](/cases)
- [case2](/cases)
- [Cyberdeck](https://github.com/rafaelromao/keyboards/tree/main/stls/Dongle) by @rafaelromao
- [Dongle PCB](https://github.com/spe2/zmk_dongle_hardware) by @spe2
- [Macintosh](https://makerworld.com/en/models/403660) by @rain2813
- [Redox](https://makerworld.com/en/models/242951) by @rurounikexin
- [sai44 Dongle](https://github.com/leafflat/sai44/tree/main/STL/Dongle) by @leafflat
- [ZMK Display Dongle](https://makerworld.com/en/models/496738) by @yingeling
- [ZMK Nice Nano 128x64 OLED Dongle](https://www.printables.com/model/1207682-zmk-nice-nano-128x64-oled-dongle) by @James_909973


## Alternatives
- [Prospector](https://github.com/carrefinho/prospector) by @carrefinho
- [YADS](https://github.com/janpfischer/zmk-dongle-screen) by @janpfischer
