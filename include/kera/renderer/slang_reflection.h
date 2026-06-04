// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/renderer/descriptors.h"
#include "kera/renderer/shader.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace kera
{

    enum class SlangReflectionBindingKind
    {
        Unknown,
        ParameterBlock,
        ConstantBuffer,
        Resource,
        SamplerState
    };

    struct SlangReflectionBinding
    {
        std::string name;
        SlangReflectionBindingKind kind = SlangReflectionBindingKind::Unknown;
        ShaderType stage = ShaderType::Vertex;
        uint32_t binding = 0;
        uint32_t space = 0;
        uint32_t count = 1;
        std::string typeName;
        std::size_t uniformSize = 0;
    };

    struct SlangReflectionInput
    {
        std::string name;
        std::string parameterName;
        std::string fieldName;
        std::string semanticName;
        uint32_t location = 0;
        uint32_t locationCount = 1;
        VertexFormat format = VertexFormat::Float3;
        bool hasFormat = false;
    };

    struct SlangReflectionEntryPoint
    {
        std::string name;
        ShaderType stage = ShaderType::Vertex;
        std::vector<SlangReflectionInput> inputs;
    };

    struct SlangReflectionMetadata
    {
        std::vector<SlangReflectionBinding> bindings;
        std::vector<SlangReflectionEntryPoint> entryPoints;

        const SlangReflectionBinding* findBinding(const std::string& name) const;
        const SlangReflectionEntryPoint* findEntryPoint(const std::string& name) const;
    };

    bool parseSlangReflectionMetadata(const std::string& reflectionJson, SlangReflectionMetadata& outMetadata,
                                      std::string* outDiagnostics = nullptr);

}  // namespace kera
