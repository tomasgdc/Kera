// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/renderer/descriptors.h"
#include "kera/renderer/slang_reflection.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace kera
{

    struct ShaderStageContract
    {
        std::string path;
        std::string entryPoint;
        ShaderStage stage = ShaderStage::Vertex;
        ShaderSourceKind source = ShaderSourceKind::SlangFile;
    };

    struct ReflectedVertexBindingDesc
    {
        std::string name;
        uint32_t binding = 0;
        uint32_t stride = 0;
        VertexInputRate inputRate = VertexInputRate::Vertex;
    };

    struct ReflectedVertexSemanticDesc
    {
        std::string semanticName;
        std::string bindingName;
        uint32_t offset = 0;
        VertexFormat format = VertexFormat::Float3;
    };

    struct ReflectedDescriptorBindingDesc
    {
        std::string name;
        DescriptorType type = DescriptorType::UniformBuffer;
        std::size_t uniformSize = 0;
    };

    struct ReflectedPipelineContract
    {
        std::string debugName;
        std::string vertexEntryPoint;
        std::span<const ReflectedVertexBindingDesc> vertexBindings;
        std::span<const ReflectedVertexSemanticDesc> vertexSemantics;
        std::span<const ReflectedDescriptorBindingDesc> descriptors;
    };

    class PipelineReflectionContract
    {
    public:
        PipelineReflectionContract() = default;
        PipelineReflectionContract(std::string debugName, std::string vertexEntryPoint,
                                   std::vector<ReflectedVertexBindingDesc> vertexBindings,
                                   std::vector<ReflectedVertexSemanticDesc> vertexSemantics,
                                   std::vector<ReflectedDescriptorBindingDesc> descriptors);

        ReflectedPipelineContract view() const;
        operator ReflectedPipelineContract() const
        {
            return view();
        }
        bool empty() const
        {
            return m_debugName.empty() && m_vertexEntryPoint.empty() && m_vertexBindings.empty() &&
                   m_vertexSemantics.empty() && m_descriptors.empty();
        }

    private:
        std::string m_debugName;
        std::string m_vertexEntryPoint;
        std::vector<ReflectedVertexBindingDesc> m_vertexBindings;
        std::vector<ReflectedVertexSemanticDesc> m_vertexSemantics;
        std::vector<ReflectedDescriptorBindingDesc> m_descriptors;
    };

    // Compatibility alias for older sample code; prefer PipelineReflectionContract.
    using BuiltReflectedPipelineContract = PipelineReflectionContract;

    struct GraphicsPipelineCreateDesc
    {
        ShaderProgramHandle shaderProgram;
        PipelineReflectionContract reflectionContract;
        RenderTargetHandle renderTarget;
        PrimitiveTopologyKind topology = PrimitiveTopologyKind::TriangleList;
        CullModeKind cullMode = CullModeKind::Back;
        FrontFaceKind frontFace = FrontFaceKind::Clockwise;
        BlendModeKind blendMode = BlendModeKind::Opaque;
        bool depthTest = false;
        bool depthWrite = false;
        VertexLayoutDesc vertexLayout;
        std::vector<DescriptorSetLayoutDesc> descriptorSets;
        std::string debugName;
    };

    class ReflectedPipelineContractBuilder
    {
    public:
        ReflectedPipelineContractBuilder& debugName(std::string name);
        ReflectedPipelineContractBuilder& vertexEntry(std::string entryPoint);
        ReflectedPipelineContractBuilder& vertexBinding(std::string name, uint32_t binding, uint32_t stride,
                                                        VertexInputRate inputRate = VertexInputRate::Vertex);
        template <typename VertexT>
        ReflectedPipelineContractBuilder& vertexBinding(std::string name, uint32_t binding,
                                                        VertexInputRate inputRate = VertexInputRate::Vertex)
        {
            return vertexBinding(std::move(name), binding, static_cast<uint32_t>(sizeof(VertexT)), inputRate);
        }
        ReflectedPipelineContractBuilder& semantic(std::string semanticName, std::string bindingName, uint32_t offset,
                                                   VertexFormat format);
        ReflectedPipelineContractBuilder& descriptor(std::string name, DescriptorType type,
                                                     std::size_t uniformSize = 0);

        template <typename UniformT>
        ReflectedPipelineContractBuilder& uniform(std::string name)
        {
            return descriptor(std::move(name), DescriptorType::UniformBuffer, sizeof(UniformT));
        }

        ReflectedPipelineContractBuilder& sampledImage(std::string name);
        ReflectedPipelineContractBuilder& sampler(std::string name);
        PipelineReflectionContract build() const;

    private:
        std::string m_debugName;
        std::string m_vertexEntryPoint;
        std::vector<ReflectedVertexBindingDesc> m_vertexBindings;
        std::vector<ReflectedVertexSemanticDesc> m_vertexSemantics;
        std::vector<ReflectedDescriptorBindingDesc> m_descriptors;
    };

    // Public spelling for application code; ReflectedPipelineContractBuilder remains available.
    using PipelineReflectionBuilder = ReflectedPipelineContractBuilder;

    ShaderProgramDesc makeShaderProgramDesc(std::span<const ShaderStageContract> stages);
    const SlangReflectionBinding* requireReflectedDescriptorBinding(const SlangReflectionMetadata* reflection,
                                                                    const std::string& name, DescriptorType type);
    bool validateReflectedUniformSize(const SlangReflectionBinding& binding, std::size_t expectedSize);
    bool appendValidatedReflectedPipelineContract(VertexLayoutDesc& layout, const SlangReflectionMetadata* reflection,
                                                  const ReflectedPipelineContract& contract);

}  // namespace kera
