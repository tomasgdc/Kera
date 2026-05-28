# Copyright 2026 Tomas Mikalauskas
# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED KERA_SOURCE_DIR)
    message(FATAL_ERROR "KERA_SOURCE_DIR must be provided")
endif()

file(TO_CMAKE_PATH "${KERA_SOURCE_DIR}" KERA_SOURCE_DIR_NORMALIZED)

set(KERA_ALLOWED_CONSOLE_FILES
    "${KERA_SOURCE_DIR_NORMALIZED}/samples/main.cpp"
)

set(KERA_ALLOWED_SPDLOG_FILES
    "${KERA_SOURCE_DIR_NORMALIZED}/src/utilities/logger.cpp"
)

set(KERA_PUBLIC_LOGGING_HEADERS
    "${KERA_SOURCE_DIR_NORMALIZED}/include/kera/kera.h"
    "${KERA_SOURCE_DIR_NORMALIZED}/include/kera/renderer/api.h"
    "${KERA_SOURCE_DIR_NORMALIZED}/include/kera/renderer/abi.h"
)

file(GLOB_RECURSE KERA_LOGGING_RULE_FILES
    "${KERA_SOURCE_DIR_NORMALIZED}/include/*.h"
    "${KERA_SOURCE_DIR_NORMALIZED}/src/*.cpp"
    "${KERA_SOURCE_DIR_NORMALIZED}/src/*.h"
    "${KERA_SOURCE_DIR_NORMALIZED}/samples/*.cpp"
    "${KERA_SOURCE_DIR_NORMALIZED}/samples/*.h"
)

foreach(file IN LISTS KERA_LOGGING_RULE_FILES)
    file(TO_CMAKE_PATH "${file}" normalized_file)
    file(READ "${file}" content)

    list(FIND KERA_ALLOWED_CONSOLE_FILES "${normalized_file}" console_allowed)
    if(console_allowed EQUAL -1 AND content MATCHES "std::(cout|cerr)|(^|[^A-Za-z_])printf[ \t]*\\(|(^|[^A-Za-z_])fprintf[ \t]*\\(")
        message(FATAL_ERROR "Production logging must use kera::Logger, but ${normalized_file} contains direct console output.")
    endif()

    list(FIND KERA_ALLOWED_SPDLOG_FILES "${normalized_file}" spdlog_allowed)
    if(spdlog_allowed EQUAL -1 AND content MATCHES "spdlog::|#include[ \t]*<spdlog")
        message(FATAL_ERROR "Raw spdlog usage is only allowed in src/utilities/logger.cpp, but found it in ${normalized_file}.")
    endif()
endforeach()

foreach(header IN LISTS KERA_PUBLIC_LOGGING_HEADERS)
    file(READ "${header}" content)
    if(content MATCHES "spdlog::|#include[ \t]*<spdlog")
        message(FATAL_ERROR "Installed public header ${header} must not expose spdlog.")
    endif()
endforeach()

message(STATUS "Kera logging rules passed.")
