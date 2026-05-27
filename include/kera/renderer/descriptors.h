#pragma once

#include "kera/types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace kera
{

    enum class GraphicsBackend
    {
        Vulkan
    };

    enum class ShaderStage
    {
        Vertex,
        Fragment,
        Compute,
        AllGraphics
    };

    enum class ShaderSourceKind
    {
        SlangFile,
        SpirvFile,
        SpirvBinary
    };

    enum class BufferUsageKind
    {
        Vertex,
        Index,
        Uniform,
        Storage
    };

    enum class MemoryAccess
    {
        GpuOnly,
        CpuWrite
    };

    enum class IndexFormat
    {
        UInt16,
        UInt32
    };

    enum class VertexFormat
    {
        Float2,
        Float3,
        Float4
    };

    enum class VertexInputRate
    {
        Vertex,
        Instance
    };

    enum class PrimitiveTopologyKind
    {
        TriangleList
    };

    enum class CullModeKind
    {
        None,
        Front,
        Back
    };

    enum class FrontFaceKind
    {
        Clockwise,
        CounterClockwise
    };

    enum class BlendModeKind
    {
        Opaque,
        Alpha
    };

    enum class DescriptorType
    {
        UniformBuffer,
        SampledImage,
        Sampler
    };

    enum class TextureFormat
    {
        RGBA8,
        RGBA8Srgb,
        Depth32
    };

    inline std::size_t textureFormatBytesPerPixel(TextureFormat format) noexcept
    {
        switch (format)
        {
            case TextureFormat::RGBA8:
            case TextureFormat::RGBA8Srgb:
                return 4;
            case TextureFormat::Depth32:
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

    enum class SamplerFilter
    {
        Nearest,
        Linear
    };

    enum class SamplerMipFilter
    {
        Nearest,
        Linear
    };

    enum class SamplerAddressMode
    {
        ClampToEdge,
        Repeat,
        MirroredRepeat
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
        std::string entryPoint = "main";
        ShaderStage stage = ShaderStage::Vertex;
        ShaderSourceKind source = ShaderSourceKind::SlangFile;
        std::vector<std::string> searchPaths;
        std::vector<uint32_t> spirvCode;
        std::string debugName;
    };

    struct ShaderProgramDesc
    {
        std::vector<ShaderModuleDesc> stages;
    struct GraphicsShaderProgramDesc
    {
        std::string path;
        std::string vertexEntryPoint = "vertexMain";
        std::string fragmentEntryPoint = "fragmentMain";
        ShaderSourceKind source = ShaderSourceKind::SlangFile;
        std::string debugName;
    };

    struct BufferDesc
    {
        std::size_t size = 0;
        BufferUsageKind usage = BufferUsageKind::Vertex;
        MemoryAccess memoryAccess = MemoryAccess::GpuOnly;
    };

    struct TextureDesc
    {
        uint32_t width = 0;
        uint32_t height = 0;
        TextureFormat format = TextureFormat::RGBA8;
        // A value of 1 with generateMipmaps=true requests the full mip chain.
        uint32_t mipLevels = 1;
        // uploadTexture() uploads level 0 and generates the remaining levels when enabled.
        bool generateMipmaps = false;
        bool renderTarget = false;
        bool sampled = true;
        bool depthStencil = false;
    };

    struct SamplerDesc
    {
        SamplerFilter minFilter = SamplerFilter::Linear;
        SamplerFilter magFilter = SamplerFilter::Linear;
        SamplerMipFilter mipFilter = SamplerMipFilter::Linear;
        SamplerAddressMode addressModeU = SamplerAddressMode::ClampToEdge;
        SamplerAddressMode addressModeV = SamplerAddressMode::ClampToEdge;
        float minLod = 0.0f;
        float maxLod = 0.0f;
        // Values above one request anisotropic filtering, clamped to device limits by the backend.
        float maxAnisotropy = 1.0f;
    };

    struct RenderTargetDesc
    {
        TextureHandle colorTexture;
        TextureHandle depthTexture;
    };

    struct InstanceBufferDesc
    {
        std::size_t size = 0;
        BufferUsageKind usage = BufferUsageKind::Vertex;
        MemoryAccess memoryAccess = MemoryAccess::GpuOnly;
    };

    struct VertexBindingDesc
    {
        uint32_t binding = 0;
        uint32_t stride = 0;
        VertexInputRate inputRate = VertexInputRate::Vertex;
    };

    struct VertexAttributeDesc
    {
        uint32_t location = 0;
        uint32_t binding = 0;
        uint32_t offset = 0;
        VertexFormat format = VertexFormat::Float3;
    };

    struct VertexLayoutDesc
    {
        std::vector<VertexBindingDesc> bindings;
        std::vector<VertexAttributeDesc> attributes;
    };

    struct DescriptorBindingDesc
    {
        uint32_t binding = 0;
        DescriptorType type = DescriptorType::UniformBuffer;
        ShaderStage stage = ShaderStage::Vertex;
        uint32_t count = 1;
    };

    struct DescriptorSetLayoutDesc
    {
        uint32_t set = 0;
        std::vector<DescriptorBindingDesc> bindings;
    };

    struct GraphicsPipelineDesc
    {
        PrimitiveTopologyKind topology = PrimitiveTopologyKind::TriangleList;
        CullModeKind cullMode = CullModeKind::Back;
        FrontFaceKind frontFace = FrontFaceKind::Clockwise;
        RenderTargetHandle renderTarget;
        VertexLayoutDesc vertexLayout;
        std::vector<DescriptorSetLayoutDesc> descriptorSets;
        BlendModeKind blendMode = BlendModeKind::Opaque;
        bool depthTest = false;
        bool depthWrite = false;
    };

    struct ClearColorValue
    {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 1.0f;
    };

    struct RenderPassDesc
    {
        ClearColorValue clearColor;
        float clearDepth = 1.0f;
    };

}  // namespace kera
