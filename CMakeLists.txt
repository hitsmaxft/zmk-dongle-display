zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
target_sources(app PRIVATE
    boards/shields/dongle_display/widgets/behavior_animation_next.c
    boards/shields/dongle_display/widgets/behavior_fighter_next.c
)
