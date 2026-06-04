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
        uint32_t binding = 0;
        uint32_t stride = 0;
        VertexInputRate inputRate = VertexInputRate::Vertex;
    };

    struct ReflectedVertexFieldDesc
    {
        std::string parameterName;
        std::string fieldName;
        uint32_t binding = 0;
        uint32_t offset = 0;
        VertexFormat format = VertexFormat::Float3;
    };

    struct VertexInputLayoutView
    {
        std::string_view debugName;
        std::span<const ReflectedVertexBindingDesc> bindings;
        std::span<const ReflectedVertexFieldDesc> fields;
    };

    class VertexInputLayoutBuildResult
    {
    public:
        static VertexInputLayoutBuildResult success(VertexLayoutDesc layout);
        static VertexInputLayoutBuildResult failure(RendererValidationReport report);

        bool ok() const noexcept;
        explicit operator bool() const noexcept
        {
            return ok();
        }
        const VertexLayoutDesc& layout() const noexcept;
        const RendererValidationReport& report() const noexcept;
        std::span<const RendererValidationIssue> issues() const noexcept;
        const std::string& errorMessage() const noexcept;

    private:
        VertexLayoutDesc m_layout;
        RendererValidationReport m_report;
        bool m_ok = false;
    };

    struct GraphicsPipelineCreateDesc
    {
        ShaderProgramHandle shaderProgram;
        RenderTargetHandle renderTarget;
        PrimitiveTopologyKind topology = PrimitiveTopologyKind::TriangleList;
        CullModeKind cullMode = CullModeKind::Back;
        FrontFaceKind frontFace = FrontFaceKind::Clockwise;
        BlendModeKind blendMode = BlendModeKind::Opaque;
        bool depthTest = false;
        bool depthWrite = false;
        std::vector<ReflectedVertexBindingDesc> vertexBindings;
        std::vector<ReflectedVertexFieldDesc> vertexFields;
        VertexLayoutDesc vertexLayout;
        std::vector<DescriptorSetLayoutDesc> descriptorSets;
        std::string debugName;
    };

    class VertexInputLayoutBuilder
    {
    public:
        VertexInputLayoutBuilder& debugName(std::string name);
        VertexInputLayoutBuilder& vertexBinding(uint32_t binding, uint32_t stride,
                                                VertexInputRate inputRate = VertexInputRate::Vertex);
        template <typename VertexT>
        VertexInputLayoutBuilder& vertexBinding(uint32_t binding, VertexInputRate inputRate = VertexInputRate::Vertex)
        {
            return vertexBinding(binding, static_cast<uint32_t>(sizeof(VertexT)), inputRate);
        }
        VertexInputLayoutBuilder& field(std::string fieldName, uint32_t binding, uint32_t offset, VertexFormat format);
        VertexInputLayoutBuilder& fieldIn(std::string parameterName, std::string fieldName, uint32_t binding,
                                          uint32_t offset, VertexFormat format);
        VertexInputLayoutBuildResult build(const SlangReflectionMetadata& reflection) &&;

    private:
        std::string m_debugName;
        std::vector<ReflectedVertexBindingDesc> m_vertexBindings;
        std::vector<ReflectedVertexFieldDesc> m_vertexFields;
    };

    ShaderProgramDesc makeShaderProgramDesc(std::span<const ShaderStageContract> stages);
    const SlangReflectionBinding* requireReflectedDescriptorBinding(const SlangReflectionMetadata* reflection,
                                                                    const std::string& name, DescriptorType type);
    bool validateReflectedUniformSize(const SlangReflectionBinding& binding, std::size_t expectedSize);
    VertexInputLayoutBuildResult buildValidatedVertexInputLayout(const SlangReflectionMetadata& reflection,
                                                                 VertexInputLayoutView vertexInput);

}  // namespace kera
