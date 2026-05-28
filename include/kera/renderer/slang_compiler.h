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
        std::string shaderPath;
        std::string entryPoint;
        ShaderType shaderType = ShaderType::Vertex;
        std::vector<std::string> searchPaths;
    };

    class SlangCompiler
    {
    public:
        static bool compileToSpirv(const SlangCompileRequest& request, std::vector<uint32_t>& outSpirv,
                                   std::string* outDiagnostics = nullptr);
        static bool compileToSpirvAndReflect(const SlangCompileRequest& request, std::vector<uint32_t>& outSpirv,
                                             SlangReflectionMetadata& outReflection,
                                             std::string* outDiagnostics = nullptr);

    private:
        SlangCompiler() = delete;
    };

}  // namespace kera
