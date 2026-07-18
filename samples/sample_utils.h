// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/renderer/api.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <string>

namespace kera
{
    struct FullscreenTriangleVertex
    {
        glm::vec2 position;
        glm::vec2 uv;
    };

    std::string resolveShaderPath(const std::string& shader_path);
    std::string resolveSampleAssetPath(const std::string& asset_path);
    KeraStringView sampleStringView(const std::string& text);
    void sampleLog(KeraLogLevel level, const std::string& message);
    void sampleLogDebug(const std::string& message);
    void sampleLogInfo(const std::string& message);
    void sampleLogWarning(const std::string& message);
    void sampleLogError(const std::string& message);
    const std::array<FullscreenTriangleVertex, 3>& fullscreenTriangleVertices();
    const std::array<uint16_t, 3>& fullscreenTriangleIndices();

}  // namespace kera
