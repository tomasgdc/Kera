# Kera - Cross-Platform Rendering Library

Kera is a cross-platform rendering library built on Vulkan and SDL3, designed for high-performance graphics applications.

## Features

- **Vulkan-based rendering**: Modern, cross-platform graphics API
- **SDL3 integration**: Window management, input handling, and platform abstraction
- **Slang shader language**: Cross-platform shading with HLSL-like syntax
- **Modular architecture**: Clean separation between core systems and rendering
- **Sample applications**: Interactive demos showcasing different rendering techniques

## Building

### Prerequisites

- CMake 3.25 or later
- C++20 compatible compiler
- Vulkan SDK
- Git (for submodules)

### Clone with Submodules

```bash
git clone --recursive https://github.com/your-repo/kera.git
cd kera
```

If you already cloned without `--recursive`, initialize submodules:

```bash
git submodule update --init --recursive
```

### Build

```bash
# Configure
cmake --preset windows-debug  # or linux-debug

# Build
cmake --build --preset windows-debug  # or linux-debug

# Run samples
./build/windows-debug/samples/kera_samples
```

## Project Structure

```
├── include/kera/          # Public headers
├── src/                   # Source files
│   ├── core/             # SDL3 platform layer
│   ├── renderer/         # Vulkan rendering
│   └── utilities/        # Helper utilities
├── samples/              # Sample applications
├── third_party/          # External dependencies
│   └── SDL/             # SDL3 submodule
└── CMakeLists.txt       # Build configuration
```

## Architecture

- **Core Layer**: SDL3-based window, input, and platform abstraction
- **Renderer Layer**: Vulkan implementation with shader management
- **Utilities**: Logging, file I/O, validation, and common helpers
- **Samples**: Demonstrations of rendering techniques and API usage

## Dependencies

- **SDL3**: Window management and input (included as submodule)
- **Vulkan**: Graphics API (requires Vulkan SDK)
- **Slang**: Shader compilation from `.slang` source at startup

## Shader Compilation

Kera is set up to compile `.slang` shaders to SPIR-V at application startup.

- Add shader source files to your project, for example `basic_triangle.slang`
- Call `Shader::initializeFromSlangFile(...)` with the shader path and entry point name
- Kera will compile the Slang source in process and create the Vulkan shader module from the generated SPIR-V

## License

See LICENSE file for details.
