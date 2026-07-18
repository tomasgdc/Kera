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
        BufferHandle m_instance_buffer;
        BufferHandle m_uniform_buffer;
        std::vector<DescriptorSetHandle> m_uniform_descriptor_sets;
        uint32_t m_index_count;
        uint32_t m_instance_count;
        GraphicsPipelineHandle m_pipeline;
        float m_rotation_angle;
    };

}  // namespace kera
