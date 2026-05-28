# Copyright 2026 Tomas Mikalauskas
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.25)

project(Kera VERSION 0.1.0 LANGUAGES CXX)

# Options
option(KERA_BUILD_SAMPLES "Build Kera samples" ON)
option(KERA_BUILD_SHARED "Build Kera as shared library" ON)
option(KERA_BUILD_TESTS "Build Kera tests" ON)
option(KERA_ENABLE_SLANG "Enable Slang shader compilation support" ON)
option(KERA_ENABLE_IMGUI "Enable Dear ImGui debug UI support" ${KERA_BUILD_SAMPLES})

include(CTest)

function(kera_configure_no_exceptions target_name)
    if(MSVC)
        target_compile_options(${target_name} PUBLIC /EHs-c-)
        target_compile_definitions(${target_name} PUBLIC _HAS_EXCEPTIONS=0)
    else()
        target_compile_options(${target_name} PUBLIC -fno-exceptions)
    endif()
endfunction()

if(MSVC)
    foreach(flag_var
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS_MINSIZEREL
    )
        string(REPLACE "/EHsc" "" ${flag_var} "${${flag_var}}")
        string(REPLACE "/EHsc-" "" ${flag_var} "${${flag_var}}")
        set(${flag_var} "${${flag_var}}" CACHE STRING "" FORCE)
    endforeach()
endif()

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find dependencies
if(NOT DEFINED ENV{VULKAN_SDK})
    set(ENV{VULKAN_SDK} "C:/VulkanSDK/1.4.341.1")
endif()
find_package(Vulkan REQUIRED)

# Add GLM (header-only math library)
add_subdirectory(third_party/glm EXCLUDE_FROM_ALL)

# Add SDL3 submodule
if(KERA_BUILD_SHARED)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(KERA_SDL_TARGET SDL3::SDL3-shared)
else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(KERA_SDL_TARGET SDL3::SDL3-static)
endif()
add_subdirectory(third_party/SDL)

if(NOT TARGET ${KERA_SDL_TARGET})
    message(FATAL_ERROR "Expected SDL target ${KERA_SDL_TARGET} was not created")
endif()

# Add spdlog submodule
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE_HO OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS_HO OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
set(SPDLOG_NO_EXCEPTIONS ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/spdlog EXCLUDE_FROM_ALL)

if(KERA_ENABLE_IMGUI)
    set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
    if(NOT EXISTS "${IMGUI_DIR}/imgui.cpp")
        message(FATAL_ERROR "KERA_ENABLE_IMGUI is ON but third_party/imgui is missing")
    endif()

    if(KERA_BUILD_SHARED)
        add_library(kera_imgui SHARED
            "${IMGUI_DIR}/imgui.cpp"
            "${IMGUI_DIR}/imgui_draw.cpp"
            "${IMGUI_DIR}/imgui_tables.cpp"
            "${IMGUI_DIR}/imgui_widgets.cpp"
            "${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
            "${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp"
        )
        if(WIN32)
            set_target_properties(kera_imgui PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
            target_compile_definitions(kera_imgui
                PRIVATE "IMGUI_API=__declspec(dllexport)"
                INTERFACE "IMGUI_API=__declspec(dllimport)"
            )
        endif()
    else()
        add_library(kera_imgui STATIC
            "${IMGUI_DIR}/imgui.cpp"
            "${IMGUI_DIR}/imgui_draw.cpp"
            "${IMGUI_DIR}/imgui_tables.cpp"
            "${IMGUI_DIR}/imgui_widgets.cpp"
            "${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
            "${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp"
        )
    endif()
    set_target_properties(kera_imgui PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_include_directories(kera_imgui PUBLIC
        "${IMGUI_DIR}"
        "${IMGUI_DIR}/backends"
    )
    target_link_libraries(kera_imgui PUBLIC
        Vulkan::Vulkan
        ${KERA_SDL_TARGET}
    )
    kera_configure_no_exceptions(kera_imgui)
endif()

# Add pre-compiled Slang libraries
set(KERA_SLANG_TARGET "")
set(KERA_SLANGC_EXECUTABLE "")
if(KERA_ENABLE_SLANG)
    set(SLANG_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/slang/include")
    
    if(WIN32)
        set(SLANG_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/slang/lib/windows")
        set(SLANG_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/slang/bin/windows")
        set(SLANG_LIBS 
            "${SLANG_LIB_DIR}/slang.lib"
            "${SLANG_LIB_DIR}/slang-compiler.lib"
            "${SLANG_LIB_DIR}/slang-rt.lib"
            "${SLANG_LIB_DIR}/gfx.lib"
        )
    else()  # Linux/Unix
        set(SLANG_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/slang/lib/linux")
        set(SLANG_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/slang/bin/linux")
        set(SLANG_LIBS 
            "${SLANG_LIB_DIR}/libslang.so"
            "${SLANG_LIB_DIR}/libslang-compiler.so"
            "${SLANG_LIB_DIR}/libslang-rt.so"
            "${SLANG_LIB_DIR}/libgfx.so"
        )
    endif()
    
    if(EXISTS "${SLANG_INCLUDE_DIR}/slang.h")
        # Create imported target for Slang
        add_library(slang SHARED IMPORTED)
        
        set_target_properties(slang PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${SLANG_INCLUDE_DIR}"
        )
        
        if(WIN32)
            set_target_properties(slang PROPERTIES
                IMPORTED_LOCATION "${SLANG_BIN_DIR}/slang.dll"
                IMPORTED_IMPLIB "${SLANG_LIB_DIR}/slang.lib"
            )
        else()
            set_target_properties(slang PROPERTIES
                IMPORTED_LOCATION "${SLANG_LIB_DIR}/libslang.so"
            )
        endif()
        
        set(KERA_SLANG_TARGET slang)
        if(WIN32)
            set(KERA_SLANGC_EXECUTABLE "${SLANG_BIN_DIR}/slangc.exe")
        else()
            set(KERA_SLANGC_EXECUTABLE "${SLANG_BIN_DIR}/slangc")
        endif()
        if(WIN32)
            file(GLOB KERA_SLANG_RUNTIME_DLLS CONFIGURE_DEPENDS "${SLANG_BIN_DIR}/*.dll")
        endif()
        message(STATUS "Using pre-compiled Slang from ${SLANG_LIB_DIR}")
    else()
        message(WARNING "Slang headers not found at ${SLANG_INCLUDE_DIR}. Slang support disabled.")
    endif()
endif()

# === KERA CORE (SDL3 Platform Layer) ===
set(KERA_CORE_SOURCES
    src/core/window.cpp
    src/core/input.cpp
    src/core/audio.cpp
    src/core/sdl_input_utils.cpp
)

set(KERA_CORE_HEADERS
    include/kera/core/window.h
    include/kera/core/input.h
    include/kera/core/audio.h
)

# === KERA RENDERER (Cross-platform Vulkan) ===
set(KERA_RENDERER_SOURCES
    src/renderer/api.cpp
    src/renderer/factory.cpp
    src/renderer/vulkan_renderer.cpp
    src/renderer/instance.cpp
    src/renderer/device.cpp
    src/renderer/physical_device.cpp
    src/renderer/surface.cpp
    src/renderer/swapchain.cpp
    src/renderer/command_buffer.cpp
    src/renderer/pipeline.cpp
    src/renderer/render_pass.cpp
    src/renderer/framebuffer.cpp
    src/renderer/shader.cpp
    src/renderer/slang_compiler.cpp
    src/renderer/slang_reflection.cpp
    src/renderer/reflection_contracts.cpp
    src/renderer/gltf_loader.cpp
    src/renderer/buffer.cpp
    src/renderer/image.cpp
)

set(KERA_RENDERER_PUBLIC_HEADERS
    include/kera/renderer/api.h
    include/kera/renderer/abi.h
)

set(KERA_RENDERER_INTERNAL_HEADERS
    include/kera/renderer/descriptors.h
    include/kera/renderer/descriptor_contracts.h
    include/kera/renderer/factory.h
    include/kera/renderer/interfaces.h
    include/kera/renderer/resource_registry.h
    include/kera/renderer/backend/vulkan/layout_utils.h
    include/kera/renderer/backend/vulkan/vulkan_renderer.h
    include/kera/renderer/vulkan_renderer.h
    include/kera/renderer/instance.h
    include/kera/renderer/device.h
    include/kera/renderer/physical_device.h
    include/kera/renderer/surface.h
    include/kera/renderer/swapchain.h
    include/kera/renderer/command_buffer.h
    include/kera/renderer/pipeline.h
    include/kera/renderer/render_pass.h
    include/kera/renderer/framebuffer.h
    include/kera/renderer/shader.h
    include/kera/renderer/slang_compiler.h
    include/kera/renderer/slang_reflection.h
    include/kera/renderer/reflection_contracts.h
    include/kera/renderer/gltf_loader.h
    include/kera/renderer/result.h
    include/kera/renderer/buffer.h
    include/kera/renderer/image.h
)

set(KERA_PUBLIC_API_HEADERS
    include/kera/kera.h
    include/kera/renderer/api.h
    include/kera/renderer/abi.h
)

set(KERA_RENDERER_HEADERS
    ${KERA_RENDERER_PUBLIC_HEADERS}
    ${KERA_RENDERER_INTERNAL_HEADERS}
)

# === KERA UTILITIES ===
set(KERA_UTILITIES_SOURCES
    src/utilities/logger.cpp
    src/utilities/validation.cpp
    src/utilities/file_utils.cpp
    src/utilities/json.cpp
)

set(KERA_UTILITIES_HEADERS
    include/kera/utilities/logger.h
    include/kera/utilities/validation.h
    include/kera/utilities/file_utils.h
    include/kera/utilities/json.h
)

# === COMMON HEADERS ===
set(KERA_COMMON_HEADERS
    include/kera/kera.h
    include/kera/types.h
)

# Combine all sources
set(KERA_ALL_SOURCES
    ${KERA_CORE_SOURCES}
    ${KERA_RENDERER_SOURCES}
    ${KERA_UTILITIES_SOURCES}
)

set(KERA_ALL_HEADERS
    ${KERA_CORE_HEADERS}
    ${KERA_RENDERER_HEADERS}
    ${KERA_UTILITIES_HEADERS}
    ${KERA_COMMON_HEADERS}
)

# Create library
if(KERA_BUILD_SHARED)
    add_library(kera SHARED ${KERA_ALL_SOURCES} ${KERA_ALL_HEADERS})
    target_compile_definitions(kera PRIVATE KERA_EXPORT)
    target_compile_definitions(kera INTERFACE KERA_IMPORT)
else()
    add_library(kera STATIC ${KERA_ALL_SOURCES} ${KERA_ALL_HEADERS})
endif()

target_include_directories(kera
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinygltf
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include
)

target_compile_definitions(kera PRIVATE
    TINYGLTF_NOEXCEPTION
    JSON_NOEXCEPTION
)

# Link libraries
target_link_libraries(kera
    PUBLIC
        Vulkan::Vulkan
        ${KERA_SDL_TARGET}
        glm::glm
    PRIVATE
        spdlog::spdlog
)

if(KERA_SLANG_TARGET)
    target_link_libraries(kera PUBLIC ${KERA_SLANG_TARGET})
    target_compile_definitions(kera PUBLIC KERA_HAS_SLANG=1)
endif()

if(TARGET kera_imgui)
    target_link_libraries(kera PUBLIC kera_imgui)
    target_compile_definitions(kera PUBLIC KERA_HAS_IMGUI=1)
endif()

kera_configure_no_exceptions(kera)

# Compiler warnings
if(MSVC)
    target_compile_options(kera PRIVATE /W4 /WX)
else()
    target_compile_options(kera PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(BUILD_TESTING AND KERA_BUILD_TESTS AND KERA_BUILD_SHARED)
    add_library(kera_internal_tests STATIC ${KERA_ALL_SOURCES} ${KERA_ALL_HEADERS})

    target_include_directories(kera_internal_tests
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinygltf
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include
    )
    target_compile_definitions(kera_internal_tests PRIVATE
        TINYGLTF_NOEXCEPTION
        JSON_NOEXCEPTION
    )
    target_link_libraries(kera_internal_tests
        PUBLIC
            Vulkan::Vulkan
            ${KERA_SDL_TARGET}
            glm::glm
        PRIVATE
            spdlog::spdlog
    )
    if(KERA_SLANG_TARGET)
        target_link_libraries(kera_internal_tests PUBLIC ${KERA_SLANG_TARGET})
        target_compile_definitions(kera_internal_tests PUBLIC KERA_HAS_SLANG=1)
    endif()
    if(TARGET kera_imgui)
        target_link_libraries(kera_internal_tests PUBLIC kera_imgui)
        target_compile_definitions(kera_internal_tests PUBLIC KERA_HAS_IMGUI=1)
    endif()
    kera_configure_no_exceptions(kera_internal_tests)
    if(MSVC)
        target_compile_options(kera_internal_tests PRIVATE /W4 /WX)
    else()
        target_compile_options(kera_internal_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endif()

# Samples
if(KERA_BUILD_SAMPLES)
    add_subdirectory(samples)
endif()

# Tests
if(BUILD_TESTING AND KERA_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Install rules
install(TARGETS kera
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES include/kera/kera.h
    DESTINATION include/kera
)

install(FILES
    ${KERA_RENDERER_PUBLIC_HEADERS}
    DESTINATION include/kera/renderer
)
