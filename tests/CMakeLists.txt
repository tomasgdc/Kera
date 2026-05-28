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

add_executable(kera_unit_tests
    renderer_lifecycle_tests.cpp
    reflection_contracts_tests.cpp
    renderer_public_api_tests.cpp
    gltf_loader_tests.cpp
    logger_tests.cpp
    repo_policy_tests.cpp
    slang_reflection_metadata_tests.cpp
)

target_link_libraries(kera_unit_tests PRIVATE ${KERA_INTERNAL_TEST_TARGET} GTest::gtest_main)
target_compile_definitions(kera_unit_tests PRIVATE
    KERA_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
kera_copy_test_runtime_dlls(kera_unit_tests)

function(kera_add_unit_test test_name filter labels)
    add_test(NAME ${test_name}
        COMMAND kera_unit_tests --gtest_filter=${filter}
    )
    set_tests_properties(${test_name} PROPERTIES
        LABELS "${labels}"
    )
endfunction()

kera_add_unit_test(kera_unit_renderer_lifecycle_tests "KeraRendererLifecycle.*" "quick;renderer;command_buffer")
kera_add_unit_test(kera_unit_reflection_contracts_tests "KeraReflectionContracts.*" "quick;renderer;shader;reflection;descriptor")
kera_add_unit_test(kera_unit_renderer_public_api_tests "KeraRendererPublicApi.*" "quick;renderer;api")
kera_add_unit_test(kera_unit_gltf_loader_tests "KeraGltfLoader.*" "quick;renderer;gltf;texture")
kera_add_unit_test(kera_unit_logger_tests "KeraLogger.*" "quick;logger;utilities")
kera_add_unit_test(kera_unit_repo_policy_abi_tests "KeraRepoPolicy.PublicAbiHeadersAreStlFree" "quick;renderer;api;abi")
kera_add_unit_test(kera_unit_repo_policy_sample_api_tests "KeraRepoPolicy.SamplesUsePublicKeraApiOnly" "quick;renderer;api;samples")
kera_add_unit_test(kera_unit_repo_policy_logging_tests "KeraRepoPolicy.LoggingRulesStayBehindLoggerFacade" "quick;logger;api")
kera_add_unit_test(kera_unit_slang_shader_contracts "KeraShaderContracts.SourceEntryPointsAndStageMarkers" "quick;shader;slang")

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
            -D KERA_UNIT_TEST_EXE=$<TARGET_FILE:kera_unit_tests>
            -P "${CMAKE_SOURCE_DIR}/cmake/ReflectSlangShaderContracts.cmake"
    )
    set_tests_properties(kera_slang_reflection_tests PROPERTIES
        LABELS "quick;shader;slang;reflection"
    )
endif()
