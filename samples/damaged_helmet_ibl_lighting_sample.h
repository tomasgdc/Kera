// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/renderer/api.h"
#include "samples.h"

#include <cstdint>
#include <vector>

namespace kera
{

    class DamagedHelmetIBLLightingSample : public Sample
    {
    public:
        explicit DamagedHelmetIBLLightingSample(Renderer& renderer);
        DamagedHelmetIBLLightingSample(Renderer& renderer, uint32_t debugView);
        DamagedHelmetIBLLightingSample(Renderer& renderer, uint32_t debugView, bool fixedYaw, float yawRadians);

        void initialize() override;
        void update(float deltaTime) override;
        void resize(Extent2D extent) override;
        void render(RenderContext& context) override;
        void cleanup() override;
        ClearColorValue getClearColor() const override;

    private:
        bool createShaderPrograms();
        bool loadGltfModelResources();
        bool loadIblEnvironmentResources();
        bool createFullscreenGeometry();
        bool recreateRenderResources(Extent2D extent);
        bool createPipelinesAndDescriptors();
        void destroyRenderResources();
        void destroyLoadedResources();

        Renderer& m_renderer;
        ShaderProgramHandle m_meshShaderProgram;
        ShaderProgramHandle m_displayShaderProgram;
        ShaderProgramHandle m_skyboxShaderProgram;
        GltfLoadedModel m_model;
        IblEnvironment m_iblEnvironment;
        BufferHandle m_fullscreenVertexBuffer;
        BufferHandle m_fullscreenIndexBuffer;
        BufferHandle m_uniformBuffer;
        TextureHandle m_sceneTexture;
        TextureHandle m_sceneDepthTexture;
        RenderTargetHandle m_sceneRenderTarget;
        GraphicsPipelineHandle m_meshPipeline;
        GraphicsPipelineHandle m_displayPipeline;
        GraphicsPipelineHandle m_skyboxPipeline;
        std::vector<DescriptorSetHandle> m_meshDescriptorSets;
        std::vector<DescriptorSetHandle> m_skyboxDescriptorSets;
        DescriptorSetHandle m_displayDescriptorSet;
        Extent2D m_renderExtent;
        uint32_t m_fullscreenIndexCount = 0;
        float m_elapsedTime = 0.0f;
        float m_initialYawRadians = 2.2f;
        uint32_t m_debugView = 0;
        bool m_fixedYaw = false;
        bool m_initialized = false;
    };

}  // namespace kera
