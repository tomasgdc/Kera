cmake_minimum_required(VERSION 3.25)

project(Kera VERSION 0.1.0 LANGUAGES CXX)

# Options
option(KERA_BUILD_SAMPLES "Build Kera samples" ON)
option(KERA_BUILD_SHARED "Build Kera as shared library" OFF)
option(KERA_ENABLE_SLANG "Enable Slang shader compilation support" ON)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find dependencies
if(NOT DEFINED ENV{VULKAN_SDK})
    set(ENV{VULKAN_SDK} "C:/VulkanSDK/1.4.341.1")
endif()
find_package(Vulkan REQUIRED)

# Add SDL3 submodule
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/SDL)

# Add Slang when available. This keeps configuration working before the
# submodule is fetched, while making the intended dependency explicit.
set(KERA_SLANG_TARGET "")
if(KERA_ENABLE_SLANG)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/slang/CMakeLists.txt")
        set(SLANG_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
        set(SLANG_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(SLANG_ENABLE_SLANGD OFF CACHE BOOL "" FORCE)
        set(SLANG_ENABLE_SLANGI OFF CACHE BOOL "" FORCE)
        set(SLANG_ENABLE_REPLAYER OFF CACHE BOOL "" FORCE)
        set(SLANG_ENABLE_DXIL OFF CACHE BOOL "" FORCE)
        set(SLANG_ENABLE_SLANG_RHI OFF CACHE BOOL "" FORCE)
        add_subdirectory(third_party/slang EXCLUDE_FROM_ALL)

        if(TARGET slang::slang)
            set(KERA_SLANG_TARGET slang::slang)
        elseif(TARGET slang)
            set(KERA_SLANG_TARGET slang)
        endif()
    else()
        message(STATUS "Slang submodule not found at third_party/slang. Run 'git submodule update --init --recursive'.")
    endif()
endif()

# === KERA CORE (SDL3 Platform Layer) ===
set(KERA_CORE_SOURCES
    src/core/window.cpp
    src/core/input.cpp
    src/core/audio.cpp
)

set(KERA_CORE_HEADERS
    include/kera/core/window.h
    include/kera/core/input.h
    include/kera/core/audio.h
)

# === KERA RENDERER (Cross-platform Vulkan) ===
set(KERA_RENDERER_SOURCES
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
    src/renderer/buffer.cpp
    src/renderer/image.cpp
)

set(KERA_RENDERER_HEADERS
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
    include/kera/renderer/buffer.h
    include/kera/renderer/image.h
)

# === KERA UTILITIES ===
set(KERA_UTILITIES_SOURCES
    src/utilities/logger.cpp
    src/utilities/validation.cpp
    src/utilities/file_utils.cpp
)

set(KERA_UTILITIES_HEADERS
    include/kera/utilities/logger.h
    include/kera/utilities/validation.h
    include/kera/utilities/file_utils.h
)

# === COMMON HEADERS ===
set(KERA_COMMON_HEADERS
    include/kera/kera.h
    include/kera/types.h
    include/kera/result.h
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
)

# Link libraries
target_link_libraries(kera
    PUBLIC
        Vulkan::Vulkan
        SDL3::SDL3
)

if(KERA_SLANG_TARGET)
    target_link_libraries(kera PUBLIC ${KERA_SLANG_TARGET})
    target_compile_definitions(kera PUBLIC KERA_HAS_SLANG=1)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(kera PRIVATE /W4 /WX)
else()
    target_compile_options(kera PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Samples
if(KERA_BUILD_SAMPLES)
    add_subdirectory(samples)
endif()

# Install rules
install(TARGETS kera
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/kera
    DESTINATION include
)
