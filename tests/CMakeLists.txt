# Copyright 2026 Tomas Mikalauskas
# SPDX-License-Identifier: Apache-2.0

set(KERA_INTERNAL_TEST_TARGET kera)
if(TARGET kera_internal_tests)
    set(KERA_INTERNAL_TEST_TARGET kera_internal_tests)
endif()

function(kera_copy_test_runtime_dlls target_name)
    if(WIN32)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${target_name}>
                $<TARGET_FILE_DIR:${target_name}>
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
        if(KERA_SLANG_RUNTIME_DLLS)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${KERA_SLANG_RUNTIME_DLLS}
                    $<TARGET_FILE_DIR:${target_name}>
                VERBATIM
            )
        endif()
    endif()
endfunction()

add_executable(kera_renderer_lifecycle_tests
    renderer_lifecycle_tests.cpp
)

target_link_libraries(kera_renderer_lifecycle_tests PRIVATE ${KERA_INTERNAL_TEST_TARGET})
kera_copy_test_runtime_dlls(kera_renderer_lifecycle_tests)

add_test(NAME kera_renderer_lifecycle_tests COMMAND kera_renderer_lifecycle_tests)
set_tests_properties(kera_renderer_lifecycle_tests PROPERTIES
    LABELS "quick;renderer;command_buffer"
)

add_executable(kera_slang_reflection_metadata_tests
    slang_reflection_metadata_tests.cpp
)

target_link_libraries(kera_slang_reflection_metadata_tests PRIVATE ${KERA_INTERNAL_TEST_TARGET})
kera_copy_test_runtime_dlls(kera_slang_reflection_metadata_tests)

add_executable(kera_reflection_contracts_tests
    reflection_contracts_tests.cpp
)

target_link_libraries(kera_reflection_contracts_tests PRIVATE ${KERA_INTERNAL_TEST_TARGET})
kera_copy_test_runtime_dlls(kera_reflection_contracts_tests)

add_test(NAME kera_reflection_contracts_tests COMMAND kera_reflection_contracts_tests)
set_tests_properties(kera_reflection_contracts_tests PROPERTIES
    LABELS "quick;renderer;shader;reflection;descriptor"
)

add_executable(kera_renderer_public_api_tests
    renderer_public_api_tests.cpp
)

target_link_libraries(kera_renderer_public_api_tests PRIVATE kera)
kera_copy_test_runtime_dlls(kera_renderer_public_api_tests)

add_test(NAME kera_renderer_public_api_tests COMMAND kera_renderer_public_api_tests)
set_tests_properties(kera_renderer_public_api_tests PROPERTIES
    LABELS "quick;renderer;api"
)

add_executable(kera_gltf_loader_tests
    gltf_loader_tests.cpp
)

target_link_libraries(kera_gltf_loader_tests PRIVATE ${KERA_INTERNAL_TEST_TARGET})
kera_copy_test_runtime_dlls(kera_gltf_loader_tests)
target_compile_definitions(kera_gltf_loader_tests PRIVATE
    KERA_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME kera_gltf_loader_tests COMMAND kera_gltf_loader_tests)
set_tests_properties(kera_gltf_loader_tests PROPERTIES
    LABELS "quick;renderer;gltf;texture"
)

add_executable(kera_logger_tests
    logger_tests.cpp
)

target_link_libraries(kera_logger_tests PRIVATE ${KERA_INTERNAL_TEST_TARGET})
kera_copy_test_runtime_dlls(kera_logger_tests)

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

add_test(NAME kera_sample_public_api_usage_tests
    COMMAND ${CMAKE_COMMAND}
        -D KERA_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P "${CMAKE_SOURCE_DIR}/cmake/ValidateSamplePublicApiUsage.cmake"
)
set_tests_properties(kera_sample_public_api_usage_tests PROPERTIES
    LABELS "quick;renderer;api;samples"
)

add_test(NAME kera_logging_rule_tests
    COMMAND ${CMAKE_COMMAND}
        -D KERA_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P "${CMAKE_SOURCE_DIR}/cmake/ValidateLoggingRules.cmake"
)
set_tests_properties(kera_logging_rule_tests PROPERTIES
    LABELS "quick;logger;api"
)

add_test(NAME kera_copyright_header_tests
    COMMAND ${CMAKE_COMMAND}
        -D KERA_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P "${CMAKE_SOURCE_DIR}/cmake/ValidateCopyrightHeaders.cmake"
)
set_tests_properties(kera_copyright_header_tests PROPERTIES
    LABELS "quick;license;docs"
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
