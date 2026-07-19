// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/renderer/descriptors.h"
#include "kera/renderer/result.h"

#include <glm/glm.hpp>

#include <string>

namespace kera
{
    class IRenderer;

    struct GltfVertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec4 tangent;
    };

    struct GltfLoadDesc
    {
        std::string path;
        std::string debug_name;
        bool require_material_textures = true;
    };

    struct GltfMaterialTextures
    {
        TextureHandle base_color;
        TextureHandle metal_roughness;
        TextureHandle emissive;
        TextureHandle occlusion;
        TextureHandle normal;
    };

    struct GltfMaterialTextureNames
    {
        std::string base_color;
        std::string metal_roughness;
        std::string emissive;
        std::string occlusion;
        std::string normal;
    };

    struct GltfMaterialFactors
    {
        glm::vec4 base_color{1.0f};
        glm::vec3 emissive{0.0f};
        float metallic = 1.0f;
        float roughness = 1.0f;
        float normal_scale = 1.0f;
        float occlusion_strength = 1.0f;
        float alpha_cutoff = 0.5f;
        EGltfAlphaMode alpha_mode = EGltfAlphaMode::ALPHA_OPAQUE;
        bool double_sided = false;
    };

    struct GltfLoadedModel
    {
        BufferHandle vertex_buffer;
        BufferHandle index_buffer;
        GltfMaterialTextures material_textures;
        SamplerHandle material_sampler;
        EIndexFormat index_format = EIndexFormat::U_INT32;
        uint32_t index_count = 0;
        glm::mat4 transform{1.0f};
        GltfMaterialTextureNames texture_names;
        GltfMaterialFactors material_factors;
    };

    RendererResult<GltfLoadedModel> loadGltfModel(IRenderer& renderer, const GltfLoadDesc& desc);
    void destroyGltfModel(IRenderer& renderer, GltfLoadedModel& model);

}  // namespace kera
