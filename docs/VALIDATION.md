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

## GitHub Actions

The Windows build workflow runs two validation lanes on the self-hosted Windows runner:

- `Windows debug validation` builds `kera_unit_tests` and runs
  `ctest --test-dir build/windows-debug -C Debug -L quick --output-on-failure`.
- `Windows Vulkan smoke` builds `kera_samples` plus `kera_ppm_metrics`, sets
  `KERA_RUN_GPU_SMOKE=1`, and runs
  `ctest --test-dir build/windows-debug -C Debug -L vulkan --output-on-failure`.

The Vulkan smoke job expects a runner with a Vulkan runtime and validation layers. If it fails, the
workflow uploads sample logs and visual-regression captures from
`build/windows-debug/samples/Debug`.

## CTest Labels

List registered tests:

```powershell
ctest --test-dir build/windows-debug -C Debug -N
```

Run only fast host-side tests:

```powershell
ctest --test-dir build/windows-debug -C Debug -L quick --output-on-failure
```

The C++ unit-style tests are consolidated into one GoogleTest executable, `kera_unit_tests`.
CTest keeps focused entries with labels by running that binary with `--gtest_filter`, so renderer,
API, glTF, logger, repo-policy, and static Slang shader-contract lanes remain individually
selectable while sharing one test binary. The repo-policy filters cover STL-free ABI headers,
sample public-API includes, and logging facade rules.

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

Kera uses Slang shader sources, with validation split into three layers:

- `kera_unit_slang_shader_contracts` checks static source contracts, such as entry point names and
  `[shader(...)]` stage annotations.
- `kera_slang_reflection_tests` owns external `slangc` orchestration. It emits reflection JSON and
  SPIR-V, checks key JSON tokens, then invokes `kera_unit_tests` with
  `KeraSlangReflectionMetadata.GeneratedJsonContracts`.
- Runtime shader-program creation compiles and reflects Slang stages through the Slang API, then
  stores merged typed metadata on the shader-program resource.

The runtime reflection path keeps the linked-program session, module, entry point, and composite
component alive while calling `getLayout()` and `toJson()`. The JSON is parsed into
`SlangReflectionMetadata`, which renderer validation uses for descriptor and vertex-input contracts.

Pipeline creation can derive descriptor set layouts from shader-program reflection when samples
leave descriptor sets empty. This keeps descriptor names, bindings, and descriptor kinds owned by
the Slang source instead of duplicated in sample code.

The common STL-free sample path is:

- `createGraphicsShaderProgram()` builds a two-stage graphics program from one Slang file.
- `PipelineReflectionBuilder` declares host vertex bindings and required shader resources.
- `createGraphicsPipeline(GraphicsPipelineCreateDesc)` applies the contract using stored reflection
  metadata.
- `updateDescriptors()` updates descriptors by reflected shader variable name and returns an
  accumulated `ok()` result.

This lane proves that samples do not need private renderer reflection headers, that reflected vertex
attributes map cleanly to host bindings, and that missing, ambiguous, duplicate, or unused semantic
mappings are rejected.

The sample shaders avoid manual `register(...)` bindings where Slang reflection can supply layout
data, and single-set reflected pipelines can allocate and bind descriptor sets without repeating the
reflected set index in sample code.
