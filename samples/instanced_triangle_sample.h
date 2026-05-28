// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/renderer/api.h"
#include "samples.h"

#include <cstdint>
#include <vector>

namespace kera
{

    class InstancedTriangleSample : public Sample
    {
    public:
        explicit InstancedTriangleSample(Renderer& renderer);

        void initialize() override;
        void update(float deltaTime) override;
        void render(RenderContext& context) override;
        void cleanup() override;

    private:
        bool createShaderProgram();
        bool createGeometry();
        bool createPipeline();

        Renderer& m_renderer;
        ShaderProgramHandle m_shaderProgram;
        BufferHandle m_vertexBuffer;
        BufferHandle m_indexBuffer;
        BufferHandle m_instanceBuffer;
        BufferHandle m_uniformBuffer;
        std::vector<DescriptorSetHandle> m_uniformDescriptorSets;
        uint32_t m_indexCount;
        uint32_t m_instanceCount;
        GraphicsPipelineHandle m_pipeline;
        float m_rotationAngle;
    };

}  // namespace kera
