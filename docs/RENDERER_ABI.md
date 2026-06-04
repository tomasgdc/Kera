<!--
Copyright 2026 Tomas Mikalauskas
SPDX-License-Identifier: Apache-2.0
-->

# Kera Renderer ABI Policy

Kera's installed renderer ABI is the public boundary for users and samples.

- `kera/kera.h` and `kera/renderer/api.h` are public entry headers.
- `kera/renderer/abi.h` contains the concrete C ABI types and `KeraRendererApiV1`.
- Internal C++ headers may use STL, but they are not installed and are not public API.

## ABI Version

The current renderer ABI is:

```c
#define KERA_RENDERER_ABI_VERSION 1u
```

Consumers must check `KeraRendererApiV1::abiVersion` before using the function table.

## ABI Shape

The installed ABI uses:

- `KeraStringView` and `KeraByteView` borrowed views.
- `KeraHandle` plus typed renderer handles for resources.
- POD descriptors for buffers, textures, samplers, render targets, shader programs, vertex input
  layouts, render passes, stats, validation reports, and glTF-loaded models.
- Opaque `KeraRenderer`.
- `KeraRendererApiV1` for create/destroy, resources, descriptors, frame lifecycle, UI forwarding,
  resize, stats, logging, validation, and glTF loading.

The ABI does not expose STL types, C++ ownership types, or virtual interfaces.

Graphics pipeline creation uses `KeraVertexInputLayout` for the one piece Slang reflection cannot
infer: host vertex-buffer memory layout. `KeraVertexInputBindingDesc` declares numeric vertex buffer
slots and strides. `KeraVertexInputFieldDesc` maps reflected Slang vertex input field names to host
binding slots, byte offsets, and formats; `parameterName` is only needed when a field name is
ambiguous across vertex entry parameters. Shader entry points and descriptor set layouts come from
the shader program reflection produced by `createGraphicsShaderProgram()`.

`KeraRendererApiV1::validateVertexInputLayout` can be called before pipeline creation to get a
`KeraRendererValidationReport`. `createGraphicsPipeline` performs the same validation internally and
rejects invalid vertex input layouts.

## Lifetime Rules

- Borrowed views must stay alive for the duration of the call.
- Handles are opaque; consumers should pass them back unchanged.
- Renderer-owned resources must be destroyed through the same renderer.
- `KeraRendererApiV1::destroy` owns renderer teardown for ABI consumers.
- Validation-report strings are renderer-owned and should be copied by consumers if they need to
  keep them beyond the next renderer operation.

## Shared-Build Policy

Shared builds are allowed through the STL-free public ABI. Do not expose `std::string`,
`std::vector`, `std::span`, `std::function`, smart pointers, `std::optional`, STL containers, or
public virtual renderer interfaces from installed headers.

## Validation

The boundary is checked by:

- `kera_unit_repo_policy_abi_tests`
- `kera_renderer_public_api_tests`
- `kera_unit_repo_policy_sample_api_tests`

These tests scan installed headers for forbidden STL patterns and verify samples use the public
renderer API instead of private/internal Kera renderer headers.
