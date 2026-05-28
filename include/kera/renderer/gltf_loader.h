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

    enum class GltfAlphaMode
    {
        Opaque,
        Mask,
        Blend
    };

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
        std::string debugName;
        bool requireMaterialTextures = true;
    };

    struct GltfMaterialTextures
    {
        TextureHandle baseColor;
        TextureHandle metalRoughness;
        TextureHandle emissive;
        TextureHandle occlusion;
        TextureHandle normal;
    };

    struct GltfMaterialTextureNames
    {
        std::string baseColor;
        std::string metalRoughness;
        std::string emissive;
        std::string occlusion;
        std::string normal;
    };

    struct GltfMaterialFactors
    {
        glm::vec4 baseColor{1.0f};
        glm::vec3 emissive{0.0f};
        float metallic = 1.0f;
        float roughness = 1.0f;
        float normalScale = 1.0f;
        float occlusionStrength = 1.0f;
        float alphaCutoff = 0.5f;
        GltfAlphaMode alphaMode = GltfAlphaMode::Opaque;
        bool doubleSided = false;
    };

    struct GltfLoadedModel
    {
        BufferHandle vertexBuffer;
        BufferHandle indexBuffer;
        GltfMaterialTextures materialTextures;
        SamplerHandle materialSampler;
        IndexFormat indexFormat = IndexFormat::UInt32;
        uint32_t indexCount = 0;
        glm::mat4 transform{1.0f};
        GltfMaterialTextureNames textureNames;
        GltfMaterialFactors materialFactors;
    };

    RendererResult<GltfLoadedModel> loadGltfModel(IRenderer& renderer, const GltfLoadDesc& desc);
    void destroyGltfModel(IRenderer& renderer, GltfLoadedModel& model);

}  // namespace kera
