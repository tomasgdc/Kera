// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/renderer/descriptors.h"
#include "kera/renderer/result.h"

#include <filesystem>

namespace kera
{
    class IRenderer;

    struct IblSphericalHarmonics
    {
        float coefficients[9][4]{};
    };

    struct IblEnvironmentLoadDesc
    {
        std::filesystem::path iblKtxPath;
        std::filesystem::path skyboxKtxPath;
        std::filesystem::path sphericalHarmonicsPath;
        std::string debugName = "IBL Environment";
    };

    struct IblEnvironment
    {
        TextureHandle iblTexture;
        TextureHandle skyboxTexture;
        SamplerHandle sampler;
        IblSphericalHarmonics sphericalHarmonics{};
        uint32_t iblMiplevels = 0;
        uint32_t skyboxMiplevels = 0;
    };

    RendererResult<IblEnvironment> loadIblEnvironment(IRenderer& renderer, const IblEnvironmentLoadDesc& desc);
    void destroyIblEnvironment(IRenderer& renderer, IblEnvironment& environment);
}
