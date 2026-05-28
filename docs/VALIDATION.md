<!--
Copyright 2026 Tomas Mikalauskas
SPDX-License-Identifier: Apache-2.0
-->

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

Kera uses Slang as the source of truth for shader-facing contracts. The shader CTest lane checks:

- shader source files, entry point names, and stage annotations
- Slang reflection JSON for representative sample shaders
- the C++ `SlangReflectionMetadata` parser for descriptor and vertex-input metadata

At runtime, graphics shader programs compile and reflect their Slang stages in one path. The renderer stores the merged
reflection metadata on the shader-program resource, then uses it when creating reflected graphics pipelines.

Reflection-driven pipeline creation keeps shader-owned data in the shader:

- descriptor names, binding numbers, descriptor kinds, and entry-point inputs come from Slang reflection
- C++ still owns host layout details such as vertex structs, binding slots, offsets, strides, and input rates
- `PipelineReflectionBuilder` connects those two sides and reports mismatches early

When a reflected pipeline omits manual descriptor set layouts, the Vulkan backend derives them from shader reflection.
Samples update descriptors by shader variable name through `updateDescriptors()`, and single-set reflected pipelines can
allocate and bind descriptor sets without repeating the reflected set index.
