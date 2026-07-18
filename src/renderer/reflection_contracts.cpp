// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/reflection_contracts.h"

#include "kera/utilities/logger.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace kera
{
    namespace
    {
        bool descriptorTypeFromReflectionKind(ESlangReflectionBindingKind kind, EDescriptorType& out_type)
        {
            switch (kind)
            {
                case ESlangReflectionBindingKind::PARAMETER_BLOCK:
                case ESlangReflectionBindingKind::CONSTANT_BUFFER:
                    out_type = EDescriptorType::UNIFORM_BUFFER;
                    return true;
                case ESlangReflectionBindingKind::RESOURCE:
                    out_type = EDescriptorType::SAMPLED_IMAGE;
                    return true;
                case ESlangReflectionBindingKind::SAMPLER_STATE:
                    out_type = EDescriptorType::SAMPLER;
                    return true;
                default:
                    return false;
            }
        }

        std::optional<uint32_t> vertexFormatSize(EVertexFormat format)
        {
            switch (format)
            {
                case EVertexFormat::FLOAT2:
                    return static_cast<uint32_t>(sizeof(float) * 2);
                case EVertexFormat::FLOAT3:
                    return static_cast<uint32_t>(sizeof(float) * 3);
                case EVertexFormat::FLOAT4:
                    return static_cast<uint32_t>(sizeof(float) * 4);
                default:
                    return std::nullopt;
            }
        }

        const ReflectedVertexBindingDesc* findVertexBinding(std::span<const ReflectedVertexBindingDesc> bindings,
                                                            uint32_t binding_slot)
        {
            const auto found =
                std::find_if(bindings.begin(), bindings.end(), [binding_slot](const ReflectedVertexBindingDesc& binding)
                             { return binding.binding == binding_slot; });
            return found != bindings.end() ? &(*found) : nullptr;
        }

        std::string reflectedFieldLabel(const SlangReflectionInput& input)
        {
            if (!input.parameter_name.empty())
            {
                return input.parameter_name + "." + input.field_name;
            }
            return input.field_name;
        }

        bool fieldNamesEqual(const ReflectedVertexFieldDesc& field, const SlangReflectionInput& input)
        {
            return field.field_name == input.field_name;
        }

        bool fieldParametersEqual(const ReflectedVertexFieldDesc& field, const SlangReflectionInput& input)
        {
            return field.parameter_name.empty() || field.parameter_name == input.parameter_name;
        }

        bool reflectedFieldNameIsAmbiguous(const SlangReflectionEntryPoint& entry_point,
                                           const SlangReflectionInput& input)
        {
            std::size_t matches = 0;
            for (const SlangReflectionInput& reflected_input : entry_point.inputs)
            {
                if (reflected_input.field_name == input.field_name)
                {
                    ++matches;
                }
            }
            return matches > 1;
        }

        const ReflectedVertexFieldDesc* findUniqueField(std::span<const ReflectedVertexFieldDesc> fields,
                                                        const SlangReflectionEntryPoint& entry_point,
                                                        const SlangReflectionInput& input, std::size_t& out_index,
                                                        RendererValidationReport& report, const std::string& prefix)
        {
            const ReflectedVertexFieldDesc* match = nullptr;
            std::size_t match_count = 0;
            for (std::size_t index = 0; index < fields.size(); ++index)
            {
                if (fieldNamesEqual(fields[index], input) && fieldParametersEqual(fields[index], input))
                {
                    match = &fields[index];
                    out_index = index;
                    ++match_count;
                }
            }

            if (match_count == 1)
            {
                return match;
            }
            if (match_count > 1)
            {
                report.addIssue(prefix + "Multiple C++ vertex field mappings matched reflected shader field '" +
                                reflectedFieldLabel(input) + "'.");
                return nullptr;
            }

            if (reflectedFieldNameIsAmbiguous(entry_point, input))
            {
                report.addIssue(prefix + "Reflected shader field '" + input.field_name +
                                "' is ambiguous across vertex input parameters; provide parameterName '" +
                                input.parameter_name + "'.");
                return nullptr;
            }

            report.addIssue(prefix + "No C++ vertex field mapping was provided for reflected shader field '" +
                            reflectedFieldLabel(input) + "' (semantic '" + input.semantic_name + "').");
            return nullptr;
        }

        bool appendVertexBindings(VertexLayoutDesc& layout, std::span<const ReflectedVertexBindingDesc> bindings,
                                  RendererValidationReport& report, const std::string& prefix)
        {
            for (const ReflectedVertexBindingDesc& binding : bindings)
            {
                if (binding.stride == 0)
                {
                    report.addIssue(prefix + "Vertex input layout contains zero-stride binding slot " +
                                    std::to_string(binding.binding) + ".");
                    return false;
                }

                const auto duplicate_slot =
                    std::count_if(bindings.begin(), bindings.end(), [&binding](const ReflectedVertexBindingDesc& other)
                                  { return other.binding == binding.binding; });
                if (duplicate_slot != 1)
                {
                    report.addIssue(prefix + "Vertex input layout contains duplicate binding slot " +
                                    std::to_string(binding.binding) + ".");
                    return false;
                }

                layout.bindings.push_back({
                    .binding = binding.binding,
                    .stride = binding.stride,
                    .input_rate = binding.input_rate,
                });
            }

            std::sort(layout.bindings.begin(), layout.bindings.end(),
                      [](const VertexBindingDesc& lhs, const VertexBindingDesc& rhs)
                      { return lhs.binding < rhs.binding; });
            return true;
        }

        std::string diagnosticPrefix(const VertexInputLayoutView& vertex_input)
        {
            return vertex_input.debug_name.empty() ? std::string{} : std::string(vertex_input.debug_name) + ": ";
        }

        const SlangReflectionEntryPoint* findVertexEntryPoint(const SlangReflectionMetadata& reflection,
                                                              RendererValidationReport& report,
                                                              const std::string& prefix)
        {
            const SlangReflectionEntryPoint* vertex_entry_point = nullptr;
            for (const SlangReflectionEntryPoint& entry_point : reflection.entry_points)
            {
                if (entry_point.stage != EShaderType::VERTEX)
                {
                    continue;
                }
                if (vertex_entry_point)
                {
                    report.addIssue(prefix +
                                    "Shader reflection exposed multiple vertex entry points; create one shader "
                                    "program per graphics vertex entry point.");
                    return nullptr;
                }
                vertex_entry_point = &entry_point;
            }

            if (!vertex_entry_point)
            {
                report.addIssue(prefix + "Shader reflection did not expose a vertex entry point.");
                return nullptr;
            }
            return vertex_entry_point;
        }

        bool buildValidatedVertexLayout(VertexLayoutDesc& layout, const SlangReflectionMetadata& reflection,
                                        const VertexInputLayoutView& vertex_input, RendererValidationReport& report)
        {
            const std::string prefix = diagnosticPrefix(vertex_input);
            if (!appendVertexBindings(layout, vertex_input.bindings, report, prefix))
            {
                return false;
            }

            const SlangReflectionEntryPoint* entry_point = findVertexEntryPoint(reflection, report, prefix);
            if (!entry_point)
            {
                return false;
            }

            std::vector<bool> used_fields(vertex_input.fields.size(), false);
            for (const SlangReflectionInput& input : entry_point->inputs)
            {
                if (!input.has_format)
                {
                    report.addIssue(prefix + "Reflected shader field '" + reflectedFieldLabel(input) +
                                    "' uses an unsupported vertex input type.");
                    return false;
                }

                std::size_t field_index = 0;
                const ReflectedVertexFieldDesc* field =
                    findUniqueField(vertex_input.fields, *entry_point, input, field_index, report, prefix);
                if (!field)
                {
                    return false;
                }
                if (used_fields[field_index])
                {
                    report.addIssue(prefix + "C++ vertex field mapping '" + field->field_name +
                                    "' was matched by more than one reflected shader input; provide parameterName.");
                    return false;
                }
                used_fields[field_index] = true;

                if (field->field_name.empty())
                {
                    report.addIssue(prefix + "Vertex input layout contains an unnamed field mapping.");
                    return false;
                }

                if (field->format != input.format)
                {
                    report.addIssue(prefix + "Vertex field '" + reflectedFieldLabel(input) +
                                    "' has the wrong format for the reflected shader input.");
                    return false;
                }

                const ReflectedVertexBindingDesc* binding = findVertexBinding(vertex_input.bindings, field->binding);
                if (!binding)
                {
                    report.addIssue(prefix + "Vertex field '" + reflectedFieldLabel(input) +
                                    "' references unknown vertex binding slot " + std::to_string(field->binding) + ".");
                    return false;
                }

                const std::optional<uint32_t> format_size = vertexFormatSize(field->format);
                if (!format_size)
                {
                    report.addIssue(prefix + "Vertex field '" + reflectedFieldLabel(input) +
                                    "' uses an unsupported vertex format.");
                    return false;
                }

                for (uint32_t index = 0; index < input.location_count; ++index)
                {
                    layout.attributes.push_back({
                        .location = input.location + index,
                        .binding = binding->binding,
                        .offset = field->offset + *format_size * index,
                        .format = field->format,
                    });
                }
            }

            for (std::size_t index = 0; index < used_fields.size(); ++index)
            {
                if (!used_fields[index])
                {
                    report.addIssue(prefix + "C++ vertex field mapping '" + vertex_input.fields[index].field_name +
                                    "' was not used by reflected shader inputs.");
                    return false;
                }
            }

            std::sort(layout.attributes.begin(), layout.attributes.end(),
                      [](const VertexAttributeDesc& lhs, const VertexAttributeDesc& rhs)
                      { return lhs.location < rhs.location; });
            return true;
        }
    }  // namespace

    VertexInputLayoutBuildResult VertexInputLayoutBuildResult::success(VertexLayoutDesc layout)
    {
        VertexInputLayoutBuildResult result;
        result.m_layout = std::move(layout);
        result.m_ok = true;
        return result;
    }

    VertexInputLayoutBuildResult VertexInputLayoutBuildResult::failure(RendererValidationReport report)
    {
        VertexInputLayoutBuildResult result;
        result.m_report = std::move(report);
        return result;
    }

    bool VertexInputLayoutBuildResult::ok() const noexcept
    {
        return m_ok;
    }

    const VertexLayoutDesc& VertexInputLayoutBuildResult::layout() const noexcept
    {
        return m_layout;
    }

    const RendererValidationReport& VertexInputLayoutBuildResult::report() const noexcept
    {
        return m_report;
    }

    std::span<const RendererValidationIssue> VertexInputLayoutBuildResult::issues() const noexcept
    {
        return m_report.issues;
    }

    const std::string& VertexInputLayoutBuildResult::errorMessage() const noexcept
    {
        return m_report.errorMessage();
    }

    VertexInputLayoutBuilder& VertexInputLayoutBuilder::debugName(std::string name)
    {
        m_debug_name = std::move(name);
        return *this;
    }

    VertexInputLayoutBuilder& VertexInputLayoutBuilder::vertexBinding(uint32_t binding, uint32_t stride,
                                                                       EVertexInputRate input_rate)
    {
        m_vertex_bindings.push_back({
            .binding = binding,
            .stride = stride,
            .input_rate = input_rate,
        });
        return *this;
    }

    VertexInputLayoutBuilder& VertexInputLayoutBuilder::field(std::string field_name, uint32_t binding, uint32_t offset,
                                                              EVertexFormat format)
    {
        return fieldIn({}, std::move(field_name), binding, offset, format);
    }

    VertexInputLayoutBuilder& VertexInputLayoutBuilder::fieldIn(std::string parameter_name, std::string field_name,
                                                                uint32_t binding, uint32_t offset, EVertexFormat format)
    {
        m_vertex_fields.push_back({
            .parameter_name = std::move(parameter_name),
            .field_name = std::move(field_name),
            .binding = binding,
            .offset = offset,
            .format = format,
        });
        return *this;
    }

    VertexInputLayoutBuildResult VertexInputLayoutBuilder::build(const SlangReflectionMetadata& reflection) &&
    {
        RendererValidationReport report;
        VertexInputLayoutView view{
            .debug_name = m_debug_name,
            .bindings = m_vertex_bindings,
            .fields = m_vertex_fields,
        };

        VertexLayoutDesc vertex_layout;
        if (!buildValidatedVertexLayout(vertex_layout, reflection, view, report))
        {
            return VertexInputLayoutBuildResult::failure(std::move(report));
        }

        return VertexInputLayoutBuildResult::success(std::move(vertex_layout));
    }

    ShaderProgramDesc makeShaderProgramDesc(std::span<const ShaderStageContract> stages)
    {
        ShaderProgramDesc desc{};
        desc.stages.reserve(stages.size());
        for (const ShaderStageContract& stage : stages)
        {
            desc.stages.push_back({
                .path = stage.path,
                .entry_point = stage.entry_point,
                .stage = stage.stage,
                .source = stage.source,
                .search_paths = {},
                .spirv_code = {},
                .debug_name = {},
            });
        }
        return desc;
    }

    const SlangReflectionBinding* requireReflectedDescriptorBinding(const SlangReflectionMetadata* reflection,
                                                                    const std::string& binding_name, EDescriptorType type)
    {
        if (!reflection)
        {
            Logger::getInstance().error("Shader program reflection is missing while resolving descriptor binding '" +
                                        binding_name + "'.");
            return nullptr;
        }

        const SlangReflectionBinding* binding = reflection->findBinding(binding_name);
        if (!binding)
        {
            Logger::getInstance().error("Shader reflection did not expose descriptor binding '" + binding_name + "'.");
            return nullptr;
        }

        EDescriptorType reflected_type = EDescriptorType::UNIFORM_BUFFER;
        if (!descriptorTypeFromReflectionKind(binding->kind, reflected_type) || reflected_type != type)
        {
            Logger::getInstance().error("Shader reflection descriptor binding '" + binding_name +
                                        "' does not match the expected resource type.");
            return nullptr;
        }

        return binding;
    }

    bool validateReflectedUniformSize(const SlangReflectionBinding& binding, std::size_t expected_size)
    {
        if (binding.uniform_size == 0)
        {
            return true;
        }

        if (binding.uniform_size != expected_size)
        {
            Logger::getInstance().error("Shader reflection uniform binding '" + binding.name + "' has size " +
                                        std::to_string(binding.uniform_size) + ", but the C++ struct is " +
                                        std::to_string(expected_size) + " bytes.");
            return false;
        }

        return true;
    }

    VertexInputLayoutBuildResult buildValidatedVertexInputLayout(const SlangReflectionMetadata& reflection,
                                                                 VertexInputLayoutView vertex_input)
    {
        RendererValidationReport report;
        VertexLayoutDesc layout;
        if (!buildValidatedVertexLayout(layout, reflection, vertex_input, report))
        {
            return VertexInputLayoutBuildResult::failure(std::move(report));
        }
        return VertexInputLayoutBuildResult::success(std::move(layout));
    }

}  // namespace kera
