// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "sample_utils.h"

#include <filesystem>

namespace kera
{
    namespace
    {
        bool isRegularFile(const std::filesystem::path& path)
        {
            return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
        }

        std::string resolveSampleFilePath(const std::string& path)
        {
            if (isRegularFile(path))
            {
                return path;
            }

            const std::filesystem::path cwd = std::filesystem::current_path();
            const std::array<std::filesystem::path, 3> candidates = {
                cwd / path,
                cwd.parent_path() / path,
                cwd / "samples" / path,
            };
            for (const std::filesystem::path& candidate : candidates)
            {
                if (isRegularFile(candidate))
                {
                    return candidate.string();
                }
            }

            return path;
        }
    }  // namespace

    std::string resolveShaderPath(const std::string& shaderPath)
    {
        return resolveSampleFilePath(shaderPath);
    }

    std::string resolveSampleAssetPath(const std::string& assetPath)
    {
        return resolveSampleFilePath(assetPath);
    }

    KeraStringView sampleStringView(const std::string& text)
    {
        return {text.data(), text.size()};
    }

    void sampleLog(KeraLogLevel level, const std::string& message)
    {
        keraLog(level, sampleStringView(message));
    }

    void sampleLogDebug(const std::string& message)
    {
        sampleLog(KERA_LOG_LEVEL_DEBUG, message);
    }

    void sampleLogInfo(const std::string& message)
    {
        sampleLog(KERA_LOG_LEVEL_INFO, message);
    }

    void sampleLogWarning(const std::string& message)
    {
        sampleLog(KERA_LOG_LEVEL_WARNING, message);
    }

    void sampleLogError(const std::string& message)
    {
        sampleLog(KERA_LOG_LEVEL_ERROR, message);
    }

    const std::array<FullscreenTriangleVertex, 3>& fullscreenTriangleVertices()
    {
        static const std::array<FullscreenTriangleVertex, 3> vertices = {
            FullscreenTriangleVertex{{-1.0f, -1.0f}, {0.0f, 1.0f}},
            FullscreenTriangleVertex{{3.0f, -1.0f}, {2.0f, 1.0f}},
            FullscreenTriangleVertex{{-1.0f, 3.0f}, {0.0f, -1.0f}},
        };
        return vertices;
    }

    const std::array<uint16_t, 3>& fullscreenTriangleIndices()
    {
        static constexpr std::array<uint16_t, 3> indices = {0, 1, 2};
        return indices;
    }

}  // namespace kera
