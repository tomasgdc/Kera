# Copyright 2026 Tomas Mikalauskas
# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED KERA_SPONZA_ASSET_DIR)
    message(FATAL_ERROR "KERA_SPONZA_ASSET_DIR is required")
endif()

set(sponza_gltf_dir "${KERA_SPONZA_ASSET_DIR}/glTF")
foreach(required_file
        "${KERA_SPONZA_ASSET_DIR}/LICENSE.md"
        "${KERA_SPONZA_ASSET_DIR}/README.md"
        "${KERA_SPONZA_ASSET_DIR}/metadata.json"
        "${sponza_gltf_dir}/Sponza.gltf"
        "${sponza_gltf_dir}/Sponza.bin")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Missing packaged Sponza asset file: ${required_file}")
    endif()
endforeach()

file(GLOB sponza_runtime_files "${sponza_gltf_dir}/*")
list(LENGTH sponza_runtime_files sponza_runtime_file_count)
if(NOT sponza_runtime_file_count EQUAL 71)
    message(FATAL_ERROR "Expected 71 packaged Sponza glTF files, found ${sponza_runtime_file_count}")
endif()

file(SHA256 "${sponza_gltf_dir}/Sponza.gltf" sponza_gltf_sha256)
string(TOUPPER "${sponza_gltf_sha256}" sponza_gltf_sha256)
if(NOT sponza_gltf_sha256 STREQUAL "646C10CBC8FAB990CA29F363E90E2D65155F3A3569506852EB1434A9465B9501")
    message(FATAL_ERROR "Packaged Sponza.gltf hash does not match the audited upstream asset")
endif()

file(SHA256 "${sponza_gltf_dir}/Sponza.bin" sponza_bin_sha256)
string(TOUPPER "${sponza_bin_sha256}" sponza_bin_sha256)
if(NOT sponza_bin_sha256 STREQUAL "FDBDBFB6A76EDEB6626F28A1401BC1536BB1C864131A64E90FBC3DF2D2D191BD")
    message(FATAL_ERROR "Packaged Sponza.bin hash does not match the audited upstream asset")
endif()

message(STATUS "Packaged Sponza asset verified: ${sponza_runtime_file_count} glTF files.")