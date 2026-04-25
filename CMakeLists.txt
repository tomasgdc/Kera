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

# Add GLM (header-only math library)
add_subdirectory(third_party/glm EXCLUDE_FROM_ALL)

# Add SDL3 submodule
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/SDL)

# Add pre-compiled Slang libraries
set(KERA_SLANG_TARGET "")
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
                IMPORTED_LOCATION "${SLANG_LIB_DIR}/slang.lib"
                IMPORTED_IMPLIB "${SLANG_LIB_DIR}/slang.lib"
            )
        else()
            set_target_properties(slang PROPERTIES
                IMPORTED_LOCATION "${SLANG_LIB_DIR}/libslang.so"
            )
        endif()
        
        set(KERA_SLANG_TARGET slang)
        message(STATUS "Using pre-compiled Slang from ${SLANG_LIB_DIR}")
    else()
        message(WARNING "Slang headers not found at ${SLANG_INCLUDE_DIR}. Slang support disabled.")
    endif()
endif()

# Function to compile Slang shaders to SPIR-V
function(compile_slang_shaders TARGET_NAME SHADER_DIR OUTPUT_DIR)
    if(NOT KERA_ENABLE_SLANG)
        return()
    endif()
    
    # Find slangc executable
    if(WIN32)
        set(SLANGC_EXECUTABLE "${SLANG_BIN_DIR}/slangc.exe")
    else()
        set(SLANGC_EXECUTABLE "${SLANG_BIN_DIR}/slangc")
    endif()
    
    if(NOT EXISTS "${SLANGC_EXECUTABLE}")
        message(WARNING "slangc executable not found at ${SLANGC_EXECUTABLE}. Shader compilation disabled.")
        return()
    endif()
    
    # Find all .slang files
    file(GLOB_RECURSE SLANG_FILES "${SHADER_DIR}/*.slang")
    
    foreach(SLANG_FILE ${SLANG_FILES})
        # Get relative path from shader directory
        file(RELATIVE_PATH REL_PATH "${SHADER_DIR}" "${SLANG_FILE}")
        
        # Replace .slang extension with .spv
        string(REPLACE ".slang" ".spv" SPV_REL_PATH "${REL_PATH}")
        set(SPV_FILE "${OUTPUT_DIR}/${SPV_REL_PATH}")
        
        # Determine shader stage from filename
        if("${REL_PATH}" MATCHES ".*vert.*" OR "${REL_PATH}" MATCHES ".*vertex.*")
            set(SHADER_STAGE "vertex")
        elseif("${REL_PATH}" MATCHES ".*frag.*" OR "${REL_PATH}" MATCHES ".*fragment.*")
            set(SHADER_STAGE "fragment")
        elseif("${REL_PATH}" MATCHES ".*comp.*" OR "${REL_PATH}" MATCHES ".*compute.*")
            set(SHADER_STAGE "compute")
        elseif("${REL_PATH}" MATCHES ".*geom.*" OR "${REL_PATH}" MATCHES ".*geometry.*")
            set(SHADER_STAGE "geometry")
        else()
            set(SHADER_STAGE "vertex")  # default
        endif()
        
        # Create output directory
        get_filename_component(SPV_DIR "${SPV_FILE}" DIRECTORY)
        file(MAKE_DIRECTORY "${SPV_DIR}")
        
        # Add custom command to compile shader
        add_custom_command(
            OUTPUT "${SPV_FILE}"
            COMMAND "${SLANGC_EXECUTABLE}" "${SLANG_FILE}" -o "${SPV_FILE}" -target spirv -stage ${SHADER_STAGE} -entry main
            DEPENDS "${SLANG_FILE}"
            COMMENT "Compiling ${REL_PATH} to SPIR-V (${SHADER_STAGE})"
            VERBATIM
        )
        
        # Collect output files for dependency
        list(APPEND SPV_OUTPUTS "${SPV_FILE}")
    endforeach()
    
    # Create a custom target that depends on all compiled shaders
    if(SPV_OUTPUTS)
        add_custom_target(${TARGET_NAME}_shaders DEPENDS ${SPV_OUTPUTS})
        add_dependencies(${TARGET_NAME} ${TARGET_NAME}_shaders)
    endif()
endfunction()

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
    src/renderer/buffer.cpp
    src/renderer/image.cpp
)

set(KERA_RENDERER_HEADERS
    include/kera/renderer/descriptors.h
    include/kera/renderer/factory.h
    include/kera/renderer/interfaces.h
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
    include/kera/renderer/buffer.h
    include/kera/renderer/image.h
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
)

# Link libraries
target_link_libraries(kera
    PUBLIC
        Vulkan::Vulkan
        SDL3::SDL3
        glm::glm
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
