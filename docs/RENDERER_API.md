<!--
Copyright 2026 Tomas Mikalauskas
SPDX-License-Identifier: Apache-2.0
-->

# Kera Renderer Public API

Kera's public renderer API is the STL-free surface exposed through `kera/kera.h` and
`kera/renderer/api.h`. It is designed for DLL/shared-library consumers: opaque handles, POD
descriptors, borrowed string views, pointer/count arrays, and an explicit `KeraRendererApiV1`
function table.

The implementation still uses richer C++ types internally, but those headers are not installed and
are not sample/user API.

## Public Boundary

- Include `kera/renderer/api.h` for renderer usage.
- Create a renderer through `KeraRendererCreateDesc` or the header-only `kera::Renderer` wrapper.
- The wrapper is STL-free and owns only `KeraRenderer*` plus `const KeraRendererApiV1*`.
- All strings are `KeraStringView`; callers own the pointed-to bytes for the duration of the call.
- Handles are opaque values and are released through the renderer.

## Shader Programs And Pipelines

The common Slang path uses a one-file vertex/fragment shader program:

```cpp
kera::Renderer renderer = kera::Renderer::create({
    .backend = kera::GraphicsBackend::Vulkan,
    .sdlWindow = sdlWindow,
    .width = width,
    .height = height,
});

auto program = renderer.createGraphicsShaderProgram({
    .path = kera::stringView("samples/shaders/triangle.slang"),
    .vertexEntryPoint = kera::stringView("vertexMain"),
    .fragmentEntryPoint = kera::stringView("fragmentMain"),
    .source = KERA_SHADER_SOURCE_SLANG_FILE,
});
```

Use `VertexInputLayoutBuilder` to declare the host vertex memory layout without STL containers.
Descriptors and shader entry points come from the Slang shader program reflection:

```cpp
const auto vertexInput =
    kera::VertexInputLayoutBuilder{}
        .vertexBinding<Vertex>(0)
        .field(KERA_VERTEX_FIELD(Vertex, position, 0, kera::VertexFormat::Float3))
        .field(KERA_VERTEX_FIELD(Vertex, color, 0, kera::VertexFormat::Float3))
        .layout();
```

Fields are matched against reflected Slang vertex input struct field names, not shader semantic
strings. `parameterName` is optional and only needed when two vertex entry parameters expose the
same field name. `createGraphicsPipeline()` validates the layout against the reflected vertex entry
point stored on the shader program, including missing fields, unknown fields, duplicate mappings,
wrong formats, zero strides, duplicate binding slots, and matrix expansion such as `float4x4` to
four `Float4` locations.

Use the preflight API when you want diagnostics before creating a pipeline:

```cpp
KeraRendererValidationReport report =
    renderer.validateVertexInputLayout(program, vertexInput);
```

Descriptor set layouts are derived from reflected descriptor names, kinds, sets, and bindings.

Descriptors are updated only by reflected shader variable name:

```cpp
renderer.updateDescriptors(descriptorSet)
    .uniform<Uniforms>("globalParams", uniformBuffer)
    .sampledImage("sceneTexture", texture)
    .sampler("sceneSampler", sampler);
```

## Resources And Frames

The public API covers the sample path:

- buffers, uniform ring buffers, textures, samplers, render targets.
- graphics pipelines and reflected descriptor-set allocation.
- descriptor updates by reflected shader variable name.
- frame begin/end, render-pass begin/end, binding, and indexed draws.
- resize, stats, UI forwarding, logging, validation reports, and debug names.
- DamagedHelmet glTF loading through an ABI-safe `KeraGltfLoadedModel`.

A typical frame:

```cpp
auto frame = renderer.beginFrame();
renderer.beginRenderPass(frame, {.clearColor = {0.02f, 0.02f, 0.03f, 1.0f}, .clearDepth = 1.0f});
renderer.bindPipeline(frame, pipeline);
renderer.bindVertexBuffer(frame, 0, vertexBuffer);
renderer.bindIndexBuffer(frame, indexBuffer, kera::IndexFormat::UInt16);
renderer.bindDescriptorSet(frame, pipeline, descriptorSet);
renderer.drawIndexed(frame, indexCount);
renderer.endRenderPass(frame);
renderer.endFrame(frame);
```

## Internal Headers

Headers such as `interfaces.h`, `factory.h`, `descriptors.h`, `result.h`,
`reflection_contracts.h`, `gltf_loader.h`, Slang helpers, Vulkan backend headers, utility headers,
and core SDL wrappers are implementation/internal headers. They may use STL and are not part of the
installed DLL-safe API.
