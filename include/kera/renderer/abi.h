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

    typedef enum EKeraGraphicsBackend
    {
        KERA_GRAPHICS_BACKEND_VULKAN = 0
    } KeraGraphicsBackend;

    typedef enum EKeraShaderStage
    {
        KERA_SHADER_STAGE_VERTEX = 0,
        KERA_SHADER_STAGE_FRAGMENT = 1,
        KERA_SHADER_STAGE_COMPUTE = 2,
        KERA_SHADER_STAGE_ALL_GRAPHICS = 3
    } KeraShaderStage;

    typedef enum EKeraShaderSourceKind
    {
        KERA_SHADER_SOURCE_SLANG_FILE = 0,
        KERA_SHADER_SOURCE_SPIRV_FILE = 1,
        KERA_SHADER_SOURCE_SPIRV_BINARY = 2
    } KeraShaderSourceKind;

    typedef enum EKeraBufferUsageKind
    {
        KERA_BUFFER_USAGE_VERTEX = 0,
        KERA_BUFFER_USAGE_INDEX = 1,
        KERA_BUFFER_USAGE_UNIFORM = 2,
        KERA_BUFFER_USAGE_STORAGE = 3
    } KeraBufferUsageKind;

    typedef enum EKeraMemoryAccess
    {
        KERA_MEMORY_ACCESS_GPU_ONLY = 0,
        KERA_MEMORY_ACCESS_CPU_WRITE = 1
    } KeraMemoryAccess;

    typedef enum EKeraIndexFormat
    {
        KERA_INDEX_FORMAT_UINT16 = 0,
        KERA_INDEX_FORMAT_UINT32 = 1
    } KeraIndexFormat;

    typedef enum EKeraVertexFormat
    {
        KERA_VERTEX_FORMAT_FLOAT2 = 0,
        KERA_VERTEX_FORMAT_FLOAT3 = 1,
        KERA_VERTEX_FORMAT_FLOAT4 = 2
    } KeraVertexFormat;

    typedef enum EKeraVertexInputRate
    {
        KERA_VERTEX_INPUT_RATE_VERTEX = 0,
        KERA_VERTEX_INPUT_RATE_INSTANCE = 1
    } KeraVertexInputRate;

    typedef enum EKeraPrimitiveTopologyKind
    {
        KERA_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST = 0
    } KeraPrimitiveTopologyKind;

    typedef enum EKeraCullModeKind
    {
        KERA_CULL_MODE_NONE = 0,
        KERA_CULL_MODE_FRONT = 1,
        KERA_CULL_MODE_BACK = 2
    } KeraCullModeKind;

    typedef enum EKeraFrontFaceKind
    {
        KERA_FRONT_FACE_CLOCKWISE = 0,
        KERA_FRONT_FACE_COUNTER_CLOCKWISE = 1
    } KeraFrontFaceKind;

    typedef enum EKeraBlendModeKind
    {
        KERA_BLEND_MODE_OPAQUE = 0,
        KERA_BLEND_MODE_ALPHA = 1
    } KeraBlendModeKind;

    typedef enum EKeraDescriptorType
    {
        KERA_DESCRIPTOR_TYPE_UNIFORM_BUFFER = 0,
        KERA_DESCRIPTOR_TYPE_SAMPLED_IMAGE = 1,
        KERA_DESCRIPTOR_TYPE_SAMPLER = 2
    } KeraDescriptorType;

    typedef enum EKeraTextureFormat
    {
        KERA_TEXTURE_FORMAT_RGBA8 = 0,
        KERA_TEXTURE_FORMAT_RGBA8_SRGB = 1,
        KERA_TEXTURE_FORMAT_B10G11R11_UFLOAT = 2,
        KERA_TEXTURE_FORMAT_DEPTH32 = 3
    } KeraTextureFormat;

    typedef enum EKeraSamplerFilter
    {
        KERA_SAMPLER_FILTER_NEAREST = 0,
        KERA_SAMPLER_FILTER_LINEAR = 1
    } KeraSamplerFilter;

    typedef enum EKeraSamplerMipFilter
    {
        KERA_SAMPLER_MIP_FILTER_NEAREST = 0,
        KERA_SAMPLER_MIP_FILTER_LINEAR = 1
    } KeraSamplerMipFilter;

    typedef enum EKeraSamplerAddressMode
    {
        KERA_SAMPLER_ADDRESS_CLAMP_TO_EDGE = 0,
        KERA_SAMPLER_ADDRESS_REPEAT = 1,
        KERA_SAMPLER_ADDRESS_MIRRORED_REPEAT = 2
    } KeraSamplerAddressMode;

    typedef enum EKeraGltfAlphaMode
    {
        KERA_GLTF_ALPHA_OPAQUE = 0,
        KERA_GLTF_ALPHA_MASK = 1,
        KERA_GLTF_ALPHA_BLEND = 2
    } KeraGltfAlphaMode;

    typedef enum EKeraLogLevel
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
        void* sdl_window;
        uint32_t width;
        uint32_t height;
    } KeraRendererCreateDesc;

    typedef struct KeraBufferDesc
    {
        size_t size;
        KeraBufferUsageKind usage;
        KeraMemoryAccess memory_access;
        KeraStringView debug_name;
    } KeraBufferDesc;

    typedef struct KeraTextureDesc
    {
        uint32_t width;
        uint32_t height;
        KeraTextureFormat format;
        uint32_t mip_levels;
        uint8_t generate_mipmaps;
        uint8_t render_target;
        uint8_t sampled;
        uint8_t depth_stencil;
        KeraStringView debug_name;
    } KeraTextureDesc;

    typedef struct KeraSamplerDesc
    {
        KeraSamplerFilter min_filter;
        KeraSamplerFilter mag_filter;
        KeraSamplerMipFilter mip_filter;
        KeraSamplerAddressMode address_mode_u;
        KeraSamplerAddressMode address_mode_v;
        float min_lod;
        float max_lod;
        float max_anisotropy;
        KeraStringView debug_name;
    } KeraSamplerDesc;

    typedef struct KeraRenderTargetDesc
    {
        KeraTextureHandle color_texture;
        KeraTextureHandle depth_texture;
        KeraStringView debug_name;
    } KeraRenderTargetDesc;

    typedef struct KeraGraphicsShaderProgramDesc
    {
        KeraStringView path;
        KeraStringView vertex_entry_point;
        KeraStringView fragment_entry_point;
        KeraShaderSourceKind source;
        KeraStringView debug_name;
    } KeraGraphicsShaderProgramDesc;

    typedef struct KeraVertexInputBindingDesc
    {
        uint32_t binding;
        uint32_t stride;
        KeraVertexInputRate input_rate;
    } KeraVertexInputBindingDesc;

    typedef struct KeraVertexInputFieldDesc
    {
        KeraStringView parameter_name;
        KeraStringView field_name;
        uint32_t binding;
        uint32_t offset;
        KeraVertexFormat format;
    } KeraVertexInputFieldDesc;

    typedef struct KeraVertexInputLayout
    {
        const KeraVertexInputBindingDesc* bindings;
        size_t binding_count;
        const KeraVertexInputFieldDesc* fields;
        size_t field_count;
    } KeraVertexInputLayout;

    typedef struct KeraGraphicsPipelineCreateDesc
    {
        KeraShaderProgramHandle shader_program;
        KeraVertexInputLayout vertex_input;
        KeraRenderTargetHandle render_target;
        KeraPrimitiveTopologyKind topology;
        KeraCullModeKind cull_mode;
        KeraFrontFaceKind front_face;
        KeraBlendModeKind blend_mode;
        uint8_t depth_test;
        uint8_t depth_write;
        KeraStringView debug_name;
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
        KeraClearColorValue clear_color;
        float clear_depth;
    } KeraRenderPassDesc;

    typedef struct KeraRendererResourceStats
    {
        uint32_t shader_modules;
        uint32_t shader_programs;
        uint32_t buffers;
        uint32_t textures;
        uint32_t samplers;
        uint32_t render_targets;
        uint32_t graphics_pipelines;
        uint32_t descriptor_sets;
        uint32_t frames;
    } KeraRendererResourceStats;

    typedef struct KeraRendererStats
    {
        KeraRendererResourceStats resources;
        uint32_t draw_calls_this_frame;
        uint32_t pipelines_bound_this_frame;
        uint32_t descriptor_sets_bound_this_frame;
        uint32_t vertex_buffers_bound_this_frame;
        uint32_t index_buffers_bound_this_frame;
        uint32_t buffer_uploads_this_frame;
        uint32_t texture_uploads_this_frame;
        uint32_t validation_issues_this_frame;
        uint64_t frame_index;
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
        size_t issue_count;
    } KeraRendererValidationReport;

    typedef struct KeraGltfMaterialTextures
    {
        KeraTextureHandle base_color;
        KeraTextureHandle metal_roughness;
        KeraTextureHandle emissive;
        KeraTextureHandle occlusion;
        KeraTextureHandle normal;
    } KeraGltfMaterialTextures;

    typedef struct KeraGltfMaterialFactors
    {
        float base_color[4];
        float emissive[3];
        float metallic;
        float roughness;
        float normal_scale;
        float occlusion_strength;
        float alpha_cutoff;
        KeraGltfAlphaMode alpha_mode;
        uint8_t double_sided;
    } KeraGltfMaterialFactors;

    typedef struct KeraGltfLoadedModel
    {
        KeraBufferHandle vertex_buffer;
        KeraBufferHandle index_buffer;
        KeraGltfMaterialTextures material_textures;
        KeraSamplerHandle material_sampler;
        KeraIndexFormat index_format;
        uint32_t index_count;
        float transform[16];
        KeraGltfMaterialFactors material_factors;
    } KeraGltfLoadedModel;

    typedef struct KeraGltfLoadDesc
    {
        KeraStringView path;
        KeraStringView debug_name;
        uint8_t require_material_textures;
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
        KeraStringView ibl_ktx_path;
        KeraStringView skybox_ktx_path;
        KeraStringView spherical_harmonics_path;
        KeraStringView debug_name;
    } KeraIblEnvironmentLoadDesc;

    typedef struct KeraIblEnvironment
    {
        KeraTextureHandle ibl_texture;
        KeraTextureHandle skybox_texture;
        KeraSamplerHandle sampler;
        KeraIblSphericalHarmonics spherical_harmonics;
        uint32_t ibl_mip_levels;
        uint32_t skybox_mip_levels;
    } KeraIblEnvironment;

    typedef struct KeraUniformRingBufferInfo
    {
        size_t element_size;
        size_t slot_stride;
        uint32_t slot_count;
    } KeraUniformRingBufferInfo;

    typedef struct KeraRenderer KeraRenderer;

    typedef struct KeraRendererApiV1
    {
        uint32_t abi_version;
        KeraRenderer* (*create_renderer)(const KeraRendererCreateDesc* desc);
        void (*destroy)(KeraRenderer* renderer);
        void (*shutdown)(KeraRenderer* renderer);
        KeraGraphicsBackend (*get_backend)(const KeraRenderer* renderer);
        KeraExtent2D (*get_drawable_extent)(const KeraRenderer* renderer);
        KeraRendererStats (*get_stats)(const KeraRenderer* renderer);
        int (*resize)(KeraRenderer* renderer, KeraExtent2D extent);
        int (*initialize_ui)(KeraRenderer* renderer);
        void (*shutdown_ui)(KeraRenderer* renderer);
        void (*handle_ui_event)(KeraRenderer* renderer, const void* sdl_event);
        void (*begin_ui)(KeraRenderer* renderer);
        void (*end_ui)(KeraRenderer* renderer);
        void (*render_ui)(KeraRenderer* renderer, KeraFrameHandle frame);
        int (*is_ui_available)(const KeraRenderer* renderer);
        KeraShaderProgramHandle (*create_graphics_shader_program)(KeraRenderer* renderer,
                                                               const KeraGraphicsShaderProgramDesc* desc);
        int (*destroy_shader_program)(KeraRenderer* renderer, KeraShaderProgramHandle program);
        KeraBufferHandle (*create_buffer)(KeraRenderer* renderer, const KeraBufferDesc* desc);
        KeraBufferHandle (*create_uniform_ring_buffer)(KeraRenderer* renderer, size_t element_size, uint32_t slot_count);
        int (*destroy_buffer)(KeraRenderer* renderer, KeraBufferHandle buffer);
        int (*map_buffer)(KeraRenderer* renderer, KeraBufferHandle buffer, void** data);
        void (*unmap_buffer)(KeraRenderer* renderer, KeraBufferHandle buffer);
        int (*upload_buffer)(KeraRenderer* renderer, KeraBufferHandle buffer, const void* data, size_t size,
                            size_t offset);
        int (*upload_uniform_ring_buffer)(KeraRenderer* renderer, KeraBufferHandle buffer, KeraFrameHandle frame,
                                       const void* data, size_t size);
        KeraUniformRingBufferInfo (*get_uniform_ring_buffer_info)(KeraRenderer* renderer, KeraBufferHandle buffer);
        uint32_t (*get_uniform_ring_buffer_slot)(KeraRenderer* renderer, KeraBufferHandle buffer, KeraFrameHandle frame);
        size_t (*get_uniform_ring_buffer_slot_offset)(KeraRenderer* renderer, KeraBufferHandle buffer, uint32_t slot);
        KeraTextureHandle (*create_texture)(KeraRenderer* renderer, const KeraTextureDesc* desc);
        int (*upload_texture)(KeraRenderer* renderer, KeraTextureHandle texture, const void* data, size_t size);
        int (*destroy_texture)(KeraRenderer* renderer, KeraTextureHandle texture);
        KeraSamplerHandle (*create_sampler)(KeraRenderer* renderer, const KeraSamplerDesc* desc);
        int (*destroy_sampler)(KeraRenderer* renderer, KeraSamplerHandle sampler);
        KeraRenderTargetHandle (*create_render_target)(KeraRenderer* renderer, const KeraRenderTargetDesc* desc);
        int (*destroy_render_target)(KeraRenderer* renderer, KeraRenderTargetHandle target);
        KeraRendererValidationReport (*validate_vertex_input_layout)(const KeraRenderer* renderer,
                                                                  KeraShaderProgramHandle shader_program,
                                                                  KeraVertexInputLayout vertex_input);
        KeraGraphicsPipelineHandle (*create_graphics_pipeline)(KeraRenderer* renderer,
                                                             const KeraGraphicsPipelineCreateDesc* desc);
        int (*destroy_graphics_pipeline)(KeraRenderer* renderer, KeraGraphicsPipelineHandle pipeline);
        KeraDescriptorSetHandle (*create_descriptor_set)(KeraRenderer* renderer, KeraGraphicsPipelineHandle pipeline,
                                                       uint32_t set);
        int (*destroy_descriptor_set)(KeraRenderer* renderer, KeraDescriptorSetHandle set);
        int (*update_descriptor_buffer)(KeraRenderer* renderer, KeraDescriptorSetHandle set, KeraStringView name,
                                      KeraBufferHandle buffer, size_t offset, size_t range);
        int (*update_descriptor_texture)(KeraRenderer* renderer, KeraDescriptorSetHandle set, KeraStringView name,
                                       KeraTextureHandle texture);
        int (*update_descriptor_sampler)(KeraRenderer* renderer, KeraDescriptorSetHandle set, KeraStringView name,
                                       KeraSamplerHandle sampler);
        KeraRendererValidationReport (*validate_descriptor_set)(const KeraRenderer* renderer,
                                                              KeraDescriptorSetHandle descriptor_set);
        int (*set_debug_name)(KeraRenderer* renderer, KeraHandle handle, KeraStringView name);
        KeraFrameHandle (*begin_frame)(KeraRenderer* renderer);
        void (*begin_render_pass)(KeraRenderer* renderer, KeraFrameHandle frame, const KeraRenderPassDesc* desc);
        void (*begin_render_pass_target)(KeraRenderer* renderer, KeraFrameHandle frame, KeraRenderTargetHandle target,
                                      const KeraRenderPassDesc* desc);
        void (*end_render_pass)(KeraRenderer* renderer, KeraFrameHandle frame);
        void (*bind_pipeline)(KeraRenderer* renderer, KeraFrameHandle frame, KeraGraphicsPipelineHandle pipeline);
        void (*bind_vertex_buffer)(KeraRenderer* renderer, KeraFrameHandle frame, uint32_t slot, KeraBufferHandle buffer,
                                 size_t offset);
        void (*bind_index_buffer)(KeraRenderer* renderer, KeraFrameHandle frame, KeraBufferHandle buffer,
                                KeraIndexFormat format, size_t offset);
        void (*bind_descriptor_set)(KeraRenderer* renderer, KeraFrameHandle frame, KeraGraphicsPipelineHandle pipeline,
                                  uint32_t set_index, KeraDescriptorSetHandle descriptor_set);
        void (*draw_indexed)(KeraRenderer* renderer, KeraFrameHandle frame, uint32_t index_count, uint32_t instance_count);
        int (*end_frame)(KeraRenderer* renderer, KeraFrameHandle frame);
        int (*load_gltf_model)(KeraRenderer* renderer, const KeraGltfLoadDesc* desc, KeraGltfLoadedModel* out_model);
        void (*destroy_gltf_model)(KeraRenderer* renderer, KeraGltfLoadedModel* model);
        int (*load_ibl_environment)(KeraRenderer* renderer, const KeraIblEnvironmentLoadDesc* desc,
                                  KeraIblEnvironment* out_environment);
        void (*destroy_ibl_environment)(KeraRenderer* renderer, KeraIblEnvironment* env);
    } KeraRendererApiV1;

    KERA_API const KeraRendererApiV1* keraGetRendererApiV1(void);
    KERA_API void keraLog(KeraLogLevel level, KeraStringView message);

#ifdef __cplusplus
}
#endif
