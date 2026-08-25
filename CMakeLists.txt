zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

if(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_ROM_MAX_BYTES GREATER 0)
    zephyr_linker_sources(SECTIONS linker/animation_rom_budget.ld)
endif()

target_sources(app PRIVATE
    boards/shields/dongle_display/widgets/behavior_animation_next.c
    boards/shields/dongle_display/widgets/behavior_fighter_next.c
)
