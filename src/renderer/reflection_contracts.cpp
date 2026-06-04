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
        bool descriptorTypeFromReflectionKind(SlangReflectionBindingKind kind, DescriptorType& outType)
        {
            switch (kind)
            {
                case SlangReflectionBindingKind::ParameterBlock:
                case SlangReflectionBindingKind::ConstantBuffer:
                    outType = DescriptorType::UniformBuffer;
                    return true;
                case SlangReflectionBindingKind::Resource:
                    outType = DescriptorType::SampledImage;
                    return true;
                case SlangReflectionBindingKind::SamplerState:
                    outType = DescriptorType::Sampler;
                    return true;
                default:
                    return false;
            }
        }

        std::optional<uint32_t> vertexFormatSize(VertexFormat format)
        {
            switch (format)
            {
                case VertexFormat::Float2:
                    return static_cast<uint32_t>(sizeof(float) * 2);
                case VertexFormat::Float3:
                    return static_cast<uint32_t>(sizeof(float) * 3);
                case VertexFormat::Float4:
                    return static_cast<uint32_t>(sizeof(float) * 4);
                default:
                    return std::nullopt;
            }
        }

        const ReflectedVertexBindingDesc* findVertexBinding(std::span<const ReflectedVertexBindingDesc> bindings,
                                                            uint32_t bindingSlot)
        {
            const auto found =
                std::find_if(bindings.begin(), bindings.end(), [bindingSlot](const ReflectedVertexBindingDesc& binding)
                             { return binding.binding == bindingSlot; });
            return found != bindings.end() ? &(*found) : nullptr;
        }

        std::string reflectedFieldLabel(const SlangReflectionInput& input)
        {
            if (!input.parameterName.empty())
            {
                return input.parameterName + "." + input.fieldName;
            }
            return input.fieldName;
        }

        bool fieldNamesEqual(const ReflectedVertexFieldDesc& field, const SlangReflectionInput& input)
        {
            return field.fieldName == input.fieldName;
        }

        bool fieldParametersEqual(const ReflectedVertexFieldDesc& field, const SlangReflectionInput& input)
        {
            return field.parameterName.empty() || field.parameterName == input.parameterName;
        }

        bool reflectedFieldNameIsAmbiguous(const SlangReflectionEntryPoint& entryPoint,
                                           const SlangReflectionInput& input)
        {
            std::size_t matches = 0;
            for (const SlangReflectionInput& reflectedInput : entryPoint.inputs)
            {
                if (reflectedInput.fieldName == input.fieldName)
                {
                    ++matches;
                }
            }
            return matches > 1;
        }

        const ReflectedVertexFieldDesc* findUniqueField(std::span<const ReflectedVertexFieldDesc> fields,
                                                        const SlangReflectionEntryPoint& entryPoint,
                                                        const SlangReflectionInput& input, std::size_t& outIndex,
                                                        RendererValidationReport& report, const std::string& prefix)
        {
            const ReflectedVertexFieldDesc* match = nullptr;
            std::size_t matchCount = 0;
            for (std::size_t index = 0; index < fields.size(); ++index)
            {
                if (fieldNamesEqual(fields[index], input) && fieldParametersEqual(fields[index], input))
                {
                    match = &fields[index];
                    outIndex = index;
                    ++matchCount;
                }
            }

            if (matchCount == 1)
            {
                return match;
            }
            if (matchCount > 1)
            {
                report.addIssue(prefix + "Multiple C++ vertex field mappings matched reflected shader field '" +
                                reflectedFieldLabel(input) + "'.");
                return nullptr;
            }

            if (reflectedFieldNameIsAmbiguous(entryPoint, input))
            {
                report.addIssue(prefix + "Reflected shader field '" + input.fieldName +
                                "' is ambiguous across vertex input parameters; provide parameterName '" +
                                input.parameterName + "'.");
                return nullptr;
            }

            report.addIssue(prefix + "No C++ vertex field mapping was provided for reflected shader field '" +
                            reflectedFieldLabel(input) + "' (semantic '" + input.semanticName + "').");
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

                const auto duplicateSlot =
                    std::count_if(bindings.begin(), bindings.end(), [&binding](const ReflectedVertexBindingDesc& other)
                                  { return other.binding == binding.binding; });
                if (duplicateSlot != 1)
                {
                    report.addIssue(prefix + "Vertex input layout contains duplicate binding slot " +
                                    std::to_string(binding.binding) + ".");
                    return false;
                }

                layout.bindings.push_back({
                    .binding = binding.binding,
                    .stride = binding.stride,
                    .inputRate = binding.inputRate,
                });
            }

            std::sort(layout.bindings.begin(), layout.bindings.end(),
                      [](const VertexBindingDesc& lhs, const VertexBindingDesc& rhs)
                      { return lhs.binding < rhs.binding; });
            return true;
        }

        std::string diagnosticPrefix(const VertexInputLayoutView& vertexInput)
        {
            return vertexInput.debugName.empty() ? std::string{} : std::string(vertexInput.debugName) + ": ";
        }

        const SlangReflectionEntryPoint* findVertexEntryPoint(const SlangReflectionMetadata& reflection,
                                                              RendererValidationReport& report,
                                                              const std::string& prefix)
        {
            const SlangReflectionEntryPoint* vertexEntryPoint = nullptr;
            for (const SlangReflectionEntryPoint& entryPoint : reflection.entryPoints)
            {
                if (entryPoint.stage != ShaderType::Vertex)
                {
                    continue;
                }
                if (vertexEntryPoint)
                {
                    report.addIssue(prefix +
                                    "Shader reflection exposed multiple vertex entry points; create one shader "
                                    "program per graphics vertex entry point.");
                    return nullptr;
                }
                vertexEntryPoint = &entryPoint;
            }

            if (!vertexEntryPoint)
            {
                report.addIssue(prefix + "Shader reflection did not expose a vertex entry point.");
                return nullptr;
            }
            return vertexEntryPoint;
        }

        bool buildValidatedVertexLayout(VertexLayoutDesc& layout, const SlangReflectionMetadata& reflection,
                                        const VertexInputLayoutView& vertexInput, RendererValidationReport& report)
        {
            const std::string prefix = diagnosticPrefix(vertexInput);
            if (!appendVertexBindings(layout, vertexInput.bindings, report, prefix))
            {
                return false;
            }

            const SlangReflectionEntryPoint* entryPoint = findVertexEntryPoint(reflection, report, prefix);
            if (!entryPoint)
            {
                return false;
            }

            std::vector<bool> usedFields(vertexInput.fields.size(), false);
            for (const SlangReflectionInput& input : entryPoint->inputs)
            {
                if (!input.hasFormat)
                {
                    report.addIssue(prefix + "Reflected shader field '" + reflectedFieldLabel(input) +
                                    "' uses an unsupported vertex input type.");
                    return false;
                }

                std::size_t fieldIndex = 0;
                const ReflectedVertexFieldDesc* field =
                    findUniqueField(vertexInput.fields, *entryPoint, input, fieldIndex, report, prefix);
                if (!field)
                {
                    return false;
                }
                if (usedFields[fieldIndex])
                {
                    report.addIssue(prefix + "C++ vertex field mapping '" + field->fieldName +
                                    "' was matched by more than one reflected shader input; provide parameterName.");
                    return false;
                }
                usedFields[fieldIndex] = true;

                if (field->fieldName.empty())
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

                const ReflectedVertexBindingDesc* binding = findVertexBinding(vertexInput.bindings, field->binding);
                if (!binding)
                {
                    report.addIssue(prefix + "Vertex field '" + reflectedFieldLabel(input) +
                                    "' references unknown vertex binding slot " + std::to_string(field->binding) + ".");
                    return false;
                }

                const std::optional<uint32_t> formatSize = vertexFormatSize(field->format);
                if (!formatSize)
                {
                    report.addIssue(prefix + "Vertex field '" + reflectedFieldLabel(input) +
                                    "' uses an unsupported vertex format.");
                    return false;
                }

                for (uint32_t index = 0; index < input.locationCount; ++index)
                {
                    layout.attributes.push_back({
                        .location = input.location + index,
                        .binding = binding->binding,
                        .offset = field->offset + *formatSize * index,
                        .format = field->format,
                    });
                }
            }

            for (std::size_t index = 0; index < usedFields.size(); ++index)
            {
                if (!usedFields[index])
                {
                    report.addIssue(prefix + "C++ vertex field mapping '" + vertexInput.fields[index].fieldName +
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
        m_debugName = std::move(name);
        return *this;
    }

    VertexInputLayoutBuilder& VertexInputLayoutBuilder::vertexBinding(uint32_t binding, uint32_t stride,
                                                                      VertexInputRate inputRate)
    {
        m_vertexBindings.push_back({
            .binding = binding,
            .stride = stride,
            .inputRate = inputRate,
        });
        return *this;
    }

    VertexInputLayoutBuilder& VertexInputLayoutBuilder::field(std::string fieldName, uint32_t binding, uint32_t offset,
                                                              VertexFormat format)
    {
        return fieldIn({}, std::move(fieldName), binding, offset, format);
    }

    VertexInputLayoutBuilder& VertexInputLayoutBuilder::fieldIn(std::string parameterName, std::string fieldName,
                                                                uint32_t binding, uint32_t offset, VertexFormat format)
    {
        m_vertexFields.push_back({
            .parameterName = std::move(parameterName),
            .fieldName = std::move(fieldName),
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
            .debugName = m_debugName,
            .bindings = m_vertexBindings,
            .fields = m_vertexFields,
        };

        VertexLayoutDesc vertexLayout;
        if (!buildValidatedVertexLayout(vertexLayout, reflection, view, report))
        {
            return VertexInputLayoutBuildResult::failure(std::move(report));
        }

        return VertexInputLayoutBuildResult::success(std::move(vertexLayout));
    }

    ShaderProgramDesc makeShaderProgramDesc(std::span<const ShaderStageContract> stages)
    {
        ShaderProgramDesc desc{};
        desc.stages.reserve(stages.size());
        for (const ShaderStageContract& stage : stages)
        {
            desc.stages.push_back({
                .path = stage.path,
                .entryPoint = stage.entryPoint,
                .stage = stage.stage,
                .source = stage.source,
            });
        }
        return desc;
    }

    const SlangReflectionBinding* requireReflectedDescriptorBinding(const SlangReflectionMetadata* reflection,
                                                                    const std::string& bindingName, DescriptorType type)
    {
        if (!reflection)
        {
            Logger::getInstance().error("Shader program reflection is missing while resolving descriptor binding '" +
                                        bindingName + "'.");
            return nullptr;
        }

        const SlangReflectionBinding* binding = reflection->findBinding(bindingName);
        if (!binding)
        {
            Logger::getInstance().error("Shader reflection did not expose descriptor binding '" + bindingName + "'.");
            return nullptr;
        }

        DescriptorType reflectedType = DescriptorType::UniformBuffer;
        if (!descriptorTypeFromReflectionKind(binding->kind, reflectedType) || reflectedType != type)
        {
            Logger::getInstance().error("Shader reflection descriptor binding '" + bindingName +
                                        "' does not match the expected resource type.");
            return nullptr;
        }

        return binding;
    }

    bool validateReflectedUniformSize(const SlangReflectionBinding& binding, std::size_t expectedSize)
    {
        if (binding.uniformSize == 0)
        {
            return true;
        }

        if (binding.uniformSize != expectedSize)
        {
            Logger::getInstance().error("Shader reflection uniform binding '" + binding.name + "' has size " +
                                        std::to_string(binding.uniformSize) + ", but the C++ struct is " +
                                        std::to_string(expectedSize) + " bytes.");
            return false;
        }

        return true;
    }

    VertexInputLayoutBuildResult buildValidatedVertexInputLayout(const SlangReflectionMetadata& reflection,
                                                                 VertexInputLayoutView vertexInput)
    {
        RendererValidationReport report;
        VertexLayoutDesc layout;
        if (!buildValidatedVertexLayout(layout, reflection, vertexInput, report))
        {
            return VertexInputLayoutBuildResult::failure(std::move(report));
        }
        return VertexInputLayoutBuildResult::success(std::move(layout));
    }

}  // namespace kera
