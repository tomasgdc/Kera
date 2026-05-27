#pragma once

#include <array>
#include <cstdint>

#include <glm/glm.hpp>

#include <string>

namespace kera
{
    struct FullscreenTriangleVertex
    {
        glm::vec2 position;
        glm::vec2 uv;
    };

    std::string resolveShaderPath(const std::string& shaderPath);
    std::string resolveSampleAssetPath(const std::string& assetPath);
    const std::array<FullscreenTriangleVertex, 3>& fullscreenTriangleVertices();
    const std::array<uint16_t, 3>& fullscreenTriangleIndices();

}  // namespace kera
