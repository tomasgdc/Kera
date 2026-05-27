# Kera Renderer ABI Policy

Kera has one installed C ABI layer and one C++ convenience layer:

- `kera/kera.h` and `kera/renderer/api.h` are installed public ABI entry headers that include the renderer ABI surface.
- `kera/renderer/abi.h` contains the concrete installed public C ABI types and function table declarations. It uses
  C-compatible scalar types, raw pointers, counts, opaque handles, and function pointers.
- The C++ renderer convenience headers under `include/kera/renderer/` are for Kera-owned code and samples while the
  renderer is still maturing. They expose STL types and are not stable installed ABI.

## Installed ABI Version

The current installed renderer ABI is versioned by:

```c
#define KERA_RENDERER_ABI_VERSION 1u
```

`KeraRendererApiV1::abiVersion` must be checked by consumers before using function pointers. Future ABI revisions should
add a new versioned table or append compatible fields only under an explicit version policy.

## ABI Types

The installed ABI currently defines:

- `KeraStringView`: borrowed UTF-8/string bytes as `const char* data` plus `size_t size`; not null-termination based.
- `KeraByteView`: borrowed binary data as `const void* data` plus `size_t size`.
- `KeraHandle`: opaque renderer handle identity with `int32_t index` and `uint32_t generation`.
- Handle typedefs for shader modules, shader programs, buffers, textures, samplers, render targets, graphics pipelines,
  descriptor sets, and frames.
- `KeraExtent2D`, `KeraGraphicsBackend`, shader stage/source enums, buffer usage/memory enums, and descriptor type enums.
- `KeraBufferDesc`, `KeraDescriptorBindingDesc`, and `KeraDescriptorSetLayoutDesc`.
- `KeraRendererError`: a borrowed message view for ABI error reporting when future ABI functions expose error objects.
- `KeraRendererValidationIssue` and `KeraRendererValidationReport`.
- Opaque `KeraRenderer`.

String and byte views are borrowed views. Callers must keep pointed-to memory alive for the duration of the call. Returned
views and validation-report arrays are renderer-owned unless a function explicitly documents a caller-owned allocation.

## Function Table

`KeraRendererApiV1` is intentionally small in this slice:

```c
typedef struct KeraRendererApiV1
{
    uint32_t abiVersion;
    void (*destroy)(KeraRenderer* renderer);
    KeraGraphicsBackend (*getBackend)(const KeraRenderer* renderer);
    KeraExtent2D (*getDrawableExtent)(const KeraRenderer* renderer);
    KeraBufferHandle (*createBuffer)(KeraRenderer* renderer, const KeraBufferDesc* desc);
    int (*destroyBuffer)(KeraRenderer* renderer, KeraBufferHandle buffer);
    KeraRendererValidationReport (*validateDescriptorSet)(const KeraRenderer* renderer,
                                                          KeraDescriptorSetHandle descriptorSet);
} KeraRendererApiV1;
```

This table is not the full C++ renderer surface. Advanced rendering, Slang reflection, glTF loading, descriptor updates,
pipelines, render passes, and frame submission currently live in the C++ convenience API only.

## Handle And Lifetime Rules

- Handles are opaque values. Consumers should store and pass them back unchanged, not inspect index/generation for policy.
- `destroyBuffer` currently returns an integer status, but the header does not yet document a stable zero/nonzero
  convention. Future ABI functions should document exact status semantics before external consumers depend on them.
- `destroy(KeraRenderer*)` owns renderer teardown for ABI consumers.
- Validation reports describe renderer-owned data. Consumers should copy messages immediately if they need to keep them
  past the next renderer call or renderer destruction.
- Descriptor layout arrays passed into the ABI are borrowed for the call unless a future function explicitly states that
  it copies or retains them.

## STL And Shared-Build Policy

Do not expose STL types such as `std::string`, `std::vector`, `std::span`, smart pointers, or STL-returning virtual
methods from installed public ABI headers. Those types couple consumers to the compiler, runtime library, iterator debug
level, CRT setting, and STL implementation used to build Kera.

Shared builds are blocked by default while the C++ convenience API still contains STL types. Set
`KERA_ALLOW_STL_CPP_SHARED_ABI=ON` only for local/internal builds where every consumer is built with the same compiler,
standard library, CRT mode, and CMake configuration.

## Validation

The installed public renderer headers are checked by:

- `kera_renderer_abi_header_tests`
- `cmake/ValidateRendererAbiHeaders.cmake`

The validation script fails if forbidden STL patterns appear in `include/kera/kera.h`, `include/kera/renderer/api.h`, or
`include/kera/renderer/abi.h`. `kera_renderer_public_api_tests` also checks standard layout expectations and
`KERA_RENDERER_ABI_VERSION == 1u`.
