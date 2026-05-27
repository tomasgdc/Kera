# Kera Validation

This document records the local validation lanes used while hardening the Vulkan renderer.

## Default Validation

Run the normal Windows debug build and CTest lane:

```powershell
cmake --build --preset windows-debug
ctest --test-dir build/windows-debug -C Debug --output-on-failure
```

The default CTest lane is safe for machines without a visible Vulkan device. GPU-backed sample smoke tests are
registered, but their launch script skips execution unless `KERA_RUN_GPU_SMOKE=1` is set.

## CTest Labels

List registered tests:

```powershell
ctest --test-dir build/windows-debug -C Debug -N
```

Run only fast host-side tests:

```powershell
ctest --test-dir build/windows-debug -C Debug -L quick --output-on-failure
```

Run shader contract tests:

```powershell
ctest --test-dir build/windows-debug -C Debug -L shader --output-on-failure
```

Run registered Vulkan sample smoke lanes without launching the GPU-backed sample:

```powershell
ctest --test-dir build/windows-debug -C Debug -L vulkan --output-on-failure
```

Run GPU-backed sample smoke lanes:

```powershell
$env:KERA_RUN_GPU_SMOKE = '1'
ctest --test-dir build/windows-debug -C Debug -L vulkan --output-on-failure
```

## GPU Smoke Logs

When `KERA_RUN_GPU_SMOKE=1` is set, sample smoke logs are written beside the sample executable:

- `build/windows-debug/samples/Debug/kera_validation_smoke.log`
- `build/windows-debug/samples/Debug/kera_resize_smoke.log`
- `build/windows-debug/samples/Debug/kera_many_lights_smoke.log`
- `build/windows-debug/samples/Debug/kera_depth_smoke.log`
- `build/windows-debug/samples/Debug/kera_many_lights_resize_smoke.log`
- `build/windows-debug/samples/Debug/kera_instanced_resize_smoke.log`
- `build/windows-debug/samples/Debug/kera_damaged_helmet_smoke.log`
- `build/windows-debug/samples/Debug/kera_damaged_helmet_zero_resize_smoke.log`
- `build/windows-debug/samples/Debug/kera_damaged_helmet_visual_regression_smoke.log`
- `build/windows-debug/samples/Debug/kera_damaged_helmet_back_visual_smoke.log`
- `build/windows-debug/samples/Debug/kera_damaged_helmet_gamma_audit_smoke.log`

## Frame Capture Lane

Use this lane when validation passes but rendered output, image layouts, descriptors, or pass ordering need visible
proof.

RenderDoc capture through the repo script:

```powershell
tools/CaptureKeraFrame.ps1 -Tool RenderDoc -SampleIndex 2 -Frames 2
```

Print the exact sample command without launching a capture tool:

```powershell
tools/CaptureKeraFrame.ps1 -Tool Print -SampleIndex 2 -Frames 2
```

Nsight Graphics launch sheet:

```powershell
tools/CaptureKeraFrame.ps1 -Tool Nsight -SampleIndex 2 -Frames 2
```

Expected first capture target:

- sample index `2` (`InstancedTriangleManyLightsSample`)
- geometry pass writes an offscreen render target
- geometry pass clears and uses a depth attachment
- lighting pass samples that texture through sampled-image and sampler descriptors
- validation output should contain no `[ERROR] Vulkan validation:` lines

## Shader Contract Lane

Kera uses Slang shader sources. The shader CTest lane validates source files, entry point names, stage annotations,
Slang reflection JSON, and the C++ `SlangReflectionMetadata` parser for representative descriptor and vertex-input
contracts. Runtime Slang shader-program creation also compiles and reflects Slang stages automatically, then stores
the merged metadata on the shader-program resource for renderer-side validation and future descriptor layout work.
The runtime path uses the Slang API directly: it keeps the linked-program session, module, entry point, and composite
component alive while calling `getLayout()` and `toJson()`, then parses the JSON into typed C++ metadata.
Graphics pipeline creation derives descriptor set layouts from shader-program reflection when samples leave descriptor
sets empty, so descriptor names, bindings, and descriptor kinds stay owned by the Slang source rather than duplicated
in sample code. C++ renderer convenience APIs cover the common sample path: `createGraphicsShaderProgram()` builds a
two-stage graphics program from one Slang file, `PipelineReflectionBuilder` declares C++ host vertex bindings and
required shader resources, and high-level `createGraphicsPipeline(GraphicsPipelineCreateDesc)` applies that contract
using the shader program's stored reflection metadata. The low-level `appendValidatedReflectedPipelineContract()` helper remains
available as an escape hatch, but samples no longer call it directly. `vertexBinding<T>()` derives binding stride from
the host type while offsets remain explicit, `debugName()` prefixes contract diagnostics, and
`getGraphicsPipelineDescriptorSets()` exposes pipeline descriptor layouts for tools. `updateDescriptors()` updates
descriptors by reflected shader variable name and returns an accumulated `ok()` result. The renderer helper appends
vertex bindings and reflected attributes while rejecting missing, ambiguous, duplicate, or unused semantic mappings.
The sample shaders avoid manual `register(...)` bindings where Slang reflection can supply layout data, and single-set
reflected pipelines can allocate and bind descriptor sets without repeating the reflected set index in sample code.
