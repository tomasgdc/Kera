// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/renderer/api.h"
#include "samples.h"

#include <array>
#include <cstdint>

namespace kera
{

    class InstancedTriangleManyLightsSample : public Sample
    {
    public:
        explicit InstancedTriangleManyLightsSample(Renderer& renderer);

        void initialize() override;
        void update(float delta_time) override;
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
        void updateInstances(float angle_radians);
        void updateGeometryUniforms();
        void updateLightingUniforms(float angle_radians);

        Renderer& m_renderer;
        ShaderProgramHandle m_geometry_shader_program;
        ShaderProgramHandle m_lighting_shader_program;
        BufferHandle m_vertex_buffer;
        BufferHandle m_index_buffer;
        BufferHandle m_instance_buffer;
        BufferHandle m_fullscreen_vertex_buffer;
        BufferHandle m_fullscreen_index_buffer;
        BufferHandle m_geometry_uniform_buffer;
        BufferHandle m_lighting_uniform_buffer;
        TextureHandle m_scene_texture;
        TextureHandle m_scene_depth_texture;
        SamplerHandle m_scene_sampler;
        RenderTargetHandle m_scene_render_target;
        GraphicsPipelineHandle m_geometry_pipeline;
        GraphicsPipelineHandle m_lighting_pipeline;
        DescriptorSetHandle m_geometry_descriptor_set;
        DescriptorSetHandle m_lighting_descriptor_set;
        Extent2D m_render_extent;
        uint32_t m_index_count = 0;
        uint32_t m_instance_count = 0;
        uint32_t m_fullscreen_index_count = 0;
        float m_elapsed_time = 0.0f;
        bool m_initialized = false;
    };

}  // namespace kera
