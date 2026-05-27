#pragma once

#include "kera/renderer/gltf_loader.h"
#include "kera/renderer/interfaces.h"
#include "samples.h"

#include <cstdint>
#include <vector>

namespace kera
{

    class DamagedHelmetSample : public Sample
    {
    public:
        explicit DamagedHelmetSample(IRenderer& renderer);
        DamagedHelmetSample(IRenderer& renderer, uint32_t debugView);
        DamagedHelmetSample(IRenderer& renderer, uint32_t debugView, bool fixedYaw, float yawRadians);

        void initialize() override;
        void update(float deltaTime) override;
        void resize(Extent2D extent) override;
        void render(RenderContext& context) override;
        void cleanup() override;
        ClearColorValue getClearColor() const override;

    private:
        bool createShaderPrograms();
        bool loadGltfModelResources();
        bool createFullscreenGeometry();
        bool recreateRenderResources(Extent2D extent);
        bool createPipelinesAndDescriptors();
        void destroyRenderResources();
        void destroyLoadedResources();

        IRenderer& m_renderer;
        ShaderProgramHandle m_meshShaderProgram;
        ShaderProgramHandle m_displayShaderProgram;
        GltfLoadedModel m_model;
        BufferHandle m_fullscreenVertexBuffer;
        BufferHandle m_fullscreenIndexBuffer;
        BufferHandle m_uniformBuffer;
        TextureHandle m_sceneTexture;
        TextureHandle m_sceneDepthTexture;
        RenderTargetHandle m_sceneRenderTarget;
        GraphicsPipelineHandle m_meshPipeline;
        GraphicsPipelineHandle m_displayPipeline;
        std::vector<DescriptorSetHandle> m_meshDescriptorSets;
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
