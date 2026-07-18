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
        DamagedHelmetIBLLightingSample(Renderer& renderer, uint32_t debug_view);
        DamagedHelmetIBLLightingSample(Renderer& renderer, uint32_t debug_view, bool fixed_yaw, float yaw_radians);

        void initialize() override;
        void update(float delta_time) override;
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
        ShaderProgramHandle m_mesh_shader_program;
        ShaderProgramHandle m_display_shader_program;
        ShaderProgramHandle m_skybox_shader_program;
        GltfLoadedModel m_model;
        IblEnvironment m_ibl_environment;
        BufferHandle m_fullscreen_vertex_buffer;
        BufferHandle m_fullscreen_index_buffer;
        BufferHandle m_uniform_buffer;
        TextureHandle m_scene_texture;
        TextureHandle m_scene_depth_texture;
        RenderTargetHandle m_scene_render_target;
        GraphicsPipelineHandle m_mesh_pipeline;
        GraphicsPipelineHandle m_display_pipeline;
        GraphicsPipelineHandle m_skybox_pipeline;
        std::vector<DescriptorSetHandle> m_mesh_descriptor_sets;
        std::vector<DescriptorSetHandle> m_skybox_descriptor_sets;
        DescriptorSetHandle m_display_descriptor_set;
        Extent2D m_render_extent;
        uint32_t m_fullscreen_index_count = 0;
        float m_elapsed_time = 0.0f;
        float m_initial_yaw_radians = 2.2f;
        uint32_t m_debug_view = 0;
        bool m_fixed_yaw = false;
        bool m_initialized = false;
    };

}  // namespace kera
