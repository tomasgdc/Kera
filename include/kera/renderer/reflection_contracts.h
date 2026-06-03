// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/renderer/descriptors.h"
#include "kera/renderer/result.h"
#include "kera/renderer/slang_reflection.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
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

    struct PipelineReflectionView
    {
        std::string_view debugName;
        std::string_view vertexEntryPoint;
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
                                   std::vector<ReflectedDescriptorBindingDesc> descriptors,
                                   VertexLayoutDesc vertexLayout = {});

        PipelineReflectionView view() const;
        const VertexLayoutDesc& vertexLayout() const;
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
        VertexLayoutDesc m_vertexLayout;
    };

    class PipelineReflectionBuildResult
    {
    public:
        static PipelineReflectionBuildResult success(PipelineReflectionContract contract);
        static PipelineReflectionBuildResult failure(RendererValidationReport report);

        bool ok() const noexcept;
        explicit operator bool() const noexcept
        {
            return ok();
        }
        const PipelineReflectionContract& contract() const noexcept;
        const RendererValidationReport& report() const noexcept;
        std::span<const RendererValidationIssue> issues() const noexcept;
        const std::string& errorMessage() const noexcept;

    private:
        PipelineReflectionContract m_contract;
        RendererValidationReport m_report;
        bool m_ok = false;
    };

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

    class PipelineReflectionBuilder
    {
    public:
        PipelineReflectionBuilder& debugName(std::string name);
        PipelineReflectionBuilder& vertexEntry(std::string entryPoint);
        PipelineReflectionBuilder& vertexBinding(std::string name, uint32_t binding, uint32_t stride,
                                                 VertexInputRate inputRate = VertexInputRate::Vertex);
        template <typename VertexT>
        PipelineReflectionBuilder& vertexBinding(std::string name, uint32_t binding,
                                                 VertexInputRate inputRate = VertexInputRate::Vertex)
        {
            return vertexBinding(std::move(name), binding, static_cast<uint32_t>(sizeof(VertexT)), inputRate);
        }
        // Semantic matching first tries the exact shader semantic, then falls back to a unique base name match
        // with trailing digits stripped (for example, POSITION0 can match POSITION).
        PipelineReflectionBuilder& semantic(std::string semanticName, std::string bindingName, uint32_t offset,
                                            VertexFormat format);
        PipelineReflectionBuilder& descriptor(std::string name, DescriptorType type, std::size_t uniformSize = 0);

        template <typename UniformT>
        PipelineReflectionBuilder& uniform(std::string name)
        {
            return descriptor(std::move(name), DescriptorType::UniformBuffer, sizeof(UniformT));
        }

        PipelineReflectionBuilder& sampledImage(std::string name);
        PipelineReflectionBuilder& sampler(std::string name);
        PipelineReflectionBuildResult build(const SlangReflectionMetadata& reflection) &&;

    private:
        std::string m_debugName;
        std::string m_vertexEntryPoint;
        std::vector<ReflectedVertexBindingDesc> m_vertexBindings;
        std::vector<ReflectedVertexSemanticDesc> m_vertexSemantics;
        std::vector<ReflectedDescriptorBindingDesc> m_descriptors;
    };

    ShaderProgramDesc makeShaderProgramDesc(std::span<const ShaderStageContract> stages);
    const SlangReflectionBinding* requireReflectedDescriptorBinding(const SlangReflectionMetadata* reflection,
                                                                    const std::string& name, DescriptorType type);
    bool validateReflectedUniformSize(const SlangReflectionBinding& binding, std::size_t expectedSize);
    bool appendValidatedPipelineReflectionContract(VertexLayoutDesc& layout,
                                                   const PipelineReflectionContract& contract);

}  // namespace kera
