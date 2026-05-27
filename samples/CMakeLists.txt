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
    compute_sample.cpp
    compute_sample.h
    instanced_triangle_sample.cpp
    instanced_triangle_sample.h
    instanced_triangle_many_lights_sample.cpp
    instanced_triangle_many_lights_sample.h
    ${SHADER_FILES}
)
target_link_libraries(kera_samples PRIVATE kera)
kera_configure_no_exceptions(kera_samples)

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

if(BUILD_TESTING)
    add_test(NAME kera_samples_validation_smoke
        COMMAND ${CMAKE_COMMAND}
            -D KERA_SAMPLE_EXE=$<TARGET_FILE:kera_samples>
            -D KERA_SAMPLE_LABEL=Kera_validation_sample_smoke
            "-D KERA_SAMPLE_ARGS=--smoke-test;--smoke-frames;1"
            -D KERA_SAMPLE_LOG=$<TARGET_FILE_DIR:kera_samples>/kera_validation_smoke.log
            -P "${CMAKE_SOURCE_DIR}/cmake/RunKeraSampleSmoke.cmake"
    )
    set_tests_properties(kera_samples_validation_smoke PROPERTIES
        LABELS "vulkan;validation;render"
        ENVIRONMENT "VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation"
    )

    add_test(NAME kera_samples_resize_smoke
        COMMAND ${CMAKE_COMMAND}
            -D KERA_SAMPLE_EXE=$<TARGET_FILE:kera_samples>
            -D KERA_SAMPLE_LABEL=Kera_resize_sample_smoke
            "-D KERA_SAMPLE_ARGS=--smoke-test;--smoke-frames;2;--resize-smoke"
            -D KERA_SAMPLE_LOG=$<TARGET_FILE_DIR:kera_samples>/kera_resize_smoke.log
            -P "${CMAKE_SOURCE_DIR}/cmake/RunKeraSampleSmoke.cmake"
    )
    set_tests_properties(kera_samples_resize_smoke PROPERTIES
        LABELS "vulkan;render;resize"
    )

    add_test(NAME kera_samples_many_lights_smoke
        COMMAND ${CMAKE_COMMAND}
            -D KERA_SAMPLE_EXE=$<TARGET_FILE:kera_samples>
            -D KERA_SAMPLE_LABEL=Kera_many_lights_sample_smoke
            "-D KERA_SAMPLE_ARGS=--smoke-test;--smoke-frames;2;--sample-index;2"
            -D KERA_SAMPLE_LOG=$<TARGET_FILE_DIR:kera_samples>/kera_many_lights_smoke.log
            -P "${CMAKE_SOURCE_DIR}/cmake/RunKeraSampleSmoke.cmake"
    )
    set_tests_properties(kera_samples_many_lights_smoke PROPERTIES
        LABELS "vulkan;validation;render;descriptor;render_target"
        ENVIRONMENT "VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation"
    )

    add_test(NAME kera_samples_depth_smoke
        COMMAND ${CMAKE_COMMAND}
            -D KERA_SAMPLE_EXE=$<TARGET_FILE:kera_samples>
            -D KERA_SAMPLE_LABEL=Kera_depth_sample_smoke
            "-D KERA_SAMPLE_ARGS=--smoke-test;--smoke-frames;2;--sample-index;2"
            -D KERA_SAMPLE_LOG=$<TARGET_FILE_DIR:kera_samples>/kera_depth_smoke.log
            -P "${CMAKE_SOURCE_DIR}/cmake/RunKeraSampleSmoke.cmake"
    )
    set_tests_properties(kera_samples_depth_smoke PROPERTIES
        LABELS "vulkan;validation;render;render_target;depth"
        ENVIRONMENT "VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation"
    )

    add_test(NAME kera_samples_many_lights_resize_smoke
        COMMAND ${CMAKE_COMMAND}
            -D KERA_SAMPLE_EXE=$<TARGET_FILE:kera_samples>
            -D KERA_SAMPLE_LABEL=Kera_many_lights_resize_smoke
            "-D KERA_SAMPLE_ARGS=--smoke-test;--smoke-frames;3;--resize-smoke;--sample-index;2"
            -D KERA_SAMPLE_LOG=$<TARGET_FILE_DIR:kera_samples>/kera_many_lights_resize_smoke.log
            -P "${CMAKE_SOURCE_DIR}/cmake/RunKeraSampleSmoke.cmake"
    )
    set_tests_properties(kera_samples_many_lights_resize_smoke PROPERTIES
        LABELS "vulkan;validation;render;resize;render_target;descriptor;depth"
        ENVIRONMENT "VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation"
    )

    add_test(NAME kera_samples_instanced_resize_smoke
        COMMAND ${CMAKE_COMMAND}
            -D KERA_SAMPLE_EXE=$<TARGET_FILE:kera_samples>
            -D KERA_SAMPLE_LABEL=Kera_instanced_resize_smoke
            "-D KERA_SAMPLE_ARGS=--smoke-test;--smoke-frames;3;--resize-smoke;--sample-index;1"
            -D KERA_SAMPLE_LOG=$<TARGET_FILE_DIR:kera_samples>/kera_instanced_resize_smoke.log
            -P "${CMAKE_SOURCE_DIR}/cmake/RunKeraSampleSmoke.cmake"
    )
    set_tests_properties(kera_samples_instanced_resize_smoke PROPERTIES
        LABELS "vulkan;validation;render;resize;descriptor"
        ENVIRONMENT "VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation"
    )
endif()
