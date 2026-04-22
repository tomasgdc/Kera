#pragma once

#include "kera/types.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace kera {

enum class GraphicsBackend {
    Vulkan
};

enum class ShaderStage {
    Vertex,
    Fragment,
    Compute
};

enum class BufferUsageKind {
    Vertex,
    Index,
    Uniform,
    Storage
};

enum class MemoryAccess {
    GpuOnly,
    CpuWrite
};

enum class IndexFormat {
    UInt16,
    UInt32
};

enum class VertexFormat {
    Float2,
    Float3,
    Float4
};

enum class PrimitiveTopologyKind {
    TriangleList
};

struct ShaderModuleDesc {
    std::string path;
    std::string entryPoint = "main";
    ShaderStage stage = ShaderStage::Vertex;
};

struct BufferDesc {
    std::size_t size = 0;
    BufferUsageKind usage = BufferUsageKind::Vertex;
    MemoryAccess memoryAccess = MemoryAccess::GpuOnly;
};

struct VertexBindingDesc {
    uint32_t binding = 0;
    uint32_t stride = 0;
};

struct VertexAttributeDesc {
    uint32_t location = 0;
    uint32_t binding = 0;
    uint32_t offset = 0;
    VertexFormat format = VertexFormat::Float3;
};

struct VertexLayoutDesc {
    std::vector<VertexBindingDesc> bindings;
    std::vector<VertexAttributeDesc> attributes;
};

struct GraphicsPipelineDesc {
    PrimitiveTopologyKind topology = PrimitiveTopologyKind::TriangleList;
    VertexLayoutDesc vertexLayout;
};

struct ClearColorValue {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

struct RenderPassDesc {
    ClearColorValue clearColor;
};

} // namespace kera
