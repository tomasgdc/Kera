# Samples for Kera Rendering Library

# Main sample application
add_executable(kera_samples 
    main.cpp 
    samples.cpp
    basic_triangle_sample.cpp
    compute_sample.cpp
)
target_link_libraries(kera_samples PRIVATE kera)

# Add sample subdirectories here if needed for additional organization
# add_subdirectory(basic_triangle)
# add_subdirectory(compute_example)