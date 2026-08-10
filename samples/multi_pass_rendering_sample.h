// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/renderer/api.h"
#include "samples.h"

#include <array>
#include <string>
#include <vector>

namespace kera
{
    class MultiPassRenderingSample final : public Sample
    {
    public:
        MultiPassRenderingSample(Renderer& renderer, bool capture_smoke, bool capture_after_resize_smoke,
                                 AttachmentPlaygroundConfig config, bool reconfigure_smoke, bool performance_smoke,
                                 float sun_orbit_phase_radians = 0.0f, bool sun_orbit_enabled = true);

        void initialize() override;
        void update(float delta_time) override;
        void resize(Extent2D extent) override;
        void render(RenderContext& context) override;
        void drawUi() override;
        void cleanup() override;

    private:
        enum class SponzaSceneRenderMode
        {
            MAIN,
            MSAA_REFERENCE,
            SHADOW_COMPARISON_ACTIVE,
            SHADOW_COMPARISON_REFERENCE,
        };

        enum class AttachmentPassTimingScope : uint32_t
        {
            SHADOW_MAP = 1,
            SHADOW_REFERENCE = 2,
            MAIN_SCENE = 3,
            MAIN_SCENE_RESOLVE = 4,
            MSAA_REFERENCE = 5,
            SHADOW_LENS_ACTIVE = 6,
            SHADOW_LENS_ACTIVE_RESOLVE = 7,
            SHADOW_LENS_REFERENCE = 8,
            SHADOW_LENS_REFERENCE_RESOLVE = 9,
            BACKBUFFER_COMPOSITE = 10,
            COUNT = 11,
        };

        struct AttachmentPipelines
        {
            GraphicsPipelineHandle scene;
            GraphicsPipelineHandle sponza_scene;
            GraphicsPipelineHandle sponza_scene_double_sided;
            GraphicsPipelineHandle msaa_reference_scene;
            GraphicsPipelineHandle msaa_reference_sponza_scene;
            GraphicsPipelineHandle msaa_reference_sponza_scene_double_sided;
        };

        struct AttachmentResources
        {
            TextureHandle scene;
            TextureHandle scene_msaa;
            TextureHandle scene_depth;
            TextureHandle shadow;
            TextureHandle shadow_comparison_reference;
            TextureHandle shadow_comparison_active_scene;
            TextureHandle shadow_comparison_active_scene_msaa;
            TextureHandle shadow_comparison_active_scene_depth;
            TextureHandle shadow_comparison_scene_msaa;
            TextureHandle shadow_comparison_scene_depth;
            TextureHandle msaa_reference_scene;
            TextureHandle msaa_reference_scene_depth;
            DescriptorSetHandle scene_descriptor_set;
            DescriptorSetHandle composite_descriptor_set;
            DescriptorSetHandle shadow_preview_descriptor_set;
            DescriptorSetHandle shadow_inset_descriptor_set;
            DescriptorSetHandle msaa_reference_scene_descriptor_set;
            DescriptorSetHandle shadow_comparison_scene_descriptor_set;
            DescriptorSetHandle msaa_comparison_descriptor_set;
            DescriptorSetHandle shadow_comparison_descriptor_set;
            std::vector<DescriptorSetHandle> sponza_scene_descriptor_sets;
            std::vector<DescriptorSetHandle> sponza_scene_double_sided_descriptor_sets;
            std::vector<DescriptorSetHandle> msaa_reference_sponza_scene_descriptor_sets;
            std::vector<DescriptorSetHandle> msaa_reference_sponza_scene_double_sided_descriptor_sets;
            std::vector<DescriptorSetHandle> shadow_comparison_active_sponza_scene_descriptor_sets;
            std::vector<DescriptorSetHandle> shadow_comparison_active_sponza_scene_double_sided_descriptor_sets;
            std::vector<DescriptorSetHandle> shadow_comparison_sponza_scene_descriptor_sets;
            std::vector<DescriptorSetHandle> shadow_comparison_sponza_scene_double_sided_descriptor_sets;
            std::vector<DescriptorSetHandle> sponza_shadow_descriptor_sets;
            std::vector<DescriptorSetHandle> sponza_shadow_double_sided_descriptor_sets;
        };

        bool createShaderPrograms();
        bool loadSponzaSceneIfAvailable();
        bool createGeometry();
        bool createPipelines();
        bool createSponzaPipelines();
        bool createAttachmentPipelines(KeraAttachmentSampleCount sample_count, AttachmentPipelines& pipelines);
        bool createMsaaReferenceAttachmentPipelines(AttachmentPipelines& pipelines);
        bool recreateAttachmentResources(Extent2D extent);
        bool buildAttachmentResources(Extent2D extent, KeraAttachmentSampleCount sample_count,
                                      const AttachmentPlaygroundConfig& config, AttachmentPipelines& pipelines,
                                      AttachmentResources& resources);
        bool createSceneDescriptor(const AttachmentPipelines& pipelines, AttachmentResources& resources);
        bool createMsaaReferenceSceneDescriptor(const AttachmentPipelines& pipelines, AttachmentResources& resources);
        bool createShadowComparisonSceneDescriptor(const AttachmentPipelines& pipelines,
                                                   AttachmentResources& resources);
        bool createSponzaDescriptors(const AttachmentPipelines& pipelines, AttachmentResources& resources);
        bool createCompositeDescriptor(AttachmentResources& resources);
        bool createShadowPreviewDescriptor(AttachmentResources& resources);
        bool createShadowInsetDescriptor(AttachmentResources& resources);
        bool createMsaaComparisonDescriptor(AttachmentResources& resources);
        bool createShadowComparisonDescriptor(AttachmentResources& resources);
        bool applyPendingConfiguration();
        bool applyAttachmentConfiguration(Extent2D extent, const AttachmentPlaygroundConfig& config,
                                          bool wait_for_idle);
        KeraAttachmentSampleCount selectAttachmentSampleCount(Extent2D extent, const AttachmentPlaygroundConfig& config,
                                                              AttachmentPipelines& pipelines,
                                                              AttachmentResources& resources);
        bool destroyAttachmentResources(AttachmentResources& resources);
        bool destroyAttachmentPipelines(AttachmentPipelines& pipelines);
        bool uploadSponzaUniforms(FrameHandle frame);
        void drawSponzaScene(FrameHandle frame, bool shadow_pass,
                             SponzaSceneRenderMode mode = SponzaSceneRenderMode::MAIN);
        bool verifyCapture();
        void destroySponzaResources();

        Renderer& m_renderer;
        ShaderProgramHandle m_scene_shader_program;
        ShaderProgramHandle m_shadow_shader_program;
        ShaderProgramHandle m_composite_shader_program;
        ShaderProgramHandle m_shadow_preview_shader_program;
        ShaderProgramHandle m_shadow_inset_shader_program;
        ShaderProgramHandle m_msaa_comparison_shader_program;
        ShaderProgramHandle m_shadow_comparison_shader_program;
        ShaderProgramHandle m_sponza_scene_shader_program;
        ShaderProgramHandle m_sponza_shadow_shader_program;
        BufferHandle m_scene_vertex_buffer;
        BufferHandle m_scene_index_buffer;
        BufferHandle m_fullscreen_vertex_buffer;
        BufferHandle m_fullscreen_index_buffer;
        GltfLoadedScene m_sponza_scene{};
        std::vector<BufferHandle> m_sponza_scene_uniform_buffers;
        std::vector<BufferHandle> m_sponza_shadow_diagnostic_uniform_buffers;
        std::vector<BufferHandle> m_sponza_shadow_uniform_buffers;
        SamplerHandle m_scene_sampler;
        SamplerHandle m_shadow_inset_sampler;
        GraphicsPipelineHandle m_shadow_pipeline;
        GraphicsPipelineHandle m_composite_pipeline;
        GraphicsPipelineHandle m_shadow_preview_pipeline;
        GraphicsPipelineHandle m_shadow_inset_pipeline;
        GraphicsPipelineHandle m_msaa_comparison_pipeline;
        GraphicsPipelineHandle m_shadow_comparison_pipeline;
        GraphicsPipelineHandle m_sponza_shadow_pipeline;
        GraphicsPipelineHandle m_sponza_shadow_double_sided_pipeline;
        AttachmentPipelines m_attachment_pipelines;
        AttachmentResources m_attachment_resources;
        Extent2D m_render_extent{};
        uint32_t m_scene_index_count = 0;
        uint32_t m_fullscreen_index_count = 0;
        uint32_t m_non_zero_resize_count = 0;
        float m_elapsed_seconds = 0.0f;
        float m_sun_orbit_phase_radians = 0.0f;
        float m_sun_orbit_period_seconds = 90.0f;
        bool m_sun_orbit_enabled = true;
        bool m_capture_smoke = false;
        bool m_capture_after_resize_smoke = false;
        bool m_capture_after_resize_frame_pending = false;
        AttachmentPlaygroundConfig m_active_config;
        AttachmentPlaygroundConfig m_requested_config;
        bool m_reconfigure_smoke = false;
        std::array<PassTimingHistory, static_cast<size_t>(AttachmentPassTimingScope::COUNT)> m_pass_timing_history{};
        uint32_t m_update_count = 0;
        uint32_t m_reconfigure_smoke_stage = 0;
        bool m_config_dirty = false;
        std::string m_configuration_error;
        bool m_uses_sponza = false;
        KeraAttachmentSampleCount m_scene_sample_count = KERA_ATTACHMENT_SAMPLE_COUNT_1;
        bool m_capture_requested = false;
        bool m_capture_verified = false;
        bool m_initialized = false;
    };
}  // namespace kera
