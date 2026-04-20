cmake_minimum_required(VERSION 3.25)

project(Kera VERSION 0.1.0 LANGUAGES CXX)

# Options
option(KERA_BUILD_SAMPLES "Build Kera samples" ON)
option(KERA_BUILD_SHARED "Build Kera as shared library" OFF)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find dependencies
find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED)

# === KERA CORE (SDL Platform Layer) ===
set(KERA_CORE_SOURCES
    src/core/window.cpp
    src/core/surface.cpp
    src/core/input.cpp
    src/core/audio.cpp
)

set(KERA_CORE_HEADERS
    include/kera/core/window.h
    include/kera/core/surface.h
    include/kera/core/input.h
    include/kera/core/audio.h
)

# === KERA RENDERER (Cross-platform Vulkan) ===
set(KERA_RENDERER_SOURCES
    src/renderer/instance.cpp
    src/renderer/device.cpp
    src/renderer/physical_device.cpp
    src/renderer/swapchain.cpp
    src/renderer/command_buffer.cpp
    src/renderer/pipeline.cpp
    src/renderer/render_pass.cpp
    src/renderer/framebuffer.cpp
    src/renderer/shader.cpp
    src/renderer/buffer.cpp
    src/renderer/image.cpp
)

set(KERA_RENDERER_HEADERS
    include/kera/renderer/instance.h
    include/kera/renderer/device.h
    include/kera/renderer/physical_device.h
    include/kera/renderer/swapchain.h
    include/kera/renderer/command_buffer.h
    include/kera/renderer/pipeline.h
    include/kera/renderer/render_pass.h
    include/kera/renderer/framebuffer.h
    include/kera/renderer/shader.h
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
    include/kera/utilities/result.h
    include/kera/utilities/file_utils.h
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
)

# Link libraries
target_link_libraries(kera
    PUBLIC
        Vulkan::Vulkan
    PRIVATE
        SDL2::SDL2
)

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
    EXPORT KeraTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/kera
    DESTINATION include
)

install(EXPORT KeraTargets
    FILE KeraTargets.cmake
    NAMESPACE Kera::
    DESTINATION lib/cmake/Kera
)