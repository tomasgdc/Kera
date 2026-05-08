#pragma once

#include "kera/renderer/interfaces.h"
#include "samples.h"

#include <array>
#include <cstdint>

namespace kera
{

    class InstancedTriangleManyLightsSample : public Sample
    {
    public:
        explicit InstancedTriangleManyLightsSample(IRenderer& renderer);

        void initialize() override;
        void update(float deltaTime) override;
        void resize(Extent2D extent) override;
        void render(RenderContext& context) override;
        void cleanup() override;

    private:
        static constexpr uint32_t kLightCount = 64;

        bool createShaderPrograms();
        bool createGeometry();
        bool recreateRenderResources(Extent2D extent);
        bool createPipelinesAndDescriptors();
        void destroyRenderResources();
        void updateInstances(float angleRadians);
        void updateGeometryUniforms();
        void updateLightingUniforms(float angleRadians);

        IRenderer& m_renderer;
        ShaderProgramHandle m_geometryShaderProgram;
        ShaderProgramHandle m_lightingShaderProgram;
        BufferHandle m_vertexBuffer;
        BufferHandle m_indexBuffer;
        BufferHandle m_instanceBuffer;
        BufferHandle m_fullscreenVertexBuffer;
        BufferHandle m_fullscreenIndexBuffer;
        BufferHandle m_geometryUniformBuffer;
        BufferHandle m_lightingUniformBuffer;
        TextureHandle m_sceneTexture;
        SamplerHandle m_sceneSampler;
        RenderTargetHandle m_sceneRenderTarget;
        GraphicsPipelineHandle m_geometryPipeline;
        GraphicsPipelineHandle m_lightingPipeline;
        DescriptorSetHandle m_geometryDescriptorSet;
        DescriptorSetHandle m_lightingDescriptorSet;
        Extent2D m_renderExtent;
        uint32_t m_indexCount = 0;
        uint32_t m_instanceCount = 0;
        uint32_t m_fullscreenIndexCount = 0;
        float m_elapsedTime = 0.0f;
        bool m_initialized = false;
    };

}  // namespace kera
