#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define KERA_RENDERER_ABI_VERSION 1u

    typedef struct KeraStringView
    {
        const char* data;
        size_t size;
    } KeraStringView;

    typedef struct KeraByteView
    {
        const void* data;
        size_t size;
    } KeraByteView;

    typedef struct KeraHandle
    {
        int32_t index;
        uint32_t generation;
    } KeraHandle;

    typedef KeraHandle KeraShaderModuleHandle;
    typedef KeraHandle KeraShaderProgramHandle;
    typedef KeraHandle KeraBufferHandle;
    typedef KeraHandle KeraTextureHandle;
    typedef KeraHandle KeraSamplerHandle;
    typedef KeraHandle KeraRenderTargetHandle;
    typedef KeraHandle KeraGraphicsPipelineHandle;
    typedef KeraHandle KeraDescriptorSetHandle;
    typedef KeraHandle KeraFrameHandle;

    typedef struct KeraExtent2D
    {
        uint32_t width;
        uint32_t height;
    } KeraExtent2D;

    typedef enum KeraGraphicsBackend
    {
        KERA_GRAPHICS_BACKEND_VULKAN = 0
    } KeraGraphicsBackend;

    typedef enum KeraShaderStage
    {
        KERA_SHADER_STAGE_VERTEX = 0,
        KERA_SHADER_STAGE_FRAGMENT = 1,
        KERA_SHADER_STAGE_COMPUTE = 2,
        KERA_SHADER_STAGE_ALL_GRAPHICS = 3
    } KeraShaderStage;

    typedef enum KeraShaderSourceKind
    {
        KERA_SHADER_SOURCE_SLANG_FILE = 0,
        KERA_SHADER_SOURCE_SPIRV_FILE = 1,
        KERA_SHADER_SOURCE_SPIRV_BINARY = 2
    } KeraShaderSourceKind;

    typedef enum KeraBufferUsageKind
    {
        KERA_BUFFER_USAGE_VERTEX = 0,
        KERA_BUFFER_USAGE_INDEX = 1,
        KERA_BUFFER_USAGE_UNIFORM = 2,
        KERA_BUFFER_USAGE_STORAGE = 3
    } KeraBufferUsageKind;

    typedef enum KeraMemoryAccess
    {
        KERA_MEMORY_ACCESS_GPU_ONLY = 0,
        KERA_MEMORY_ACCESS_CPU_WRITE = 1
    } KeraMemoryAccess;

    typedef enum KeraDescriptorType
    {
        KERA_DESCRIPTOR_TYPE_UNIFORM_BUFFER = 0,
        KERA_DESCRIPTOR_TYPE_SAMPLED_IMAGE = 1,
        KERA_DESCRIPTOR_TYPE_SAMPLER = 2
    } KeraDescriptorType;

    typedef struct KeraBufferDesc
    {
        size_t size;
        KeraBufferUsageKind usage;
        KeraMemoryAccess memoryAccess;
        KeraStringView debugName;
    } KeraBufferDesc;

    typedef struct KeraDescriptorBindingDesc
    {
        KeraStringView name;
        uint32_t binding;
        KeraDescriptorType type;
        KeraShaderStage stage;
        uint32_t count;
    } KeraDescriptorBindingDesc;

    typedef struct KeraDescriptorSetLayoutDesc
    {
        uint32_t set;
        const KeraDescriptorBindingDesc* bindings;
        size_t bindingCount;
    } KeraDescriptorSetLayoutDesc;

    typedef struct KeraRendererError
    {
        KeraStringView message;
    } KeraRendererError;

    typedef struct KeraRendererValidationIssue
    {
        KeraStringView message;
        KeraStringView name;
        uint32_t set;
        uint32_t binding;
    } KeraRendererValidationIssue;

    typedef struct KeraRendererValidationReport
    {
        const KeraRendererValidationIssue* issues;
        size_t issueCount;
    } KeraRendererValidationReport;

    typedef struct KeraRenderer KeraRenderer;

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

#ifdef __cplusplus
}
#endif
