add_executable(kera_renderer_lifecycle_tests
    renderer_lifecycle_tests.cpp
)

target_link_libraries(kera_renderer_lifecycle_tests PRIVATE kera)

add_test(NAME kera_renderer_lifecycle_tests COMMAND kera_renderer_lifecycle_tests)
set_tests_properties(kera_renderer_lifecycle_tests PROPERTIES
    LABELS "quick;renderer;command_buffer"
)

add_executable(kera_slang_reflection_metadata_tests
    slang_reflection_metadata_tests.cpp
)

target_link_libraries(kera_slang_reflection_metadata_tests PRIVATE kera)

add_executable(kera_reflection_contracts_tests
    reflection_contracts_tests.cpp
)

target_link_libraries(kera_reflection_contracts_tests PRIVATE kera)

add_test(NAME kera_reflection_contracts_tests COMMAND kera_reflection_contracts_tests)
set_tests_properties(kera_reflection_contracts_tests PROPERTIES
    LABELS "quick;renderer;shader;reflection;descriptor"
)

add_executable(kera_renderer_public_api_tests
    renderer_public_api_tests.cpp
)

target_link_libraries(kera_renderer_public_api_tests PRIVATE kera)

add_test(NAME kera_renderer_public_api_tests COMMAND kera_renderer_public_api_tests)
set_tests_properties(kera_renderer_public_api_tests PROPERTIES
    LABELS "quick;renderer;api"
)
add_executable(kera_logger_tests
    logger_tests.cpp
)

target_link_libraries(kera_logger_tests PRIVATE kera)

add_test(NAME kera_logger_tests COMMAND kera_logger_tests)
set_tests_properties(kera_logger_tests PROPERTIES
    LABELS "quick;logger;utilities"
)

add_test(NAME kera_renderer_abi_header_tests
    COMMAND ${CMAKE_COMMAND}
        -D KERA_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P "${CMAKE_SOURCE_DIR}/cmake/ValidateRendererAbiHeaders.cmake"
)
set_tests_properties(kera_renderer_abi_header_tests PROPERTIES
    LABELS "quick;renderer;api;abi"
)

add_test(NAME kera_logging_rule_tests
    COMMAND ${CMAKE_COMMAND}
        -D KERA_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P "${CMAKE_SOURCE_DIR}/cmake/ValidateLoggingRules.cmake"
)
set_tests_properties(kera_logging_rule_tests PROPERTIES
    LABELS "quick;logger;api"
)

if(KERA_SLANGC_EXECUTABLE)
    add_test(NAME kera_slang_reflection_tests
        COMMAND ${CMAKE_COMMAND}
            -D KERA_SHADER_ROOT=${CMAKE_SOURCE_DIR}/samples/shaders
            -D KERA_SLANGC_EXECUTABLE=${KERA_SLANGC_EXECUTABLE}
            -D KERA_REFLECTION_OUTPUT_DIR=${CMAKE_BINARY_DIR}/shader-reflection
            -D KERA_REFLECTION_METADATA_TEST_EXE=$<TARGET_FILE:kera_slang_reflection_metadata_tests>
            -P "${CMAKE_SOURCE_DIR}/cmake/ReflectSlangShaderContracts.cmake"
    )
    set_tests_properties(kera_slang_reflection_tests PROPERTIES
        LABELS "quick;shader;slang;reflection"
    )
endif()

add_test(NAME kera_slang_shader_contracts
    COMMAND ${CMAKE_COMMAND}
        -D KERA_SHADER_ROOT=${CMAKE_SOURCE_DIR}/samples/shaders
        -P "${CMAKE_SOURCE_DIR}/cmake/ValidateSlangShaderContracts.cmake"
)
set_tests_properties(kera_slang_shader_contracts PROPERTIES
    LABELS "quick;shader;slang"
)
