// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#if defined(KERA_EXPORT)
#define KERA_API __declspec(dllexport)
#elif defined(KERA_IMPORT)
#define KERA_API __declspec(dllimport)
#else
#define KERA_API
#endif
#elif defined(__GNUC__)
#if defined(KERA_EXPORT)
#define KERA_API __attribute__((visibility("default")))
#else
#define KERA_API
#endif
#else
#define KERA_API
#endif

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
        int32_t index
#ifdef __cplusplus
            = -1
#endif
            ;
        uint32_t generation
#ifdef __cplusplus
            = 0
#endif
            ;
#ifdef __cplusplus
        constexpr bool isValid() const noexcept
        {
            return index != -1;
        }
        constexpr bool operator==(const KeraHandle&) const noexcept = default;
#endif
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
#ifdef __cplusplus
        constexpr bool operator==(const KeraExtent2D&) const noexcept = default;
#endif
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

    typedef enum KeraIndexFormat
    {
        KERA_INDEX_FORMAT_UINT16 = 0,
        KERA_INDEX_FORMAT_UINT32 = 1
    } KeraIndexFormat;

    typedef enum KeraVertexFormat
    {
        KERA_VERTEX_FORMAT_FLOAT2 = 0,
        KERA_VERTEX_FORMAT_FLOAT3 = 1,
        KERA_VERTEX_FORMAT_FLOAT4 = 2
    } KeraVertexFormat;

    typedef enum KeraVertexInputRate
    {
        KERA_VERTEX_INPUT_RATE_VERTEX = 0,
        KERA_VERTEX_INPUT_RATE_INSTANCE = 1
    } KeraVertexInputRate;

    typedef enum KeraPrimitiveTopologyKind
    {
        KERA_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST = 0
    } KeraPrimitiveTopologyKind;

    typedef enum KeraCullModeKind
    {
        KERA_CULL_MODE_NONE = 0,
        KERA_CULL_MODE_FRONT = 1,
        KERA_CULL_MODE_BACK = 2
    } KeraCullModeKind;

    typedef enum KeraFrontFaceKind
    {
        KERA_FRONT_FACE_CLOCKWISE = 0,
        KERA_FRONT_FACE_COUNTER_CLOCKWISE = 1
    } KeraFrontFaceKind;

    typedef enum KeraBlendModeKind
    {
        KERA_BLEND_MODE_OPAQUE = 0,
        KERA_BLEND_MODE_ALPHA = 1
    } KeraBlendModeKind;

    typedef enum KeraDescriptorType
    {
        KERA_DESCRIPTOR_TYPE_UNIFORM_BUFFER = 0,
        KERA_DESCRIPTOR_TYPE_SAMPLED_IMAGE = 1,
        KERA_DESCRIPTOR_TYPE_SAMPLER = 2
    } KeraDescriptorType;

    typedef enum KeraTextureFormat
    {
        KERA_TEXTURE_FORMAT_RGBA8 = 0,
        KERA_TEXTURE_FORMAT_RGBA8_SRGB = 1,
        KERA_TEXTURE_FORMAT_B10G11R11_UFLOAT = 2,
        KERA_TEXTURE_FORMAT_DEPTH32 = 3
    } KeraTextureFormat;

    typedef enum KeraSamplerFilter
    {
        KERA_SAMPLER_FILTER_NEAREST = 0,
        KERA_SAMPLER_FILTER_LINEAR = 1
    } KeraSamplerFilter;

    typedef enum KeraSamplerMipFilter
    {
        KERA_SAMPLER_MIP_FILTER_NEAREST = 0,
        KERA_SAMPLER_MIP_FILTER_LINEAR = 1
    } KeraSamplerMipFilter;

    typedef enum KeraSamplerAddressMode
    {
        KERA_SAMPLER_ADDRESS_CLAMP_TO_EDGE = 0,
        KERA_SAMPLER_ADDRESS_REPEAT = 1,
        KERA_SAMPLER_ADDRESS_MIRRORED_REPEAT = 2
    } KeraSamplerAddressMode;

    typedef enum KeraGltfAlphaMode
    {
        KERA_GLTF_ALPHA_OPAQUE = 0,
        KERA_GLTF_ALPHA_MASK = 1,
        KERA_GLTF_ALPHA_BLEND = 2
    } KeraGltfAlphaMode;

    typedef enum KeraLogLevel
    {
        KERA_LOG_LEVEL_DEBUG = 0,
        KERA_LOG_LEVEL_INFO = 1,
        KERA_LOG_LEVEL_WARNING = 2,
        KERA_LOG_LEVEL_ERROR = 3,
        KERA_LOG_LEVEL_FATAL = 4
    } KeraLogLevel;

    typedef struct KeraRendererCreateDesc
    {
        KeraGraphicsBackend backend;
        void* sdlWindow;
        uint32_t width;
        uint32_t height;
    } KeraRendererCreateDesc;

    typedef struct KeraBufferDesc
    {
        size_t size;
        KeraBufferUsageKind usage;
        KeraMemoryAccess memoryAccess;
        KeraStringView debugName;
    } KeraBufferDesc;

    typedef struct KeraTextureDesc
    {
        uint32_t width;
        uint32_t height;
        KeraTextureFormat format;
        uint32_t mipLevels;
        uint8_t generateMipmaps;
        uint8_t renderTarget;
        uint8_t sampled;
        uint8_t depthStencil;
        KeraStringView debugName;
    } KeraTextureDesc;

    typedef struct KeraSamplerDesc
    {
        KeraSamplerFilter minFilter;
        KeraSamplerFilter magFilter;
        KeraSamplerMipFilter mipFilter;
        KeraSamplerAddressMode addressModeU;
        KeraSamplerAddressMode addressModeV;
        float minLod;
        float maxLod;
        float maxAnisotropy;
        KeraStringView debugName;
    } KeraSamplerDesc;

    typedef struct KeraRenderTargetDesc
    {
        KeraTextureHandle colorTexture;
        KeraTextureHandle depthTexture;
        KeraStringView debugName;
    } KeraRenderTargetDesc;

    typedef struct KeraGraphicsShaderProgramDesc
    {
        KeraStringView path;
        KeraStringView vertexEntryPoint;
        KeraStringView fragmentEntryPoint;
        KeraShaderSourceKind source;
        KeraStringView debugName;
    } KeraGraphicsShaderProgramDesc;

    typedef struct KeraVertexInputBindingDesc
    {
        uint32_t binding;
        uint32_t stride;
        KeraVertexInputRate inputRate;
    } KeraVertexInputBindingDesc;

    typedef struct KeraVertexInputFieldDesc
    {
        KeraStringView parameterName;
        KeraStringView fieldName;
        uint32_t binding;
        uint32_t offset;
        KeraVertexFormat format;
    } KeraVertexInputFieldDesc;

    typedef struct KeraVertexInputLayout
    {
        const KeraVertexInputBindingDesc* bindings;
        size_t bindingCount;
        const KeraVertexInputFieldDesc* fields;
        size_t fieldCount;
    } KeraVertexInputLayout;

    typedef struct KeraGraphicsPipelineCreateDesc
    {
        KeraShaderProgramHandle shaderProgram;
        KeraVertexInputLayout vertexInput;
        KeraRenderTargetHandle renderTarget;
        KeraPrimitiveTopologyKind topology;
        KeraCullModeKind cullMode;
        KeraFrontFaceKind frontFace;
        KeraBlendModeKind blendMode;
        uint8_t depthTest;
        uint8_t depthWrite;
        KeraStringView debugName;
    } KeraGraphicsPipelineCreateDesc;

    typedef struct KeraClearColorValue
    {
        float r;
        float g;
        float b;
        float a;
    } KeraClearColorValue;

    typedef struct KeraRenderPassDesc
    {
        KeraClearColorValue clearColor;
        float clearDepth;
    } KeraRenderPassDesc;

    typedef struct KeraRendererResourceStats
    {
        uint32_t shaderModules;
        uint32_t shaderPrograms;
        uint32_t buffers;
        uint32_t textures;
        uint32_t samplers;
        uint32_t renderTargets;
        uint32_t graphicsPipelines;
        uint32_t descriptorSets;
        uint32_t frames;
    } KeraRendererResourceStats;

    typedef struct KeraRendererStats
    {
        KeraRendererResourceStats resources;
        uint32_t drawCallsThisFrame;
        uint32_t pipelinesBoundThisFrame;
        uint32_t descriptorSetsBoundThisFrame;
        uint32_t vertexBuffersBoundThisFrame;
        uint32_t indexBuffersBoundThisFrame;
        uint32_t bufferUploadsThisFrame;
        uint32_t textureUploadsThisFrame;
        uint32_t validationIssuesThisFrame;
        uint64_t frameIndex;
    } KeraRendererStats;

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

    typedef struct KeraGltfMaterialTextures
    {
        KeraTextureHandle baseColor;
        KeraTextureHandle metalRoughness;
        KeraTextureHandle emissive;
        KeraTextureHandle occlusion;
        KeraTextureHandle normal;
    } KeraGltfMaterialTextures;

    typedef struct KeraGltfMaterialFactors
    {
        float baseColor[4];
        float emissive[3];
        float metallic;
        float roughness;
        float normalScale;
        float occlusionStrength;
        float alphaCutoff;
        KeraGltfAlphaMode alphaMode;
        uint8_t doubleSided;
    } KeraGltfMaterialFactors;

    typedef struct KeraGltfLoadedModel
    {
        KeraBufferHandle vertexBuffer;
        KeraBufferHandle indexBuffer;
        KeraGltfMaterialTextures materialTextures;
        KeraSamplerHandle materialSampler;
        KeraIndexFormat indexFormat;
        uint32_t indexCount;
        float transform[16];
        KeraGltfMaterialFactors materialFactors;
    } KeraGltfLoadedModel;

    typedef struct KeraGltfLoadDesc
    {
        KeraStringView path;
        KeraStringView debugName;
        uint8_t requireMaterialTextures;
    } KeraGltfLoadDesc;

    typedef struct KeraGltfVertex
    {
        float position[3];
        float normal[3];
        float uv[2];
        float tangent[4];
    } KeraGltfVertex;

    typedef struct KeraIblSphericalHarmonics
    {
        float coefficients[9][4];
    } KeraIblSphericalHarmonics;

    typedef struct KeraIblEnvironmentLoadDesc
    {
        KeraStringView iblKtxPath;
        KeraStringView skyboxKtxPath;
        KeraStringView sphericalHarmonicsPath;
        KeraStringView debugName;
    } KeraIblEnvironmentLoadDesc;

    typedef struct KeraIblEnvironment
    {
        KeraTextureHandle iblTexture;
        KeraTextureHandle skyboxTexture;
        KeraSamplerHandle sampler;
        KeraIblSphericalHarmonics sphericalHarmonics;
        uint32_t iblMipLevels;
        uint32_t skyboxMipLevels;
    } KeraIblEnvironment;

    typedef struct KeraUniformRingBufferInfo
    {
        size_t elementSize;
        size_t slotStride;
        uint32_t slotCount;
    } KeraUniformRingBufferInfo;

    typedef struct KeraRenderer KeraRenderer;

    typedef struct KeraRendererApiV1
    {
        uint32_t abiVersion;
        KeraRenderer* (*createRenderer)(const KeraRendererCreateDesc* desc);
        void (*destroy)(KeraRenderer* renderer);
        void (*shutdown)(KeraRenderer* renderer);
        KeraGraphicsBackend (*getBackend)(const KeraRenderer* renderer);
        KeraExtent2D (*getDrawableExtent)(const KeraRenderer* renderer);
        KeraRendererStats (*getStats)(const KeraRenderer* renderer);
        int (*resize)(KeraRenderer* renderer, KeraExtent2D extent);
        int (*initializeUi)(KeraRenderer* renderer);
        void (*shutdownUi)(KeraRenderer* renderer);
        void (*handleUiEvent)(KeraRenderer* renderer, const void* sdlEvent);
        void (*beginUi)(KeraRenderer* renderer);
        void (*endUi)(KeraRenderer* renderer);
        void (*renderUi)(KeraRenderer* renderer, KeraFrameHandle frame);
        int (*isUiAvailable)(const KeraRenderer* renderer);
        KeraShaderProgramHandle (*createGraphicsShaderProgram)(KeraRenderer* renderer,
                                                               const KeraGraphicsShaderProgramDesc* desc);
        int (*destroyShaderProgram)(KeraRenderer* renderer, KeraShaderProgramHandle program);
        KeraBufferHandle (*createBuffer)(KeraRenderer* renderer, const KeraBufferDesc* desc);
        KeraBufferHandle (*createUniformRingBuffer)(KeraRenderer* renderer, size_t elementSize, uint32_t slotCount);
        int (*destroyBuffer)(KeraRenderer* renderer, KeraBufferHandle buffer);
        int (*mapBuffer)(KeraRenderer* renderer, KeraBufferHandle buffer, void** data);
        void (*unmapBuffer)(KeraRenderer* renderer, KeraBufferHandle buffer);
        int (*uploadBuffer)(KeraRenderer* renderer, KeraBufferHandle buffer, const void* data, size_t size,
                            size_t offset);
        int (*uploadUniformRingBuffer)(KeraRenderer* renderer, KeraBufferHandle buffer, KeraFrameHandle frame,
                                       const void* data, size_t size);
        KeraUniformRingBufferInfo (*getUniformRingBufferInfo)(KeraRenderer* renderer, KeraBufferHandle buffer);
        uint32_t (*getUniformRingBufferSlot)(KeraRenderer* renderer, KeraBufferHandle buffer, KeraFrameHandle frame);
        size_t (*getUniformRingBufferSlotOffset)(KeraRenderer* renderer, KeraBufferHandle buffer, uint32_t slot);
        KeraTextureHandle (*createTexture)(KeraRenderer* renderer, const KeraTextureDesc* desc);
        int (*uploadTexture)(KeraRenderer* renderer, KeraTextureHandle texture, const void* data, size_t size);
        int (*destroyTexture)(KeraRenderer* renderer, KeraTextureHandle texture);
        KeraSamplerHandle (*createSampler)(KeraRenderer* renderer, const KeraSamplerDesc* desc);
        int (*destroySampler)(KeraRenderer* renderer, KeraSamplerHandle sampler);
        KeraRenderTargetHandle (*createRenderTarget)(KeraRenderer* renderer, const KeraRenderTargetDesc* desc);
        int (*destroyRenderTarget)(KeraRenderer* renderer, KeraRenderTargetHandle target);
        KeraRendererValidationReport (*validateVertexInputLayout)(const KeraRenderer* renderer,
                                                                  KeraShaderProgramHandle shaderProgram,
                                                                  KeraVertexInputLayout vertexInput);
        KeraGraphicsPipelineHandle (*createGraphicsPipeline)(KeraRenderer* renderer,
                                                             const KeraGraphicsPipelineCreateDesc* desc);
        int (*destroyGraphicsPipeline)(KeraRenderer* renderer, KeraGraphicsPipelineHandle pipeline);
        KeraDescriptorSetHandle (*createDescriptorSet)(KeraRenderer* renderer, KeraGraphicsPipelineHandle pipeline,
                                                       uint32_t set);
        int (*destroyDescriptorSet)(KeraRenderer* renderer, KeraDescriptorSetHandle set);
        int (*updateDescriptorBuffer)(KeraRenderer* renderer, KeraDescriptorSetHandle set, KeraStringView name,
                                      KeraBufferHandle buffer, size_t offset, size_t range);
        int (*updateDescriptorTexture)(KeraRenderer* renderer, KeraDescriptorSetHandle set, KeraStringView name,
                                       KeraTextureHandle texture);
        int (*updateDescriptorSampler)(KeraRenderer* renderer, KeraDescriptorSetHandle set, KeraStringView name,
                                       KeraSamplerHandle sampler);
        KeraRendererValidationReport (*validateDescriptorSet)(const KeraRenderer* renderer,
                                                              KeraDescriptorSetHandle descriptorSet);
        int (*setDebugName)(KeraRenderer* renderer, KeraHandle handle, KeraStringView name);
        KeraFrameHandle (*beginFrame)(KeraRenderer* renderer);
        void (*beginRenderPass)(KeraRenderer* renderer, KeraFrameHandle frame, const KeraRenderPassDesc* desc);
        void (*beginRenderPassTarget)(KeraRenderer* renderer, KeraFrameHandle frame, KeraRenderTargetHandle target,
                                      const KeraRenderPassDesc* desc);
        void (*endRenderPass)(KeraRenderer* renderer, KeraFrameHandle frame);
        void (*bindPipeline)(KeraRenderer* renderer, KeraFrameHandle frame, KeraGraphicsPipelineHandle pipeline);
        void (*bindVertexBuffer)(KeraRenderer* renderer, KeraFrameHandle frame, uint32_t slot, KeraBufferHandle buffer,
                                 size_t offset);
        void (*bindIndexBuffer)(KeraRenderer* renderer, KeraFrameHandle frame, KeraBufferHandle buffer,
                                KeraIndexFormat format, size_t offset);
        void (*bindDescriptorSet)(KeraRenderer* renderer, KeraFrameHandle frame, KeraGraphicsPipelineHandle pipeline,
                                  uint32_t setIndex, KeraDescriptorSetHandle descriptorSet);
        void (*drawIndexed)(KeraRenderer* renderer, KeraFrameHandle frame, uint32_t indexCount, uint32_t instanceCount);
        int (*endFrame)(KeraRenderer* renderer, KeraFrameHandle frame);
        int (*loadGltfModel)(KeraRenderer* renderer, const KeraGltfLoadDesc* desc, KeraGltfLoadedModel* outModel);
        void (*destroyGltfModel)(KeraRenderer* renderer, KeraGltfLoadedModel* model);
        int (*loadIblEnvironment)(KeraRenderer* renderer, const KeraIblEnvironmentLoadDesc* desc,
                                  KeraIblEnvironment* outEnvironment);
        void (*destroyIblEnvironment)(KeraRenderer* renderer, KeraIblEnvironment* env);
    } KeraRendererApiV1;

    KERA_API const KeraRendererApiV1* keraGetRendererApiV1(void);
    KERA_API void keraLog(KeraLogLevel level, KeraStringView message);

#ifdef __cplusplus
}
#endif
