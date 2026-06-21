# Copyright 2026 Tomas Mikalauskas
# SPDX-License-Identifier: Apache-2.0

# Samples for Kera Rendering Library

# Collect shader files so IDE generators show them under the sample target.
file(GLOB SHADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.slang")

# Main sample application
add_executable(kera_samples
    main.cpp
    samples.cpp
    samples.h
    render_context.cpp
    render_context.h
    stats_overlay.cpp
    stats_overlay.h
    sample_utils.cpp
    sample_utils.h
    basic_triangle_sample.cpp
    basic_triangle_sample.h
    instanced_triangle_sample.cpp
    instanced_triangle_sample.h
    instanced_triangle_many_lights_sample.cpp
    instanced_triangle_many_lights_sample.h
    damaged_helmet_sample.cpp
    damaged_helmet_sample.h
    damaged_helmet_ibl_lighting_sample.cpp
    damaged_helmet_ibl_lighting_sample.h
    ${SHADER_FILES}
)
target_link_libraries(kera_samples PRIVATE kera)
kera_configure_no_exceptions(kera_samples)

add_executable(kera_ppm_metrics
    ppm_metrics.cpp
    sample_utils.cpp
    sample_utils.h
)
target_link_libraries(kera_ppm_metrics PRIVATE kera)
kera_configure_no_exceptions(kera_ppm_metrics)

if(WIN32)
    if(KERA_BUILD_SHARED OR KERA_SLANG_TARGET)
        add_custom_command(TARGET kera_samples POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:kera_samples>
                $<TARGET_FILE_DIR:kera_samples>
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    endif()

    if(KERA_SLANG_RUNTIME_DLLS)
        add_custom_command(TARGET kera_samples POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${KERA_SLANG_RUNTIME_DLLS}
                $<TARGET_FILE_DIR:kera_samples>
            VERBATIM
        )
    endif()
endif()

source_group("Shaders" FILES ${SHADER_FILES})

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
    # Copy shader sources next to the sample executable for runtime loading.
    add_custom_command(TARGET kera_samples POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
            "${CMAKE_BINARY_DIR}/samples/shaders"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config")
    add_custom_command(TARGET kera_samples POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/config"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/config"
            "${CMAKE_BINARY_DIR}/samples/config"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    add_custom_command(TARGET kera_samples POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "${CMAKE_BINARY_DIR}/samples/assets"
    )
endif()

if(BUILD_TESTING)
    function(kera_add_sample_smoke test_name)
        set(options VALIDATION_LAYER)
        set(one_value_args LABEL LOG SCREENSHOT_FRAMES SCREENSHOT_DIR)
        set(multi_value_args ARGS LABELS METRICS_ARGS)
        cmake_parse_arguments(SMOKE "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

        list(JOIN SMOKE_ARGS "\\;" sample_args)
        list(JOIN SMOKE_METRICS_ARGS "\\;" sample_metrics_args)
        set(sample_smoke_args
            "-DKERA_SAMPLE_EXE=$<TARGET_FILE:kera_samples>"
            "-DKERA_SAMPLE_LABEL=${SMOKE_LABEL}"
            "-DKERA_SAMPLE_ARGS=${sample_args}"
            "-DKERA_SAMPLE_LOG=$<TARGET_FILE_DIR:kera_samples>/${SMOKE_LOG}"
        )
        if(SMOKE_SCREENSHOT_FRAMES)
            list(APPEND sample_smoke_args "-DKERA_SAMPLE_SCREENSHOT_FRAMES=${SMOKE_SCREENSHOT_FRAMES}")
        endif()
        if(SMOKE_SCREENSHOT_DIR)
            list(APPEND sample_smoke_args
                "-DKERA_SAMPLE_SCREENSHOT_DIR=$<TARGET_FILE_DIR:kera_samples>/${SMOKE_SCREENSHOT_DIR}"
                "-DKERA_SAMPLE_SCREENSHOT_METRICS_EXE=$<TARGET_FILE:kera_ppm_metrics>"
                "-DKERA_SAMPLE_SCREENSHOT_METRICS_ARGS=${sample_metrics_args}"
            )
        endif()

        add_test(NAME ${test_name}
            COMMAND ${CMAKE_COMMAND}
                ${sample_smoke_args}
                -P "${CMAKE_SOURCE_DIR}/cmake/RunKeraSampleSmoke.cmake"
        )
        set_tests_properties(${test_name} PROPERTIES LABELS "${SMOKE_LABELS}")
        if(SMOKE_VALIDATION_LAYER)
            set_tests_properties(${test_name} PROPERTIES
                ENVIRONMENT "VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation"
            )
        endif()
    endfunction()

    kera_add_sample_smoke(kera_samples_validation_smoke
        LABEL Kera_validation_sample_smoke
        ARGS --smoke-test --smoke-frames 1
        LOG kera_validation_smoke.log
        LABELS vulkan validation render
        VALIDATION_LAYER
    )

    kera_add_sample_smoke(kera_samples_resize_smoke
        LABEL Kera_resize_sample_smoke
        ARGS --smoke-test --smoke-frames 2 --resize-smoke
        LOG kera_resize_smoke.log
        LABELS vulkan render resize
    )

    kera_add_sample_smoke(kera_samples_many_lights_smoke
        LABEL Kera_many_lights_sample_smoke
        ARGS --smoke-test --smoke-frames 2 --sample-index 2
        LOG kera_many_lights_smoke.log
        LABELS vulkan validation render descriptor render_target
        VALIDATION_LAYER
    )

    kera_add_sample_smoke(kera_samples_depth_smoke
        LABEL Kera_depth_sample_smoke
        ARGS --smoke-test --smoke-frames 2 --sample-index 2
        LOG kera_depth_smoke.log
        LABELS vulkan validation render render_target depth
        VALIDATION_LAYER
    )

    kera_add_sample_smoke(kera_samples_many_lights_resize_smoke
        LABEL Kera_many_lights_resize_smoke
        ARGS --smoke-test --smoke-frames 3 --resize-smoke --sample-index 2
        LOG kera_many_lights_resize_smoke.log
        LABELS vulkan validation render resize render_target descriptor depth
        VALIDATION_LAYER
    )

    kera_add_sample_smoke(kera_samples_instanced_resize_smoke
        LABEL Kera_instanced_resize_smoke
        ARGS --smoke-test --smoke-frames 3 --resize-smoke --sample-index 1
        LOG kera_instanced_resize_smoke.log
        LABELS vulkan validation render resize descriptor
        VALIDATION_LAYER
    )

    kera_add_sample_smoke(kera_samples_damaged_helmet_smoke
        LABEL Kera_damaged_helmet_smoke
        ARGS --smoke-test --smoke-frames 2 --sample-index 3 --hide-stats-overlay
        LOG kera_damaged_helmet_smoke.log
        LABELS vulkan validation render texture gltf descriptor
        VALIDATION_LAYER
    )

    kera_add_sample_smoke(kera_samples_damaged_helmet_zero_resize_smoke
        LABEL Kera_damaged_helmet_zero_resize_smoke
        ARGS --smoke-test --smoke-frames 3 --sample-index 3 --zero-resize-smoke --hide-stats-overlay
        LOG kera_damaged_helmet_zero_resize_smoke.log
        LABELS vulkan validation render resize texture gltf descriptor
        VALIDATION_LAYER
    )

    kera_add_sample_smoke(kera_samples_damaged_helmet_visual_regression_smoke
        LABEL Kera_damaged_helmet_visual_regression_smoke
        ARGS --smoke-test --smoke-frames 40 --sample-index 3 --hide-stats-overlay
        LOG kera_damaged_helmet_visual_regression_smoke.log
        LABELS vulkan validation render texture gltf descriptor visual
        SCREENSHOT_FRAMES 24
        SCREENSHOT_DIR visual_regression/damaged_helmet
        METRICS_ARGS --min-coverage 0.08 --max-coverage 0.45 --min-luma-range 0.45
    )

    kera_add_sample_smoke(kera_samples_damaged_helmet_back_visual_smoke
        LABEL Kera_damaged_helmet_back_visual_smoke
        ARGS --smoke-test --smoke-frames 3 --sample-index 3 --hide-stats-overlay --damaged-helmet-fixed-yaw-degrees 0
        LOG kera_damaged_helmet_back_visual_smoke.log
        LABELS vulkan validation render texture gltf descriptor visual
        SCREENSHOT_FRAMES 2
        SCREENSHOT_DIR visual_regression/damaged_helmet_back
        METRICS_ARGS --min-coverage 0.08 --max-coverage 0.45 --min-luma-range 0.35
    )

    kera_add_sample_smoke(kera_samples_damaged_helmet_gamma_audit_smoke
        LABEL Kera_damaged_helmet_gamma_audit_smoke
        ARGS --smoke-test --smoke-frames 40 --sample-index 3 --hide-stats-overlay --damaged-helmet-debug-view 10
        LOG kera_damaged_helmet_gamma_audit_smoke.log
        LABELS vulkan validation render texture gltf descriptor visual gamma
        SCREENSHOT_FRAMES 24
        SCREENSHOT_DIR visual_regression/damaged_helmet_gamma
        METRICS_ARGS --min-coverage 0.08 --max-coverage 0.45 --min-luma-range 0.35
    )
endif()
