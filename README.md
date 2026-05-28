<!--
Copyright 2026 Tomas Mikalauskas
SPDX-License-Identifier: Apache-2.0
-->

# Kera - Cross-Platform Rendering Library

Kera is a Vulkan-first rendering library built with SDL3, Slang, and CMake for high-performance graphics applications.

## Features

- **Vulkan renderer**: Vulkan 1.3+ rendering backend with swapchain, render target, descriptor, synchronization, and validation hardening.
- **SDL3 integration**: Window management, input handling, and platform abstraction.
- **Slang shaders**: Runtime Slang compilation and reflection for graphics shader programs.
- **Reflection-driven pipelines**: C++ vertex/layout contracts are validated against Slang entry points and descriptor names.
- **glTF texture loading sample**: DamagedHelmet demonstrates renderer-owned glTF asset loading, texture upload, mipmaps, and sampler fidelity.
- **Sample application**: Basic Triangle, Instanced Triangle, Many Lights, and DamagedHelmet renderer demonstrations.
- **No-exception build**: Kera targets no-exception C++ and routes diagnostics through the Kera logger facade.

## Documentation

- [Renderer API](docs/RENDERER_API.md): STL-free public renderer API, Slang reflection, descriptors, and frame flow.
- [Renderer ABI Policy](docs/RENDERER_ABI.md): installed public ABI headers and shared-build guardrails.
- [Validation](docs/VALIDATION.md): CTest labels, Vulkan smoke tests, shader contract checks, and capture lanes.
- [DamagedHelmet Attribution](samples/assets/gltf/DamagedHelmet/ATTRIBUTION.md): source and license note for the bundled glTF sample asset.

## Building

### Prerequisites

- CMake 3.25 or later
- C++20 compatible compiler
- Vulkan SDK
- Git for submodules

### Clone With Submodules

```bash
git clone --recursive https://github.com/tomasgdc/Kera.git
cd Kera
```

If you already cloned without `--recursive`, initialize submodules:

```bash
git submodule update --init --recursive
```

### Configure And Build

```bash
# Configure
cmake --preset windows-debug

# Build
cmake --build --preset windows-debug
```

Default presets build Kera as a shared library so samples exercise the same STL-free DLL boundary
external users consume. Static build presets remain available with the `-static` suffix. See
[Renderer ABI Policy](docs/RENDERER_ABI.md) for the public ABI rules. Other configured presets are
listed in `CMakePresets.json`.

### Run Samples

With the Visual Studio generator on Windows, the debug sample executable is:

```powershell
build/windows-debug/samples/Debug/kera_samples.exe
```

Useful sample arguments:

```powershell
build/windows-debug/samples/Debug/kera_samples.exe --sample-index 3
build/windows-debug/samples/Debug/kera_samples.exe --help
```

Sample indices:

- `0`: Basic Triangle
- `1`: Instanced Triangle
- `2`: Instanced Triangle Many Lights
- `3`: DamagedHelmet Texture Loading

## Validation

```powershell
ctest --test-dir build/windows-debug -C Debug --output-on-failure
```

GPU-backed Vulkan smoke tests are registered with CTest but skip by default. Set `KERA_RUN_GPU_SMOKE=1` to launch
them. See [Validation](docs/VALIDATION.md) for labels, shader contract checks, smoke log paths, and capture commands.

## Project Structure

```text
include/kera/          Public STL-free ABI entry headers plus private build-tree headers
src/                   Source files
  core/                SDL3 platform layer
  renderer/            Vulkan renderer and renderer-owned asset helpers
  utilities/           Logging, file I/O, validation, and common helpers
samples/               Sample application, shaders, and sample assets
docs/                  Renderer API, ABI, and validation documentation
third_party/           Vendored dependencies
CMakeLists.txt         Build configuration
```

## Renderer API Shape

The common Slang graphics path uses the STL-free public renderer wrapper in `kera/renderer/api.h`:

```cpp
ShaderProgramHandle program = renderer.createGraphicsShaderProgram({
    .path = kera::stringView("samples/shaders/triangle.slang"),
    .vertexEntryPoint = kera::stringView("vertexMain"),
    .fragmentEntryPoint = kera::stringView("fragmentMain"),
    .source = KERA_SHADER_SOURCE_SLANG_FILE,
});

const PipelineReflectionContract contract =
    PipelineReflectionBuilder{}
        .debugName("Triangle Pipeline")
        .vertexEntry("vertexMain")
        .vertexBinding<Vertex>("meshVertex", 0)
        .semantic("POSITION", "meshVertex", offsetof(Vertex, position), VertexFormat::Float3)
        .semantic("COLOR", "meshVertex", offsetof(Vertex, color), VertexFormat::Float3)
        .build();

GraphicsPipelineHandle pipeline = renderer.createGraphicsPipeline({
    .shaderProgram = program,
    .reflectionContract = contract,
});
```

Descriptor updates use reflected shader variable names:

```cpp
const bool ok = renderer.updateDescriptors(descriptorSet)
    .uniform<Uniforms>("globalParams", uniformBuffer)
    .sampledImage("sceneTexture", sceneTexture)
    .sampler("sceneSampler", sceneSampler)
    .ok();
```

Samples use this same public API boundary. Internal renderer headers may still use STL, but they are not the installed
public API. See [Renderer API](docs/RENDERER_API.md) for the current flow.

## Dependencies

- **Vulkan SDK**: graphics API, validation layers, and shader tooling.
- **SDL3**: window management and input.
- **GLM**: math types.
- **Slang**: shader compilation and reflection.
- **Dear ImGui**: optional sample stats/debug overlay.
- **spdlog**: private implementation behind Kera's logger facade.
- **tinygltf/stb/nlohmann-json**: private renderer implementation dependencies for glTF loading.

## License

Kera-owned source code and documentation are licensed under the [Apache License 2.0](LICENSE). See [NOTICE](NOTICE) for
project attribution and distribution notes.

Third-party dependencies and bundled sample assets retain their own licenses. The bundled DamagedHelmet sample asset
remains Creative Commons Attribution-NonCommercial and is not relicensed by Kera; see
[DamagedHelmet Attribution](samples/assets/gltf/DamagedHelmet/ATTRIBUTION.md).
