add_executable(kera_renderer_lifecycle_tests
    renderer_lifecycle_tests.cpp
)

target_link_libraries(kera_renderer_lifecycle_tests PRIVATE kera)

add_test(NAME kera_renderer_lifecycle_tests COMMAND kera_renderer_lifecycle_tests)
set_tests_properties(kera_renderer_lifecycle_tests PROPERTIES
    LABELS "quick;renderer;command_buffer"
)

add_executable(kera_logger_tests
    logger_tests.cpp
)

target_link_libraries(kera_logger_tests PRIVATE kera)

add_test(NAME kera_logger_tests COMMAND kera_logger_tests)
set_tests_properties(kera_logger_tests PROPERTIES
    LABELS "quick;logger;utilities"
)
add_test(NAME kera_logging_rule_tests
    COMMAND ${CMAKE_COMMAND}
        -D KERA_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P "${CMAKE_SOURCE_DIR}/cmake/ValidateLoggingRules.cmake"
)
set_tests_properties(kera_logging_rule_tests PROPERTIES
    LABELS "quick;logger;api"
)
add_test(NAME kera_slang_shader_contracts
    COMMAND ${CMAKE_COMMAND}
        -D KERA_SHADER_ROOT=${CMAKE_SOURCE_DIR}/samples/shaders
        -P "${CMAKE_SOURCE_DIR}/cmake/ValidateSlangShaderContracts.cmake"
)
set_tests_properties(kera_slang_shader_contracts PROPERTIES
    LABELS "quick;shader;slang"
)
