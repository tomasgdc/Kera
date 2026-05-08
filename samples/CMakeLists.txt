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
    ${SHADER_FILES}
)
target_link_libraries(kera_samples PRIVATE kera)

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
