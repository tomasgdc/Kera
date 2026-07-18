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

    enum class ESlangReflectionBindingKind
    {
        UNKNOWN,
        PARAMETER_BLOCK,
        CONSTANT_BUFFER,
        RESOURCE,
        SAMPLER_STATE
    };

    struct SlangReflectionBinding
    {
        std::string name;
        ESlangReflectionBindingKind kind = ESlangReflectionBindingKind::UNKNOWN;
        EShaderType stage = EShaderType::VERTEX;
        uint32_t binding = 0;
        uint32_t space = 0;
        uint32_t count = 1;
        std::string type_name;
        std::size_t uniform_size = 0;
    };

    struct SlangReflectionInput
    {
        std::string name;
        std::string parameter_name;
        std::string field_name;
        std::string semantic_name;
        uint32_t location = 0;
        uint32_t location_count = 1;
        EVertexFormat format = EVertexFormat::FLOAT3;
        bool has_format = false;
    };

    struct SlangReflectionEntryPoint
    {
        std::string name;
        EShaderType stage = EShaderType::VERTEX;
        std::vector<SlangReflectionInput> inputs;
    };

    struct SlangReflectionMetadata
    {
        std::vector<SlangReflectionBinding> bindings;
        std::vector<SlangReflectionEntryPoint> entry_points;

        const SlangReflectionBinding* findBinding(const std::string& name) const;
        const SlangReflectionEntryPoint* findEntryPoint(const std::string& name) const;
    };

    bool parseSlangReflectionMetadata(const std::string& reflection_json, SlangReflectionMetadata& out_metadata,
                                      std::string* out_diagnostics = nullptr);

}  // namespace kera
