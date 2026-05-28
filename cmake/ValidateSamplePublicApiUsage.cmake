# Copyright 2026 Tomas Mikalauskas
# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED KERA_SOURCE_DIR)
    message(FATAL_ERROR "KERA_SOURCE_DIR must be provided")
endif()

file(GLOB_RECURSE KERA_SAMPLE_SOURCES
    "${KERA_SOURCE_DIR}/samples/*.h"
    "${KERA_SOURCE_DIR}/samples/*.cpp"
)

set(KERA_FORBIDDEN_SAMPLE_KERA_HEADERS
    "kera/renderer/interfaces.h"
    "kera/renderer/factory.h"
    "kera/renderer/gltf_loader.h"
    "kera/renderer/reflection_contracts.h"
    "kera/renderer/descriptors.h"
    "kera/renderer/result.h"
    "kera/renderer/backend/"
    "kera/utilities/logger.h"
    "kera/core/"
)

foreach(source IN LISTS KERA_SAMPLE_SOURCES)
    file(READ "${source}" content)
    foreach(pattern IN LISTS KERA_FORBIDDEN_SAMPLE_KERA_HEADERS)
        string(FIND "${content}" "${pattern}" found_at)
        if(NOT found_at EQUAL -1)
            message(FATAL_ERROR "Sample source ${source} includes forbidden Kera private/internal header pattern: ${pattern}")
        endif()
    endforeach()
endforeach()

message(STATUS "Samples use the public STL-free Kera renderer API boundary.")
