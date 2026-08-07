// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace kera
{

    enum class EGraphicsBackend
    {
        VULKAN
    };

    enum class EShaderStage
    {
        VERTEX,
        FRAGMENT,
        COMPUTE,
        ALL_GRAPHICS
    };

    enum class EShaderSourceKind
    {
        SLANG_FILE,
        SPIRV_FILE,
        SPIRV_BINARY
    };

    enum class EBufferUsageKind
    {
        VERTEX,
        INDEX,
        UNIFORM,
        STORAGE
    };

    enum class EMemoryAccess
    {
        GPU_ONLY,
        CPU_WRITE
    };

    enum class EIndexFormat
    {
        U_INT16,
        U_INT32
    };

    enum class EVertexFormat
    {
        FLOAT2,
        FLOAT3,
        FLOAT4
    };

    enum class EVertexInputRate
    {
        VERTEX,
        INSTANCE
    };

    enum class EPrimitiveTopologyKind
    {
        TRIANGLE_LIST
    };

    enum class ECullModeKind
    {
        NONE,
        FRONT,
        BACK
    };

    enum class EFrontFaceKind
    {
        CLOCKWISE,
        COUNTER_CLOCKWISE
    };

    enum class EBlendModeKind
    {
        OPAQUE,
        ALPHA
    };

    enum class EDescriptorType
    {
        UNIFORM_BUFFER,
        SAMPLED_IMAGE,
        SAMPLER
    };

    enum class ETextureFormat
    {
        RGBA8,
        RGB_A8_SRGB,
        B10_G11_R11_UFLOAT,
        DEPTH32
    };

    enum class ETextureDimension
    {
        TEXTURE2_D,
        TEXTURE_CUBE
    };

    inline std::size_t textureFormatBytesPerPixel(ETextureFormat format) noexcept
    {
        switch (format)
        {
            case ETextureFormat::RGBA8:
            case ETextureFormat::RGB_A8_SRGB:
            case ETextureFormat::B10_G11_R11_UFLOAT:
            case ETextureFormat::DEPTH32:
                return 4;
            default:
                return 0;
        }
    }

    inline uint32_t textureFullMipLevelCount(uint32_t width, uint32_t height) noexcept
    {
        if (width == 0 || height == 0)
        {
            return 0;
        }

        uint32_t levels = 1;
        while (width > 1 || height > 1)
        {
            width = width > 1 ? width / 2 : 1;
            height = height > 1 ? height / 2 : 1;
            ++levels;
        }
        return levels;
    }

    enum class ESamplerFilter
    {
        NEAREST,
        LINEAR
    };

    enum class ESamplerMipFilter
    {
        NEAREST,
        LINEAR
    };

    enum class ESamplerAddressMode
    {
        CLAMP_TO_EDGE,
        REPEAT,
        MIRRORED_REPEAT
    };

    enum class EGltfAlphaMode
    {
        ALPHA_OPAQUE,
        ALPHA_MASK,
        ALPHA_BLEND
    };

    template <typename Tag>
    struct Handle
    {
        int32_t m_index = -1;
        uint32_t m_generation = 0;

        constexpr Handle() noexcept = default;
        constexpr Handle(int32_t index, uint32_t generation) noexcept : m_index(index), m_generation(generation) {}

        constexpr bool isValid() const noexcept
        {
            return m_index != -1;
        }
        constexpr bool operator==(const Handle&) const noexcept = default;
    };

    using ShaderModuleHandle = Handle<struct ShaderModuleTag>;
    using ShaderProgramHandle = Handle<struct ShaderProgramTag>;
    using BufferHandle = Handle<struct BufferTag>;
    using TextureHandle = Handle<struct TextureTag>;
    using SamplerHandle = Handle<struct SamplerTag>;
    using RenderTargetHandle = Handle<struct RenderTargetTag>;
    using GraphicsPipelineHandle = Handle<struct GraphicsPipelineTag>;
    using DescriptorSetHandle = Handle<struct DescriptorSetTag>;
    using FrameHandle = Handle<struct FrameTag>;

    struct ShaderModuleDesc
    {
        std::string path;
        std::string entry_point = "main";
        EShaderStage stage = EShaderStage::VERTEX;
        EShaderSourceKind source = EShaderSourceKind::SLANG_FILE;
        std::vector<std::string> search_paths;
        std::vector<uint32_t> spirv_code;
        std::string debug_name;
    };

    struct ShaderProgramDesc
    {
        std::vector<ShaderModuleDesc> stages;
        std::string debug_name;
    };

    struct GraphicsShaderProgramDesc
    {
        std::string path;
        std::string vertex_entry_point = "vertexMain";
        std::string fragment_entry_point = "fragmentMain";
        EShaderSourceKind source = EShaderSourceKind::SLANG_FILE;
        std::string debug_name;
    };

    struct BufferDesc
    {
        std::size_t size = 0;
        EBufferUsageKind usage = EBufferUsageKind::VERTEX;
        EMemoryAccess memory_access = EMemoryAccess::GPU_ONLY;
        std::string debug_name;
    };

    struct TextureDesc
    {
        uint32_t width = 0;
        uint32_t height = 0;
        ETextureFormat format = ETextureFormat::RGBA8;
        ETextureDimension dimension = ETextureDimension::TEXTURE2_D;
        // A value of 1 with generateMipmaps=true requests the full mip chain.
        uint32_t mip_levels = 1;
        // uploadTexture() uploads level 0 and generates the remaining levels when enabled.
        bool generate_mipmaps = false;
        bool render_target = false;
        bool sampled = true;
        bool transfer_src = false;
        bool transfer_dst = false;
        bool depth_stencil = false;
        uint32_t sample_count = 1;
        std::string debug_name;
    };

    inline constexpr uint32_t kMaxAttachementColorAttachements = 4;

    enum class EAttachementLoadOp
    {
        LOAD,
        CLEAR,
        DONT_CARE
    };

    enum class EAttachementStoreOp
    {
        STORE,
        DONT_CARE
    };

    struct AttachementTextureDesc
    {
        uint32_t width = 0;
        uint32_t height = 0;
        ETextureFormat format = ETextureFormat::RGBA8;
        bool color_attachement = false;
        bool depth_stencil_attachement = false;
        bool sampled = false;
        bool transfer_src = false;
        uint32_t sample_count = 1;
        std::string debug_name;
    };

    struct AttachementPipelineSignature
    {
        std::vector<ETextureFormat> color_formats;
        bool has_depth_attachement = false;
        ETextureFormat depth_format = ETextureFormat::DEPTH32;
        uint32_t sample_count = 1;

        bool matches(const AttachementPipelineSignature& other) const noexcept
        {
            if (color_formats.size() != other.color_formats.size())
            {
                return false;
            }

            for (std::size_t i = 0; i < color_formats.size(); ++i)
            {
                if (color_formats[i] != other.color_formats[i])
                {
                    return false;
                }
            }

            if (has_depth_attachement != other.has_depth_attachement)
            {
                return false;
            }

            if (has_depth_attachement && depth_format != other.depth_format)
            {
                return false;
            }

            if (sample_count != other.sample_count)
            {
                return false;
            }

            return true;
        }
    };

    struct SamplerDesc
    {
        ESamplerFilter min_filter = ESamplerFilter::LINEAR;
        ESamplerFilter mag_filter = ESamplerFilter::LINEAR;
        ESamplerMipFilter mip_filter = ESamplerMipFilter::LINEAR;
        ESamplerMipFilter mipmap_mop_filter = ESamplerMipFilter::LINEAR;
        ESamplerAddressMode address_mode_u = ESamplerAddressMode::CLAMP_TO_EDGE;
        ESamplerAddressMode address_mode_v = ESamplerAddressMode::CLAMP_TO_EDGE;
        ESamplerAddressMode address_mode_w = ESamplerAddressMode::CLAMP_TO_EDGE;
        float min_lod = 0.0f;
        float max_lod = 0.0f;
        // Values above one request anisotropic filtering, clamped to device limits by the backend.
        float max_anisotropy = 1.0f;
        std::string debug_name;
    };

    struct RenderTargetDesc
    {
        TextureHandle color_texture;
        TextureHandle depth_texture;
        std::string debug_name;
    };

    struct InstanceBufferDesc
    {
        std::size_t size = 0;
        EBufferUsageKind usage = EBufferUsageKind::VERTEX;
        EMemoryAccess memory_access = EMemoryAccess::GPU_ONLY;
        bool uses_attachement_redenring = false;
        AttachementPipelineSignature attachement_signature;
    };

    struct VertexBindingDesc
    {
        uint32_t binding = 0;
        uint32_t stride = 0;
        EVertexInputRate input_rate = EVertexInputRate::VERTEX;
    };

    struct VertexAttributeDesc
    {
        uint32_t location = 0;
        uint32_t binding = 0;
        uint32_t offset = 0;
        EVertexFormat format = EVertexFormat::FLOAT3;
    };

    struct VertexLayoutDesc
    {
        std::vector<VertexBindingDesc> bindings;
        std::vector<VertexAttributeDesc> attributes;
    };

    struct DescriptorBindingDesc
    {
        std::string name;
        uint32_t binding = 0;
        EDescriptorType type = EDescriptorType::UNIFORM_BUFFER;
        EShaderStage stage = EShaderStage::VERTEX;
        uint32_t count = 1;
    };

    struct DescriptorSetLayoutDesc
    {
        uint32_t set = 0;
        std::vector<DescriptorBindingDesc> bindings;
    };

    struct GraphicsPipelineDesc
    {
        EPrimitiveTopologyKind topology = EPrimitiveTopologyKind::TRIANGLE_LIST;
        ECullModeKind cull_mode = ECullModeKind::BACK;
        EFrontFaceKind front_face = EFrontFaceKind::CLOCKWISE;
        RenderTargetHandle render_target;
        VertexLayoutDesc vertex_layout;
        std::vector<DescriptorSetLayoutDesc> descriptor_sets;
        EBlendModeKind blend_mode = EBlendModeKind::OPAQUE;
        bool depth_test = false;
        bool depth_write = false;
        bool uses_attachement_rendering = false;
        AttachementPipelineSignature attachement_signature;
        std::string debug_name;
    };

    struct ClearColorValue
    {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 1.0f;
    };

    struct AttachementColorDesc
    {
        TextureHandle texture;
        EAttachementLoadOp load_op = EAttachementLoadOp::CLEAR;
        EAttachementStoreOp store_op = EAttachementStoreOp::STORE;
        ClearColorValue clear_color;
    };

    struct AttachementDepthDesc
    {
        TextureHandle texture;
        EAttachementLoadOp load_op = EAttachementLoadOp::CLEAR;
        EAttachementStoreOp store_op = EAttachementStoreOp::STORE;
        float clear_depth = 1.0f;
    };

    struct AttachementRenderingDesc
    {
        std::vector<AttachementColorDesc> color_attachements;
        bool has_depth_attachement = false;
        AttachementDepthDesc depth_attachement;
    };

    struct RenderPassDesc
    {
        ClearColorValue clear_color;
        float clear_depth = 1.0f;
    };

}  // namespace kera
