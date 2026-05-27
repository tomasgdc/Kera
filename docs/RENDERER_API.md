# Kera Renderer C++ Convenience API

This document describes Kera's current C++ renderer convenience API used by Kera-owned code and samples. These headers
live under `include/kera/renderer/` and may expose STL types such as `std::string` and `std::vector`.

The installed versioned C ABI surface is separate and intentionally STL-free: `kera/kera.h`, `kera/renderer/api.h`, and
`kera/renderer/abi.h`. See [Renderer ABI Policy](RENDERER_ABI.md) before exposing Kera across shared-library or
compiler boundaries.

## Ownership Model

- Applications own CPU data layout: vertex structs, buffer slots, offsets, strides, instance rates, resources, and render
  target lifetime.
- Slang owns shader-facing contracts: entry point inputs, semantics, descriptor names, descriptor kinds, and descriptor
  binding numbers.
- The renderer validates the bridge between application-owned C++ layout and reflected Slang metadata before reflected
  pipeline creation succeeds.
- Runtime failures use invalid handles, `false`, `RendererResult<T>`, or validation reports. Kera is built with C++
  exceptions disabled.

## Shader Programs And Reflection

Use `createGraphicsShaderProgram()` for the common one-file vertex/fragment Slang path:

```cpp
ShaderProgramHandle program = renderer.createGraphicsShaderProgram({
    .path = resolveShaderPath("shaders/triangle.slang"),
    .vertexEntryPoint = "vertexMain",
    .fragmentEntryPoint = "fragmentMain",
    .debugName = "Triangle Shader Program",
});
```

The lower-level `createShaderProgram()` remains available for custom stage lists. Applications that prefer typed failure
objects can use `tryCreateGraphicsShaderProgram()`:

```cpp
RendererResult<ShaderProgramHandle> program = renderer.tryCreateGraphicsShaderProgram({
    .path = resolveShaderPath("shaders/triangle.slang"),
});
if (!program)
{
    log(program.errorMessage());
}
```

`RendererResult<T>` provides `ok()`, `operator bool()`, `value()`, `valuePtr()`, `valueOr()`, `error()`,
`errorMessage()`, and `errorCode()`. `RendererResult<void>` provides the same status/error accessors without a value.

## Reflection Contracts And Pipelines

Use `PipelineReflectionBuilder` to declare how reflected shader inputs map onto application-owned buffers and resources:

```cpp
const PipelineReflectionContract contract =
    PipelineReflectionBuilder{}
        .debugName("Triangle Pipeline")
        .vertexEntry("vertexMain")
        .vertexBinding<Vertex>("meshVertex", 0)
        .semantic("POSITION", "meshVertex", offsetof(Vertex, position), VertexFormat::Float3)
        .semantic("COLOR", "meshVertex", offsetof(Vertex, color), VertexFormat::Float3)
        .uniform<Uniforms>("globalParams")
        .build();
```

`vertexBinding<T>()` uses `sizeof(T)` for binding stride. Offsets remain explicit because the application owns host
memory layout.

Create reflected graphics pipelines with `GraphicsPipelineCreateDesc`:

```cpp
GraphicsPipelineHandle pipeline = renderer.createGraphicsPipeline({
    .shaderProgram = program,
    .reflectionContract = contract,
    .cullMode = CullModeKind::None,
    .debugName = "Triangle Pipeline",
});
```

The renderer reads shader-program reflection, appends reflected vertex attributes, validates descriptor names and uniform
sizes, and derives descriptor set layouts when the descriptor layout list is empty. The lower-level
`createGraphicsPipeline(GraphicsPipelineDesc, ShaderProgramHandle)` and `appendValidatedReflectedPipelineContract()`
APIs remain available for advanced or manual layout paths.

## Descriptor Updates And Validation

Use `updateDescriptors()` to update descriptors by reflected shader variable name:

```cpp
const bool updated = renderer.updateDescriptors(descriptorSet)
    .uniform<Uniforms>("globalParams", uniformBuffer)
    .sampledImage("sceneTexture", sceneTexture)
    .sampler("sceneSampler", sceneSampler)
    .ok();
```

`DescriptorSetWriter::ok()` reports the accumulated result. `errorMessage()`, `report()`, and `result()` expose the
first failure and a structured `RendererValidationReport`. Lower-level `updateDescriptorSet()` overloads remain
available by numeric binding or reflected name.

`validateDescriptorSet()`, `validateGraphicsPipelineDescriptorSets()`, `validateDescriptorSetDetailed()`, and
`validateGraphicsPipelineDescriptorSetsDetailed()` check descriptor/resource compatibility for reflected pipelines.

## Resources

- `BufferDesc` creates vertex, index, uniform, or storage buffers with GPU-only or CPU-write memory access.
- `createUniformRingBuffer()`, `uploadUniformRingBuffer()`, and `getUniformRingBufferOffset()` support per-frame uniform
  updates without reusing the same byte range across in-flight frames.
- `TextureDesc` supports `TextureFormat::RGBA8`, `TextureFormat::RGBA8Srgb`, and `TextureFormat::Depth32`.
- `TextureDesc::mipLevels` and `TextureDesc::generateMipmaps` request explicit mip counts or full-chain generation from
  uploaded level 0 data when supported by the backend.
- `SamplerDesc::mipFilter`, `minLod`, `maxLod`, and `maxAnisotropy` expose mip filtering, LOD range, and anisotropy.
  The Vulkan backend clamps anisotropy to device limits.
- `RenderTargetDesc` accepts a color texture and optional depth texture.
- Resource descriptors include `debugName`; `setDebugName()` can assign debug names to buffers, textures, samplers,
  graphics pipelines, and descriptor sets after creation.

Destroy resources through the renderer. Descriptor sets should be destroyed before resources they reference, and
graphics pipelines should be destroyed before shader programs they reference.

## glTF Loader Convenience API

`loadGltfModel(IRenderer&, const GltfLoadDesc&)` is a renderer-owned C++ convenience helper for sample/application code:

```cpp
RendererResult<GltfLoadedModel> model = loadGltfModel(renderer, {
    .path = "C:/assets/DamagedHelmet/DamagedHelmet.gltf",
    .debugName = "DamagedHelmet",
});
```

Kera samples resolve that path with sample-private asset helpers; applications should pass their own resolved asset path.

Current scope is intentionally narrow:

- External `.gltf` files with external buffers/images.
- One mesh primitive with `POSITION`, `NORMAL`, `TEXCOORD_0`, indexed triangles, and generated tangent data in
  `GltfVertex`.
- Base color and emissive textures are loaded as sRGB; metallic-roughness, occlusion, and normal textures are loaded as
  linear RGBA8.
- `GltfLoadedModel` returns vertex/index buffers, material texture handles, one material sampler, index format/count,
  texture names, transform, and material factors including alpha mode, alpha cutoff, double-sided, normal scale,
  occlusion strength, base color, emissive, metallic, and roughness.

Release all model-owned renderer resources with `destroyGltfModel(renderer, model)`. The loader does not create shaders,
pipelines, descriptor sets, cameras, scene graphs, animations, skinning, or material systems.

## Frame Lifecycle

A typical frame:

```cpp
FrameHandle frame = renderer.beginFrame();
renderer.beginRenderPass(frame, {
    .clearColor = {0.02f, 0.02f, 0.03f, 1.0f},
});
renderer.bindPipeline(frame, pipeline);
renderer.bindVertexBuffer(frame, 0, vertexBuffer);
renderer.bindIndexBuffer(frame, indexBuffer, IndexFormat::UInt16);
renderer.bindDescriptorSet(frame, pipeline, descriptorSet);
renderer.drawIndexed(frame, indexCount);
renderer.endRenderPass(frame);
renderer.endFrame(frame);
```

Use `beginRenderPass(frame, renderTarget, desc)` for offscreen render targets. `resize(Extent2D)` recreates
swapchain-dependent backend state; renderer-owned buffers and shader programs remain valid, and live graphics pipelines
are recreated from stored metadata where possible. Zero-size extents are treated as a paused/minimized surface state.

## Introspection

Tools and applications can inspect shader reflection metadata and derived graphics-pipeline state:

```cpp
std::vector<SlangReflectionEntryPoint> entries = renderer.getShaderProgramEntryPoints(program);
std::vector<SlangReflectionBinding> bindings = renderer.getShaderProgramDescriptorBindings(program);
std::vector<SlangReflectionInput> inputs = renderer.getShaderProgramVertexInputs(program, "vertexMain");
std::vector<DescriptorSetLayoutDesc> layouts = renderer.getGraphicsPipelineDescriptorSets(pipeline);
VertexLayoutDesc vertexLayout = renderer.getGraphicsPipelineVertexLayout(pipeline);
PipelineReflectionContract contract = renderer.getGraphicsPipelineReflectionContract(pipeline);
```

This is useful for debug UI, validation reports, renderer tooling, and sample diagnostics.
