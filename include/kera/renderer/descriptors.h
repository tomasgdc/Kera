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
        Compute
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

    enum class DescriptorType
    {
        UniformBuffer,
        SampledImage,
        Sampler
    };

    enum class TextureFormat
    {
        RGBA8
    };

    enum class SamplerFilter
    {
        Nearest,
        Linear
    };

    enum class SamplerAddressMode
    {
        ClampToEdge,
        Repeat
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
    };

    struct ShaderProgramDesc
    {
        std::vector<ShaderModuleDesc> stages;
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
        bool renderTarget = false;
        bool sampled = true;
    };

    struct SamplerDesc
    {
        SamplerFilter minFilter = SamplerFilter::Linear;
        SamplerFilter magFilter = SamplerFilter::Linear;
        SamplerAddressMode addressModeU = SamplerAddressMode::ClampToEdge;
        SamplerAddressMode addressModeV = SamplerAddressMode::ClampToEdge;
    };

    struct RenderTargetDesc
    {
        TextureHandle colorTexture;
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
    };

}  // namespace kera
