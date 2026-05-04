# Samples for Kera Rendering Library

# Collect shader files for Visual Studio project
file(GLOB SHADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.slang")

# Main sample application
add_executable(kera_samples
    main.cpp
    samples.cpp
    basic_triangle_sample.cpp
    compute_sample.cpp
    instanced_triangle.cpp
    ${SHADER_FILES}  # Include shaders in the project
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

# Organize shaders in Visual Studio
source_group("Shaders" FILES ${SHADER_FILES})

# Compile Slang shaders to SPIR-V
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
    set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/samples/shaders")
    #compile_slang_shaders(kera_samples "${CMAKE_CURRENT_SOURCE_DIR}/shaders" "${SHADER_OUTPUT_DIR}")
    
    # Copy any non-slang shader files (like compiled SPIR-V if not using slangc)
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

# Add sample subdirectories here if needed for additional organization
# add_subdirectory(basic_triangle)
# add_subdirectory(compute_example)
