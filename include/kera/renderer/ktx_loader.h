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
        std::filesystem::path ibl_ktx_path;
        std::filesystem::path skybox_ktx_path;
        std::filesystem::path spherical_harmonics_path;
        std::string debug_name = "IBL Environment";
    };

    struct IblEnvironment
    {
        TextureHandle ibl_texture;
        TextureHandle skybox_texture;
        SamplerHandle sampler;
        IblSphericalHarmonics spherical_harmonics{};
        uint32_t ibl_miplevels = 0;
        uint32_t skybox_miplevels = 0;
    };

    RendererResult<IblEnvironment> loadIblEnvironment(IRenderer& renderer, const IblEnvironmentLoadDesc& desc);
    void destroyIblEnvironment(IRenderer& renderer, IblEnvironment& environment);
}
