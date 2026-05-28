# Copyright 2026 Tomas Mikalauskas
# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED KERA_SOURCE_DIR)
    message(FATAL_ERROR "KERA_SOURCE_DIR must be provided")
endif()

set(KERA_REQUIRED_COPYRIGHT "Copyright 2026 Tomas Mikalauskas")
set(KERA_REQUIRED_SPDX "SPDX-License-Identifier: Apache-2.0")

file(GLOB_RECURSE KERA_COPYRIGHT_CANDIDATES
    "${KERA_SOURCE_DIR}/*.h"
    "${KERA_SOURCE_DIR}/*.hpp"
    "${KERA_SOURCE_DIR}/*.c"
    "${KERA_SOURCE_DIR}/*.cpp"
    "${KERA_SOURCE_DIR}/*.cc"
    "${KERA_SOURCE_DIR}/*.cxx"
    "${KERA_SOURCE_DIR}/*.slang"
    "${KERA_SOURCE_DIR}/*.cmake"
    "${KERA_SOURCE_DIR}/*.ps1"
    "${KERA_SOURCE_DIR}/*.yml"
    "${KERA_SOURCE_DIR}/*.yaml"
    "${KERA_SOURCE_DIR}/*.md"
    "${KERA_SOURCE_DIR}/*.txt"
    "${KERA_SOURCE_DIR}/*/CMakeLists.txt"
)

list(APPEND KERA_COPYRIGHT_CANDIDATES
    "${KERA_SOURCE_DIR}/CMakeLists.txt"
)

set(KERA_COPYRIGHT_EXCLUDED_PATH_PATTERNS
    "/.git/"
    "/.tmp/"
    "/.vs/"
    "/.vscode/"
    "/build/"
    "/out/"
    "/tmp/"
    "/third_party/"
    "/samples/assets/"
)

set(KERA_COPYRIGHT_MISSING_FILES "")

foreach(candidate IN LISTS KERA_COPYRIGHT_CANDIDATES)
    if(NOT EXISTS "${candidate}")
        continue()
    endif()

    file(RELATIVE_PATH relative_path "${KERA_SOURCE_DIR}" "${candidate}")
    string(REPLACE "\\" "/" normalized_path "${relative_path}")
    set(normalized_with_slashes "/${normalized_path}")

    set(is_excluded FALSE)
    foreach(pattern IN LISTS KERA_COPYRIGHT_EXCLUDED_PATH_PATTERNS)
        string(FIND "${normalized_with_slashes}" "${pattern}" found_exclusion)
        if(NOT found_exclusion EQUAL -1)
            set(is_excluded TRUE)
        endif()
    endforeach()

    if(is_excluded)
        continue()
    endif()

    get_filename_component(file_name "${candidate}" NAME)
    if(file_name STREQUAL "LICENSE" OR file_name STREQUAL "NOTICE")
        continue()
    endif()

    file(READ "${candidate}" content)
    string(SUBSTRING "${content}" 0 512 leading_content)

    string(FIND "${leading_content}" "${KERA_REQUIRED_COPYRIGHT}" copyright_found)
    string(FIND "${leading_content}" "${KERA_REQUIRED_SPDX}" spdx_found)

    if(copyright_found EQUAL -1 OR spdx_found EQUAL -1)
        list(APPEND KERA_COPYRIGHT_MISSING_FILES "${normalized_path}")
    endif()
endforeach()

if(KERA_COPYRIGHT_MISSING_FILES)
    list(JOIN KERA_COPYRIGHT_MISSING_FILES "\n  " missing_files)
    message(FATAL_ERROR "Kera-owned commentable files missing copyright/SPDX header:\n  ${missing_files}")
endif()

message(STATUS "Kera-owned commentable files contain copyright/SPDX headers.")
