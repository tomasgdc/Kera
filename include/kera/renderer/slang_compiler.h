// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/renderer/shader.h"
#include "kera/renderer/slang_reflection.h"

#include <string>
#include <vector>

namespace kera
{

    struct SlangCompileRequest
    {
        std::string shader_path;
        std::string entry_point;
        EShaderType shader_type = EShaderType::VERTEX;
        std::vector<std::string> search_paths;
    };

    class SlangCompiler
    {
    public:
        static bool compileToSpirv(const SlangCompileRequest& request, std::vector<uint32_t>& out_spirv,
                                   std::string* out_diagnostics = nullptr);
        static bool compileToSpirvAndReflect(const SlangCompileRequest& request, std::vector<uint32_t>& out_spirv,
                                             SlangReflectionMetadata& out_reflection,
                                             std::string* out_diagnostics = nullptr);

    private:
        SlangCompiler() = delete;
    };

}  // namespace kera
