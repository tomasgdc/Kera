// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/renderer/api.h"
#include "samples.h"

namespace kera
{

    class BasicTriangleSample : public Sample
    {
    public:
        explicit BasicTriangleSample(Renderer& renderer);

        void initialize() override;
        void update(float delta_time) override;
        void render(RenderContext& context) override;
        void cleanup() override;

    private:
        bool createShaderProgram();
        bool createGeometry();
        bool createPipeline();

        Renderer& m_renderer;
        ShaderProgramHandle m_shader_program;
        BufferHandle m_vertex_buffer;
        BufferHandle m_index_buffer;
        uint32_t m_index_count;
        GraphicsPipelineHandle m_pipeline;
        float m_rotation_angle;
    };

}  // namespace kera
