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

## Frame Capture Lane

Use this lane when validation passes but rendered output, image layouts, descriptors, or pass ordering need visible
proof.

RenderDoc capture:

```powershell
$env:KERA_RUN_GPU_SMOKE = '1'
renderdoccmd capture --wait-for-exit --output build/windows-debug/samples/Debug/kera_many_lights `
    build/windows-debug/samples/Debug/kera_samples.exe --smoke-test --smoke-frames 2 --sample-index 2
```

Nsight Graphics can launch the same executable and arguments:

```powershell
build/windows-debug/samples/Debug/kera_samples.exe --smoke-test --smoke-frames 2 --sample-index 2
```

Expected first capture target:

- sample index `2` (`InstancedTriangleManyLightsSample`)
- geometry pass writes an offscreen render target
- lighting pass samples that texture through sampled-image and sampler descriptors
- validation output should contain no `[ERROR] Vulkan validation:` lines

## Shader Contract Lane

Kera uses Slang shader sources. The shader CTest lane intentionally validates source files, entry point names, and
stage annotations instead of requiring a Vulkan-only SPIR-V validator. Runtime Vulkan compilation can still generate
SPIR-V through the existing Slang compiler path.
