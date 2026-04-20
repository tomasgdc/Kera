#pragma once

#include "kera/renderer/shader.h"
#include <string>
#include <vector>

namespace kera {

struct SlangCompileRequest {
    std::string shaderPath;
    std::string entryPoint;
    ShaderType shaderType = ShaderType::Vertex;
    std::vector<std::string> searchPaths;
};

class SlangCompiler {
public:
    static bool compileToSpirv(
        const SlangCompileRequest& request,
        std::vector<uint32_t>& outSpirv,
        std::string* outDiagnostics = nullptr);

private:
    SlangCompiler() = delete;
};

} // namespace kera
