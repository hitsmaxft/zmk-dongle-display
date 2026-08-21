if(CONFIG_ZMK_DISPLAY AND CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM AND ((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL))
    zephyr_library()
    zephyr_library_sources(${ZEPHYR_BASE}/misc/empty_file.c)
    zephyr_library_include_directories(${ZEPHYR_LVGL_MODULE_DIR})
    zephyr_library_include_directories(${ZEPHYR_BASE}/lib/gui/lvgl/)
    zephyr_library_include_directories(${ZEPHYR_BASE}/drivers)
    zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
    zephyr_library_sources(custom_status_screen.c)
    zephyr_library_sources(widgets/battery_status.c)
    if(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_EXTENSION)
        zephyr_library_sources(widgets/animation.c)
        zephyr_library_sources(widgets/animation_provider.c)
        if(CONFIG_ZMK_DONGLE_DISPLAY_CUSTOM_ANIMATION_PROVIDER)
            if("${CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_PROVIDER_HEADER}" STREQUAL "")
                message(FATAL_ERROR "Custom dongle animation provider is enabled, but no header is configured")
            endif()
            if(CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_PROVIDER_GENERATED)
                if("${CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_PROVIDER_GENERATOR}" STREQUAL "")
                    message(FATAL_ERROR "Generated dongle animation provider has no generator")
                endif()
                if(IS_ABSOLUTE "${CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_PROVIDER_GENERATOR}" OR
                   IS_ABSOLUTE "${CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_PROVIDER_CACHE_DIR}" OR
                   IS_ABSOLUTE "${CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_PROVIDER_HEADER}")
                    message(FATAL_ERROR "Generated dongle animation paths must be relative to ZMK_CONFIG")
                endif()
                get_filename_component(DONGLE_ANIMATION_PROVIDER_GENERATOR
                    "${ZMK_CONFIG}/${CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_PROVIDER_GENERATOR}"
                    ABSOLUTE)
                get_filename_component(DONGLE_ANIMATION_PROVIDER_CACHE_DIR
                    "${ZMK_CONFIG}/${CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_PROVIDER_CACHE_DIR}"
                    ABSOLUTE)
                set(DONGLE_ANIMATION_PROVIDER_HEADER
                    "${DONGLE_ANIMATION_PROVIDER_CACHE_DIR}/${CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_PROVIDER_HEADER}")
                if(NOT EXISTS "${DONGLE_ANIMATION_PROVIDER_GENERATOR}")
                    message(FATAL_ERROR
                        "Dongle animation provider generator not found: ${DONGLE_ANIMATION_PROVIDER_GENERATOR}")
                endif()
                execute_process(
                    COMMAND ${PYTHON_EXECUTABLE} "${DONGLE_ANIMATION_PROVIDER_GENERATOR}"
                            --output "${DONGLE_ANIMATION_PROVIDER_HEADER}"
                    RESULT_VARIABLE DONGLE_ANIMATION_PROVIDER_RESULT
                    OUTPUT_VARIABLE DONGLE_ANIMATION_PROVIDER_OUTPUT
                    ERROR_VARIABLE DONGLE_ANIMATION_PROVIDER_ERROR
                )
                if(NOT DONGLE_ANIMATION_PROVIDER_RESULT EQUAL 0)
                    message(FATAL_ERROR
                        "Dongle animation provider generation failed:\n${DONGLE_ANIMATION_PROVIDER_OUTPUT}${DONGLE_ANIMATION_PROVIDER_ERROR}")
                endif()
                string(STRIP "${DONGLE_ANIMATION_PROVIDER_OUTPUT}" DONGLE_ANIMATION_PROVIDER_OUTPUT)
                message(STATUS "${DONGLE_ANIMATION_PROVIDER_OUTPUT}")
                zephyr_library_include_directories(${DONGLE_ANIMATION_PROVIDER_CACHE_DIR})
            else()
                if(IS_ABSOLUTE "${CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_PROVIDER_HEADER}")
                    message(FATAL_ERROR "Dongle animation provider header must be relative to ZMK_CONFIG")
                endif()
                set(DONGLE_ANIMATION_PROVIDER_HEADER
                    "${ZMK_CONFIG}/${CONFIG_ZMK_DONGLE_DISPLAY_ANIMATION_PROVIDER_HEADER}")
                zephyr_library_include_directories(${ZMK_CONFIG})
            endif()
            if(NOT EXISTS "${DONGLE_ANIMATION_PROVIDER_HEADER}")
                message(FATAL_ERROR
                    "Dongle animation provider header not found: ${DONGLE_ANIMATION_PROVIDER_HEADER}")
            endif()
        else()
            zephyr_library_sources(widgets/bongo_cat_images.c)
        endif()
    else()
        zephyr_library_sources(widgets/bongo_cat.c)
        zephyr_library_sources(widgets/bongo_cat_images.c)
    endif()
    target_sources_ifdef(CONFIG_ZMK_HID_INDICATORS app PRIVATE widgets/hid_indicators.c)
    zephyr_library_sources(widgets/layer_status.c)
    zephyr_library_sources(widgets/modifiers.c)
    zephyr_library_sources(widgets/modifiers_sym.c)
    zephyr_library_sources(widgets/output_status.c)
    zephyr_library_sources(widgets/device_name.c)
    zephyr_library_sources(widgets/output_status_sym.c)
endif()
