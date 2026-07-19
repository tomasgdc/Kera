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
        std::string entry_point;
        EShaderStage stage = EShaderStage::VERTEX;
        EShaderSourceKind source = EShaderSourceKind::SLANG_FILE;
    };

    struct ReflectedVertexBindingDesc
    {
        uint32_t binding = 0;
        uint32_t stride = 0;
        EVertexInputRate input_rate = EVertexInputRate::VERTEX;
    };

    struct ReflectedVertexFieldDesc
    {
        std::string parameter_name;
        std::string field_name;
        uint32_t binding = 0;
        uint32_t offset = 0;
        EVertexFormat format = EVertexFormat::FLOAT3;
    };

    struct VertexInputLayoutView
    {
        std::string_view debug_name;
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
        ShaderProgramHandle shader_program;
        RenderTargetHandle render_target;
        EPrimitiveTopologyKind topology = EPrimitiveTopologyKind::TRIANGLE_LIST;
        ECullModeKind cull_mode = ECullModeKind::BACK;
        EFrontFaceKind front_face = EFrontFaceKind::CLOCKWISE;
        EBlendModeKind blend_mode = EBlendModeKind::OPAQUE;
        bool depth_test = false;
        bool depth_write = false;
        std::vector<ReflectedVertexBindingDesc> vertex_bindings;
        std::vector<ReflectedVertexFieldDesc> vertex_fields;
        VertexLayoutDesc vertex_layout;
        std::vector<DescriptorSetLayoutDesc> descriptor_sets;
        std::string debug_name;
    };

    class VertexInputLayoutBuilder
    {
    public:
        VertexInputLayoutBuilder& debugName(std::string name);
        VertexInputLayoutBuilder& vertexBinding(uint32_t binding, uint32_t stride,
                                                EVertexInputRate input_rate = EVertexInputRate::VERTEX);
        template <typename VertexT>
        VertexInputLayoutBuilder& vertexBinding(uint32_t binding,
                                                EVertexInputRate input_rate = EVertexInputRate::VERTEX)
        {
            return vertexBinding(binding, static_cast<uint32_t>(sizeof(VertexT)), input_rate);
        }
        VertexInputLayoutBuilder& field(std::string field_name, uint32_t binding, uint32_t offset,
                                        EVertexFormat format);
        VertexInputLayoutBuilder& fieldIn(std::string parameter_name, std::string field_name, uint32_t binding,
                                          uint32_t offset, EVertexFormat format);
        VertexInputLayoutBuildResult build(const SlangReflectionMetadata& reflection) &&;

    private:
        std::string m_debug_name;
        std::vector<ReflectedVertexBindingDesc> m_vertex_bindings;
        std::vector<ReflectedVertexFieldDesc> m_vertex_fields;
    };

    ShaderProgramDesc makeShaderProgramDesc(std::span<const ShaderStageContract> stages);
    const SlangReflectionBinding* requireReflectedDescriptorBinding(const SlangReflectionMetadata* reflection,
                                                                    const std::string& name, EDescriptorType type);
    bool validateReflectedUniformSize(const SlangReflectionBinding& binding, std::size_t expected_size);
    VertexInputLayoutBuildResult buildValidatedVertexInputLayout(const SlangReflectionMetadata& reflection,
                                                                 VertexInputLayoutView vertex_input);

}  // namespace kera
