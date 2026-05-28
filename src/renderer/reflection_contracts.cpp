// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/reflection_contracts.h"

#include "kera/utilities/logger.h"

#include <algorithm>
#include <cctype>
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

        uint32_t vertexFormatSize(VertexFormat format)
        {
            switch (format)
            {
                case VertexFormat::Float2:
                    return sizeof(float) * 2;
                case VertexFormat::Float3:
                    return sizeof(float) * 3;
                case VertexFormat::Float4:
                    return sizeof(float) * 4;
                default:
                    return sizeof(float) * 3;
            }
        }

        std::string semanticBaseName(const std::string& semanticName)
        {
            std::string baseName = semanticName;
            while (!baseName.empty() && std::isdigit(static_cast<unsigned char>(baseName.back())))
            {
                baseName.pop_back();
            }
            return baseName;
        }

        const ReflectedVertexBindingDesc* findVertexBinding(std::span<const ReflectedVertexBindingDesc> bindings,
                                                            const std::string& name)
        {
            const auto found =
                std::find_if(bindings.begin(), bindings.end(),
                             [&name](const ReflectedVertexBindingDesc& binding) { return binding.name == name; });
            return found != bindings.end() ? &(*found) : nullptr;
        }

        const ReflectedVertexSemanticDesc* findUniqueSemantic(std::span<const ReflectedVertexSemanticDesc> semantics,
                                                              const std::string& semanticName, std::size_t& outIndex,
                                                              const std::string& prefix)
        {
            const ReflectedVertexSemanticDesc* match = nullptr;
            std::size_t matchCount = 0;
            for (std::size_t index = 0; index < semantics.size(); ++index)
            {
                if (semantics[index].semanticName == semanticName)
                {
                    match = &semantics[index];
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
                Logger::getInstance().error(prefix +
                                            "Multiple C++ vertex semantic mappings matched reflected semantic '" +
                                            semanticName + "'.");
                return nullptr;
            }

            const std::string reflectedBaseName = semanticBaseName(semanticName);
            for (std::size_t index = 0; index < semantics.size(); ++index)
            {
                if (semanticBaseName(semantics[index].semanticName) == reflectedBaseName)
                {
                    match = &semantics[index];
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
                Logger::getInstance().error(prefix +
                                            "Multiple C++ vertex semantic mappings matched reflected semantic base '" +
                                            reflectedBaseName + "'.");
                return nullptr;
            }

            Logger::getInstance().error(
                prefix + "No C++ vertex semantic mapping was provided for reflected semantic '" + semanticName + "'.");
            return nullptr;
        }

        bool appendVertexBindings(VertexLayoutDesc& layout, std::span<const ReflectedVertexBindingDesc> bindings,
                                  const std::string& prefix)
        {
            for (const ReflectedVertexBindingDesc& binding : bindings)
            {
                if (binding.name.empty())
                {
                    Logger::getInstance().error(prefix +
                                                "Reflected vertex binding contract contains an unnamed binding.");
                    return false;
                }

                const auto duplicateName =
                    std::count_if(bindings.begin(), bindings.end(), [&binding](const ReflectedVertexBindingDesc& other)
                                  { return other.name == binding.name; });
                const auto duplicateSlot =
                    std::count_if(bindings.begin(), bindings.end(), [&binding](const ReflectedVertexBindingDesc& other)
                                  { return other.binding == binding.binding; });
                if (duplicateName != 1 || duplicateSlot != 1)
                {
                    Logger::getInstance().error(
                        prefix + "Reflected vertex binding contract contains duplicate binding names or slots.");
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

        std::string diagnosticPrefix(const ReflectedPipelineContract& contract)
        {
            return contract.debugName.empty() ? std::string{} : contract.debugName + ": ";
        }
    }  // namespace

    PipelineReflectionContract::PipelineReflectionContract(std::string debugName, std::string vertexEntryPoint,
                                                           std::vector<ReflectedVertexBindingDesc> vertexBindings,
                                                           std::vector<ReflectedVertexSemanticDesc> vertexSemantics,
                                                           std::vector<ReflectedDescriptorBindingDesc> descriptors)
        : m_debugName(std::move(debugName))
        , m_vertexEntryPoint(std::move(vertexEntryPoint))
        , m_vertexBindings(std::move(vertexBindings))
        , m_vertexSemantics(std::move(vertexSemantics))
        , m_descriptors(std::move(descriptors))
    {
    }

    ReflectedPipelineContract PipelineReflectionContract::view() const
    {
        return {
            .debugName = m_debugName,
            .vertexEntryPoint = m_vertexEntryPoint,
            .vertexBindings = m_vertexBindings,
            .vertexSemantics = m_vertexSemantics,
            .descriptors = m_descriptors,
        };
    }

    ReflectedPipelineContractBuilder& ReflectedPipelineContractBuilder::debugName(std::string name)
    {
        m_debugName = std::move(name);
        return *this;
    }

    ReflectedPipelineContractBuilder& ReflectedPipelineContractBuilder::vertexEntry(std::string entryPoint)
    {
        m_vertexEntryPoint = std::move(entryPoint);
        return *this;
    }

    ReflectedPipelineContractBuilder& ReflectedPipelineContractBuilder::vertexBinding(std::string name,
                                                                                      uint32_t binding, uint32_t stride,
                                                                                      VertexInputRate inputRate)
    {
        m_vertexBindings.push_back({
            .name = std::move(name),
            .binding = binding,
            .stride = stride,
            .inputRate = inputRate,
        });
        return *this;
    }

    ReflectedPipelineContractBuilder& ReflectedPipelineContractBuilder::semantic(std::string semanticName,
                                                                                 std::string bindingName,
                                                                                 uint32_t offset, VertexFormat format)
    {
        m_vertexSemantics.push_back({
            .semanticName = std::move(semanticName),
            .bindingName = std::move(bindingName),
            .offset = offset,
            .format = format,
        });
        return *this;
    }

    ReflectedPipelineContractBuilder& ReflectedPipelineContractBuilder::descriptor(std::string name,
                                                                                   DescriptorType type,
                                                                                   std::size_t uniformSize)
    {
        m_descriptors.push_back({
            .name = std::move(name),
            .type = type,
            .uniformSize = uniformSize,
        });
        return *this;
    }

    ReflectedPipelineContractBuilder& ReflectedPipelineContractBuilder::sampledImage(std::string name)
    {
        return descriptor(std::move(name), DescriptorType::SampledImage);
    }

    ReflectedPipelineContractBuilder& ReflectedPipelineContractBuilder::sampler(std::string name)
    {
        return descriptor(std::move(name), DescriptorType::Sampler);
    }

    PipelineReflectionContract ReflectedPipelineContractBuilder::build() const
    {
        return {m_debugName, m_vertexEntryPoint, m_vertexBindings, m_vertexSemantics, m_descriptors};
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

    bool appendValidatedReflectedPipelineContract(VertexLayoutDesc& layout, const SlangReflectionMetadata* reflection,
                                                  const ReflectedPipelineContract& contract)
    {
        const std::string prefix = diagnosticPrefix(contract);
        if (!reflection)
        {
            Logger::getInstance().error(prefix +
                                        "Shader program reflection is missing while building reflected vertex layout.");
            return false;
        }

        if (!appendVertexBindings(layout, contract.vertexBindings, prefix))
        {
            return false;
        }

        const std::string entryPointName = contract.vertexEntryPoint;
        const SlangReflectionEntryPoint* entryPoint = reflection->findEntryPoint(entryPointName);
        if (!entryPoint)
        {
            Logger::getInstance().error(prefix + "Shader reflection did not expose entry point '" + entryPointName +
                                        "'.");
            return false;
        }

        std::vector<bool> usedSemantics(contract.vertexSemantics.size(), false);
        for (const SlangReflectionInput& input : entryPoint->inputs)
        {
            std::size_t semanticIndex = 0;
            const ReflectedVertexSemanticDesc* semantic =
                findUniqueSemantic(contract.vertexSemantics, input.semanticName, semanticIndex, prefix);
            if (!semantic)
            {
                return false;
            }
            usedSemantics[semanticIndex] = true;

            const ReflectedVertexBindingDesc* binding =
                findVertexBinding(contract.vertexBindings, semantic->bindingName);
            if (!binding)
            {
                Logger::getInstance().error(prefix + "Vertex semantic '" + semantic->semanticName +
                                            "' references unknown vertex binding '" + semantic->bindingName + "'.");
                return false;
            }

            const uint32_t formatSize = vertexFormatSize(semantic->format);
            for (uint32_t index = 0; index < input.locationCount; ++index)
            {
                layout.attributes.push_back({
                    .location = input.location + index,
                    .binding = binding->binding,
                    .offset = semantic->offset + formatSize * index,
                    .format = semantic->format,
                });
            }
        }

        for (std::size_t index = 0; index < usedSemantics.size(); ++index)
        {
            if (!usedSemantics[index])
            {
                Logger::getInstance().error(prefix + "C++ vertex semantic mapping '" +
                                            contract.vertexSemantics[index].semanticName +
                                            "' was not used by reflected shader inputs.");
                return false;
            }
        }

        std::sort(layout.attributes.begin(), layout.attributes.end(),
                  [](const VertexAttributeDesc& lhs, const VertexAttributeDesc& rhs)
                  { return lhs.location < rhs.location; });

        for (const ReflectedDescriptorBindingDesc& descriptor : contract.descriptors)
        {
            const SlangReflectionBinding* binding =
                requireReflectedDescriptorBinding(reflection, descriptor.name, descriptor.type);
            if (!binding)
            {
                return false;
            }

            if (descriptor.uniformSize != 0 && !validateReflectedUniformSize(*binding, descriptor.uniformSize))
            {
                return false;
            }
        }

        return true;
    }

}  // namespace kera
