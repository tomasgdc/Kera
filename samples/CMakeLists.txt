# Samples for Kera Rendering Library

# Main sample application
add_executable(kera_samples
    main.cpp
    samples.cpp
    basic_triangle_sample.cpp
    compute_sample.cpp
)
target_link_libraries(kera_samples PRIVATE kera SDL3::SDL3)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
    add_custom_command(TARGET kera_samples POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
            "${CMAKE_BINARY_DIR}/samples/shaders"
    )
endif()

# Add sample subdirectories here if needed for additional organization
# add_subdirectory(basic_triangle)
# add_subdirectory(compute_example)
