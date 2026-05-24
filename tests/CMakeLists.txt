add_executable(kera_renderer_lifecycle_tests
    renderer_lifecycle_tests.cpp
)

target_link_libraries(kera_renderer_lifecycle_tests PRIVATE kera)

add_test(NAME kera_renderer_lifecycle_tests COMMAND kera_renderer_lifecycle_tests)
set_tests_properties(kera_renderer_lifecycle_tests PROPERTIES
    LABELS "quick;renderer;command_buffer"
)
