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
    multi_pass_rendering_sample.cpp
    multi_pass_rendering_sample.h
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
        set(options VALIDATION_LAYER VALIDATION_SYNC)
        set(one_value_args LABEL LOG SCREENSHOT_FRAMES SCREENSHOT_DIR REQUIRED_OUTPUT TIMEOUT)
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
        if(SMOKE_REQUIRED_OUTPUT)
            list(APPEND sample_smoke_args "-DKERA_SAMPLE_REQUIRED_OUTPUT=${SMOKE_REQUIRED_OUTPUT}")
        endif()

        if(SMOKE_TIMEOUT)
            list(APPEND sample_smoke_args "-DKERA_SAMPLE_TIMEOUT=${SMOKE_TIMEOUT}")
        endif()

        add_test(NAME ${test_name}
            COMMAND ${CMAKE_COMMAND}
                ${sample_smoke_args}
                -P "${CMAKE_SOURCE_DIR}/cmake/RunKeraSampleSmoke.cmake"
        )
        set_tests_properties(${test_name} PROPERTIES
            LABELS "${SMOKE_LABELS}"
            SKIP_REGULAR_EXPRESSION "KERA_GPU_SMOKE_SKIPPED"
        )
        if(SMOKE_VALIDATION_LAYER)
            set(validation_environment "VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation")
            if(SMOKE_VALIDATION_SYNC)
                list(APPEND validation_environment "VK_LAYER_VALIDATE_SYNC=1")
            endif()
            set_tests_properties(${test_name} PROPERTIES ENVIRONMENT "${validation_environment}")
        endif()
    endfunction()

    add_test(NAME kera_samples_sponza_asset_layout_smoke
        COMMAND ${CMAKE_COMMAND}
            "-DKERA_SPONZA_ASSET_DIR=$<TARGET_FILE_DIR:kera_samples>/../assets/gltf/Sponza"
            -P "${CMAKE_SOURCE_DIR}/cmake/VerifySponzaSampleAsset.cmake"
    )
    set_tests_properties(kera_samples_sponza_asset_layout_smoke PROPERTIES
        LABELS "quick;sample;asset"
    )

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
        ARGS --smoke-test --smoke-frames 2 --resize-smoke --sample-index 2
        LOG kera_many_lights_resize_smoke.log
        LABELS vulkan validation render resize render_target descriptor depth
        VALIDATION_LAYER
    )

    kera_add_sample_smoke(kera_samples_instanced_resize_smoke
        LABEL Kera_instanced_resize_smoke
        ARGS --smoke-test --smoke-frames 2 --resize-smoke --sample-index 1
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

    kera_add_sample_smoke(kera_samples_damaged_helmet_ibl_smoke
        LABEL Kera_damaged_helmet_ibl_smoke
        ARGS --smoke-test --smoke-frames 2 --sample-index 4 --hide-stats-overlay
        LOG kera_damaged_helmet_ibl_smoke.log
        LABELS vulkan validation render texture gltf descriptor ibl ktx
        VALIDATION_LAYER
    )

    kera_add_sample_smoke(kera_samples_damaged_helmet_zero_resize_smoke
        LABEL Kera_damaged_helmet_zero_resize_smoke
        ARGS --smoke-test --smoke-frames 2 --sample-index 3 --zero-resize-smoke --hide-stats-overlay
        LOG kera_damaged_helmet_zero_resize_smoke.log
        LABELS vulkan validation render resize texture gltf descriptor
        VALIDATION_LAYER
    )

    kera_add_sample_smoke(kera_samples_damaged_helmet_visual_regression_smoke
        LABEL Kera_damaged_helmet_visual_regression_smoke
        ARGS --smoke-test --smoke-frames 2 --sample-index 3 --hide-stats-overlay
        LOG kera_damaged_helmet_visual_regression_smoke.log
        LABELS vulkan validation render texture gltf descriptor visual
        SCREENSHOT_FRAMES 1
        SCREENSHOT_DIR visual_regression/damaged_helmet
        METRICS_ARGS --min-coverage 0.08 --max-coverage 0.45 --min-luma-range 0.45
    )

    kera_add_sample_smoke(kera_samples_damaged_helmet_back_visual_smoke
        LABEL Kera_damaged_helmet_back_visual_smoke
        ARGS --smoke-test --smoke-frames 2 --sample-index 3 --hide-stats-overlay --damaged-helmet-fixed-yaw-degrees 0
        LOG kera_damaged_helmet_back_visual_smoke.log
        LABELS vulkan validation render texture gltf descriptor visual
        SCREENSHOT_FRAMES 1
        SCREENSHOT_DIR visual_regression/damaged_helmet_back
        METRICS_ARGS --min-coverage 0.08 --max-coverage 0.45 --min-luma-range 0.35
    )

    kera_add_sample_smoke(kera_samples_damaged_helmet_gamma_audit_smoke
        LABEL Kera_damaged_helmet_gamma_audit_smoke
        ARGS --smoke-test --smoke-frames 2 --sample-index 3 --hide-stats-overlay --damaged-helmet-debug-view 10
        LOG kera_damaged_helmet_gamma_audit_smoke.log
        LABELS vulkan validation render texture gltf descriptor visual gamma
        SCREENSHOT_FRAMES 1
        SCREENSHOT_DIR visual_regression/damaged_helmet_gamma
        METRICS_ARGS --min-coverage 0.08 --max-coverage 0.45 --min-luma-range 0.35
    )

    kera_add_sample_smoke(kera_samples_damaged_helmet_ibl_visual_smoke
        LABEL Kera_damaged_helmet_ibl_visual_smoke
        ARGS --smoke-test --smoke-frames 2 --sample-index 4 --hide-stats-overlay --damaged-helmet-fixed-yaw-degrees 0
        LOG kera_damaged_helmet_ibl_visual_smoke.log
        LABELS vulkan validation render texture gltf descriptor ibl ktx visual
        SCREENSHOT_FRAMES 1
        SCREENSHOT_DIR visual_regression/damaged_helmet_ibl
        METRICS_ARGS --min-coverage 0.75 --max-coverage 1.00 --min-luma-range 0.45
    )

    kera_add_sample_smoke(kera_samples_multi_pass_capture_smoke
        LABEL Kera_multi_pass_capture_smoke
        TIMEOUT 45
        ARGS --smoke-test --smoke-frames 2 --sample attachment-playground --multi-pass-capture-smoke --hide-stats-overlay
        LOG kera_multi_pass_capture_smoke.log
        LABELS vulkan validation render attachment readback
        # This smoke validates offscreen readback; the shadow lens smokes below validate cast-shadow output.
        REQUIRED_OUTPUT "Multi-Pass Sponza scene capture verified."
        VALIDATION_LAYER
    )

    kera_add_sample_smoke(kera_samples_multi_pass_shadows_disabled_smoke
        LABEL Kera_multi_pass_shadows_disabled_smoke
        TIMEOUT 45
        ARGS --smoke-test --smoke-frames 2 --sample attachment-playground --multi-pass-capture-smoke --multi-pass-shadows-disabled --hide-stats-overlay
        LOG kera_multi_pass_shadows_disabled_smoke.log
        LABELS vulkan validation render attachment shadow readback
        REQUIRED_OUTPUT "Multi-Pass Sponza shadows-disabled scene capture verified."
        VALIDATION_LAYER
    )

    kera_add_sample_smoke(kera_samples_multi_pass_resize_smoke
        LABEL Kera_multi_pass_resize_smoke
        TIMEOUT 45
        ARGS --smoke-test --smoke-frames 6 --sample attachment-playground --multi-pass-resize-smoke --multi-pass-capture-smoke --hide-stats-overlay
        LOG kera_multi_pass_resize_smoke.log
        LABELS vulkan validation render attachment resize readback
        REQUIRED_OUTPUT "after final resize"
        VALIDATION_LAYER
    )

    kera_add_sample_smoke(kera_samples_multi_pass_msaa_smoke
        LABEL Kera_multi_pass_msaa_smoke
        TIMEOUT 45
        ARGS --smoke-test --smoke-frames 4 --sample attachment-playground --attachment-msaa 4 --multi-pass-capture-smoke --hide-stats-overlay --hide-sample-ui
        LOG kera_multi_pass_msaa_smoke.log
        LABELS vulkan validation render attachment msaa resolve readback visual
        REQUIRED_OUTPUT "Multi-Pass 4x MSAA resolve enabled"
        SCREENSHOT_FRAMES 3
        SCREENSHOT_DIR visual_regression/multi_pass_msaa
        METRICS_ARGS --min-coverage 0.60 --max-coverage 0.90 --min-luma-range 0.45
        VALIDATION_LAYER
    )

    kera_add_sample_smoke(kera_samples_attachment_reconfigure_smoke
        LABEL Kera_attachment_reconfigure_smoke
        TIMEOUT 45
        ARGS --smoke-test --smoke-frames 9 --sample attachment-playground --attachment-msaa 4 --attachment-reconfigure-smoke --multi-pass-capture-smoke --hide-stats-overlay
        LOG kera_attachment_reconfigure_smoke.log
        LABELS vulkan validation render attachment msaa shadow reconfigure readback synchronization
        REQUIRED_OUTPUT "ATTACHMENT_PLAYGROUND_RECONFIGURE_SMOKE complete active_msaa=4 shadow_resolution=1024 shadows=on preview=off"
        VALIDATION_LAYER
        VALIDATION_SYNC
    )

    kera_add_sample_smoke(kera_samples_multi_pass_shadow_preview_smoke
        LABEL Kera_multi_pass_shadow_preview_smoke
        TIMEOUT 45
        ARGS --smoke-test --smoke-frames 2 --sample attachment-playground --attachment-preview-shadow-map --hide-stats-overlay --hide-sample-ui
        LOG kera_multi_pass_shadow_preview_smoke.log
        LABELS vulkan validation render attachment shadow depth visual
        REQUIRED_OUTPUT "Multi-Pass shadow depth preview enabled"
        SCREENSHOT_FRAMES 1
        SCREENSHOT_DIR visual_regression/multi_pass_shadow_preview
        METRICS_ARGS --min-coverage 0.35 --max-coverage 0.75 --min-luma-range 0.70 --min-dark-coverage 0.30
        VALIDATION_LAYER
    )

    kera_add_sample_smoke(kera_samples_multi_pass_shadow_inset_smoke
        LABEL Kera_multi_pass_shadow_inset_smoke
        TIMEOUT 45
        ARGS --smoke-test --smoke-frames 4 --sample attachment-playground --attachment-shadow-inset --hide-stats-overlay --hide-sample-ui
        LOG kera_multi_pass_shadow_inset_smoke.log
        REQUIRED_OUTPUT "Multi-Pass shadow inset diagnostic enabled."
        LABELS vulkan validation render attachment shadow visual
        SCREENSHOT_FRAMES 3
        SCREENSHOT_DIR visual_regression/multi_pass_shadow_inset
        METRICS_ARGS --min-coverage 0.60 --max-coverage 0.90 --min-luma-range 0.45
        VALIDATION_LAYER
    )

    kera_add_sample_smoke(kera_samples_multi_pass_msaa_comparison_smoke
        LABEL Kera_multi_pass_msaa_comparison_smoke
        TIMEOUT 45
        ARGS --smoke-test --smoke-frames 4 --sample attachment-playground --attachment-msaa 4 --attachment-msaa-comparison --hide-stats-overlay --hide-sample-ui
        LOG kera_multi_pass_msaa_comparison_smoke.log
        REQUIRED_OUTPUT "Multi-Pass MSAA comparison diagnostic enabled."
        LABELS vulkan validation render attachment msaa resolve visual
        SCREENSHOT_FRAMES 3
        SCREENSHOT_DIR visual_regression/multi_pass_msaa_comparison
        METRICS_ARGS --min-coverage 0.60 --max-coverage 0.90 --min-luma-range 0.45 --min-msaa-marker-coverage 0.003
        VALIDATION_LAYER
        VALIDATION_SYNC
    )

    kera_add_sample_smoke(kera_samples_multi_pass_shadow_comparison_smoke
        LABEL Kera_multi_pass_shadow_comparison_smoke
        TIMEOUT 45
        ARGS --smoke-test --smoke-frames 4 --sample attachment-playground --attachment-msaa 4 --attachment-shadow-resolution 4096 --attachment-shadow-comparison --hide-stats-overlay --hide-sample-ui
        LOG kera_multi_pass_shadow_comparison_smoke.log
        REQUIRED_OUTPUT "Multi-Pass shadow comparison diagnostic enabled."
        LABELS vulkan validation render attachment shadow visual
        SCREENSHOT_FRAMES 3
        SCREENSHOT_DIR visual_regression/multi_pass_shadow_comparison
        METRICS_ARGS --min-coverage 0.60 --max-coverage 0.90 --min-luma-range 0.45 --min-shadow-marker-coverage 0.003 --min-shadow-pane-delta 0.005 --min-shadow-pane-luma-range 0.45 --min-shadow-diagonal-contrast 0.015 --min-shadow-diagonal-mean-delta 0.00015
        VALIDATION_LAYER
        VALIDATION_SYNC
    )

    kera_add_sample_smoke(kera_samples_multi_pass_shadow_comparison_single_sample_smoke
        LABEL Kera_multi_pass_shadow_comparison_single_sample_smoke
        TIMEOUT 45
        ARGS --smoke-test --smoke-frames 4 --sample attachment-playground --attachment-msaa 1 --attachment-shadow-resolution 4096 --attachment-shadow-comparison --hide-stats-overlay --hide-sample-ui
        LOG kera_multi_pass_shadow_comparison_single_sample_smoke.log
        REQUIRED_OUTPUT "ATTACHMENT_PLAYGROUND_CONFIG requested_msaa=1 active_msaa=1"
        LABELS vulkan validation render attachment shadow visual single_sample
        SCREENSHOT_FRAMES 3
        SCREENSHOT_DIR visual_regression/multi_pass_shadow_comparison_single_sample
        METRICS_ARGS --min-coverage 0.60 --max-coverage 0.90 --min-luma-range 0.45 --min-shadow-marker-coverage 0.003 --min-shadow-pane-delta 0.005 --min-shadow-pane-luma-range 0.45 --min-shadow-diagonal-contrast 0.015 --min-shadow-diagonal-mean-delta 0.00015
        VALIDATION_LAYER
        VALIDATION_SYNC
    )

    kera_add_sample_smoke(kera_samples_attachment_performance_smoke
        LABEL Kera_attachment_performance_smoke
        TIMEOUT 60
        ARGS --smoke-test --smoke-frames 10 --sample attachment-playground --attachment-msaa 4 --attachment-shadow-resolution 4096 --attachment-shadow-comparison --attachment-performance-smoke --hide-stats-overlay --hide-sample-ui
        LOG kera_attachment_performance_smoke.log
        LABELS vulkan validation render attachment performance timestamp msaa shadow synchronization
        REQUIRED_OUTPUT "ATTACHMENT_PERFORMANCE_SMOKE verified"
        VALIDATION_LAYER
        VALIDATION_SYNC
    )

    kera_add_sample_smoke(kera_samples_attachment_performance_shadows_disabled_smoke
        LABEL Kera_attachment_performance_shadows_disabled_smoke
        TIMEOUT 60
        ARGS --smoke-test --smoke-frames 10 --sample attachment-playground --attachment-msaa 1 --multi-pass-shadows-disabled --attachment-performance-smoke --hide-stats-overlay --hide-sample-ui
        LOG kera_attachment_performance_shadows_disabled_smoke.log
        LABELS vulkan validation render attachment performance timestamp shadow synchronization
        REQUIRED_OUTPUT "expected_count=3 observed_count=3"
        VALIDATION_LAYER
        VALIDATION_SYNC
    )

    kera_add_sample_smoke(kera_samples_attachment_performance_ui_smoke
        LABEL Kera_attachment_performance_ui_smoke
        TIMEOUT 60
        ARGS --smoke-test --smoke-frames 10 --sample attachment-playground --attachment-msaa 4 --attachment-shadow-resolution 4096 --attachment-shadow-comparison --attachment-performance-smoke --hide-stats-overlay
        LOG kera_attachment_performance_ui_smoke.log
        LABELS vulkan validation render attachment performance timestamp msaa shadow synchronization visual ui
        SCREENSHOT_FRAMES 9
        SCREENSHOT_DIR visual_regression/attachment_performance_ui
        METRICS_ARGS --min-coverage 0.40 --max-coverage 0.95 --min-luma-range 0.45 --min-ui-status-coverage 0.50
        REQUIRED_OUTPUT "ATTACHMENT_PERFORMANCE_SMOKE verified"
        VALIDATION_LAYER
        VALIDATION_SYNC
    )

    kera_add_sample_smoke(kera_samples_multi_pass_visual_smoke
        LABEL Kera_multi_pass_visual_smoke
        TIMEOUT 45
        ARGS --smoke-test --smoke-frames 4 --sample attachment-playground --hide-stats-overlay --hide-sample-ui
        LOG kera_multi_pass_visual_smoke.log
        LABELS vulkan validation render attachment visual
        SCREENSHOT_FRAMES 3
        SCREENSHOT_DIR visual_regression/multi_pass
        METRICS_ARGS --min-coverage 0.60 --max-coverage 0.90 --min-luma-range 0.45
        VALIDATION_LAYER
    )
endif()
