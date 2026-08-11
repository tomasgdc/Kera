// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "multi_pass_rendering_sample.h"

#include "kera/renderer/test_attachment_capture.h"
#include "render_context.h"
#include "sample_utils.h"

#ifdef KERA_HAS_IMGUI
#include <imgui.h>
#endif

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace kera
{
    namespace
    {
        struct SceneVertex
        {
            glm::vec3 position;
            glm::vec3 color;
        };

        struct SponzaSceneUniforms
        {
            glm::mat4 model;
            glm::mat4 normal_matrix;
            glm::mat4 view;
            glm::mat4 projection;
            glm::mat4 light_view;
            glm::mat4 light_projection;
            glm::vec4 camera_position;
            glm::vec4 light_direction_shadow_bias;
            glm::vec4 base_color_factor;
            glm::vec4 emissive_factor_normal_scale;
            glm::vec4 metallic_roughness_occlusion;
            glm::vec4 alpha_mode_cutoff_double_sided;
        };

        struct SponzaShadowUniforms
        {
            glm::mat4 model;
            glm::mat4 light_view;
            glm::mat4 light_projection;
            glm::vec4 alpha_mode_cutoff_base_alpha;
        };

        constexpr const char* kShaderPath = "shaders/multi_pass.slang";
        constexpr const char* kCaptureName = "Multi-Pass Scene Colour";
        constexpr const char* kSponzaAssetPath = "assets/gltf/Sponza/glTF/Sponza.gltf";
        constexpr uint32_t kExpectedSponzaDrawCount = 103;
        constexpr uint32_t kSponzaUniformRingSlots = 3;
        constexpr uint32_t kShadowComparisonReferenceResolution = 1024;
        constexpr std::array<const char*, 11> kAttachmentPassTimingNames = {
            "",
            "Shadow map",
            "Shadow map reference",
            "Main scene",
            "Main scene resolve",
            "MSAA reference",
            "Shadow lens active",
            "Shadow lens active resolve",
            "Shadow lens reference",
            "Shadow lens reference resolve",
            "Backbuffer composite",
        };

        const char* attachmentPassTimingName(uint32_t scope_id)
        {
            return scope_id < kAttachmentPassTimingNames.size() ? kAttachmentPassTimingNames[scope_id] : "Unknown";
        }

        namespace multiPassSponzaShader
        {
            constexpr const char* kSceneVertexEntryPoint = "gltfSceneVertexMain";
            constexpr const char* kSceneFragmentEntryPoint = "gltfSceneFragmentMain";
            constexpr const char* kShadowVertexEntryPoint = "gltfShadowVertexMain";
            constexpr const char* kShadowFragmentEntryPoint = "gltfShadowFragmentMain";
            constexpr const char* kSceneParams = "sceneParams";
            constexpr const char* kShadowParams = "shadowParams";
            constexpr const char* kBaseColorTexture = "baseColorTexture";
            constexpr const char* kMetalRoughnessTexture = "metalRoughnessTexture";
            constexpr const char* kEmissiveTexture = "emissiveTexture";
            constexpr const char* kOcclusionTexture = "occlusionTexture";
            constexpr const char* kNormalTexture = "normalTexture";
            constexpr const char* kMaterialSampler = "materialSampler";
            constexpr const char* kShadowTexture = "shadowTexture";
            constexpr const char* kShadowSampler = "shadowSampler";
        }  // namespace multiPassSponzaShader

        float toShaderAlphaMode(KeraGltfAlphaMode alpha_mode)
        {
            return static_cast<float>(alpha_mode);
        }

        std::string attachmentErrorText(const KeraAttachmentError& error)
        {
            return error.message.data ? std::string(error.message.data, error.message.size)
                                      : "unknown attachment error";
        }

        uint32_t attachmentSampleCountValue(KeraAttachmentSampleCount sample_count)
        {
            switch (sample_count)
            {
                case KERA_ATTACHMENT_SAMPLE_COUNT_8:
                    return 8;
                case KERA_ATTACHMENT_SAMPLE_COUNT_4:
                    return 4;
                case KERA_ATTACHMENT_SAMPLE_COUNT_2:
                    return 2;
                default:
                    return 1;
            }
        }

        bool attachmentConfigsEqual(const AttachmentPlaygroundConfig& left, const AttachmentPlaygroundConfig& right)
        {
            return left.requested_msaa_samples == right.requested_msaa_samples &&
                   left.shadow_resolution == right.shadow_resolution && left.shadows_enabled == right.shadows_enabled &&
                   left.preview_shadow_map == right.preview_shadow_map &&
                   left.preview_shadow_inset == right.preview_shadow_inset &&
                   left.preview_msaa_comparison == right.preview_msaa_comparison &&
                   left.preview_shadow_comparison == right.preview_shadow_comparison;
        }

        bool attachmentResourcesDiffer(const AttachmentPlaygroundConfig& left, const AttachmentPlaygroundConfig& right)
        {
            return left.requested_msaa_samples != right.requested_msaa_samples ||
                   left.shadow_resolution != right.shadow_resolution || left.shadows_enabled != right.shadows_enabled ||
                   left.preview_msaa_comparison != right.preview_msaa_comparison ||
                   left.preview_shadow_comparison != right.preview_shadow_comparison;
        }

        void logAttachmentConfiguration(const AttachmentPlaygroundConfig& config, uint32_t active_sample_count,
                                        Extent2D extent)
        {
            sampleLogInfo("ATTACHMENT_PLAYGROUND_CONFIG requested_msaa=" +
                          (config.requested_msaa_samples == 0 ? std::string("auto")
                                                              : std::to_string(config.requested_msaa_samples)) +
                          " active_msaa=" + std::to_string(active_sample_count) +
                          " shadow_resolution=" + std::to_string(config.shadow_resolution) +
                          " shadows=" + std::string(config.shadows_enabled ? "on" : "off") +
                          " preview=" + std::string(config.preview_shadow_map ? "on" : "off") +
                          " inset=" + std::string(config.preview_shadow_inset ? "on" : "off") +
                          " msaa_compare=" + std::string(config.preview_msaa_comparison ? "on" : "off") +
                          " shadow_compare=" + std::string(config.preview_shadow_comparison ? "on" : "off") +
                          " extent=" + std::to_string(extent.width) + "x" + std::to_string(extent.height));
        }
    }  // namespace

    MultiPassRenderingSample::MultiPassRenderingSample(Renderer& renderer, bool capture_smoke,
                                                       bool capture_after_resize_smoke,
                                                       AttachmentPlaygroundConfig config, bool reconfigure_smoke,
                                                       bool performance_smoke, float sun_orbit_phase_radians,
                                                       bool sun_orbit_enabled)
        : Sample("Attachment Playground (Sponza)")
        , m_renderer(renderer)
        , m_capture_smoke(capture_smoke)
        , m_capture_after_resize_smoke(capture_after_resize_smoke)
        , m_active_config(config)
        , m_requested_config(config)
        , m_reconfigure_smoke(reconfigure_smoke)
    {
        m_sun_orbit_phase_radians = sun_orbit_phase_radians;
        m_sun_orbit_enabled = sun_orbit_enabled;
    }

    void MultiPassRenderingSample::initialize()
    {
        sampleLogInfo("Initializing " + std::string(getName()));
        if (!m_renderer.supportsAttachmentRendering())
        {
            sampleLogError("Attachment Playground requires core attachment-rendering support.");
            return;
        }

        const KeraAttachmentCapabilities capabilities = m_renderer.getAttachmentCapabilities();
        if (capabilities.max_color_attachments < 1 || !capabilities.supports_depth_only_rendering ||
            (capabilities.supported_sample_counts & KERA_ATTACHMENT_SAMPLE_COUNT_1) == 0)
        {
            sampleLogError("Attachment Playground requires single-sample colour and depth-only attachments.");
            return;
        }

        if (!createShaderPrograms() || !loadSponzaSceneIfAvailable() || !createGeometry() || !createPipelines() ||
            !applyAttachmentConfiguration(m_renderer.getDrawableExtent(), m_active_config, false))
        {
            sampleLogError("Failed to initialize Attachment Playground resources.");
            cleanup();
            return;
        }

        m_initialized = true;
        sampleLogInfo("Attachment Playground initialized successfully with " +
                      std::to_string(attachmentSampleCountValue(m_scene_sample_count)) + "x MSAA and a " +
                      std::to_string(m_active_config.shadow_resolution) + "px shadow map.");
        if (m_active_config.preview_shadow_map)
        {
            sampleLogInfo("Multi-Pass shadow depth preview enabled.");
        }
        if (m_active_config.preview_shadow_inset)
        {
            sampleLogInfo("Multi-Pass shadow inset diagnostic enabled.");
        }
        if (m_active_config.preview_msaa_comparison)
        {
            sampleLogInfo(m_attachment_resources.msaa_reference_scene.isValid()
                              ? "Multi-Pass MSAA comparison diagnostic enabled."
                              : "Multi-Pass MSAA comparison requires an active sample count above 1x.");
        }
        if (m_active_config.preview_shadow_comparison)
        {
            sampleLogInfo(m_attachment_resources.shadow_comparison_reference.isValid()
                              ? "Multi-Pass shadow comparison diagnostic enabled."
                              : "Multi-Pass shadow comparison requires an active shadow map above 1024 px.");
        }
        if (m_scene_sample_count == KERA_ATTACHMENT_SAMPLE_COUNT_4)
        {
            sampleLogInfo("Multi-Pass 4x MSAA resolve enabled.");
        }
    }

    bool MultiPassRenderingSample::createShaderPrograms()
    {
        const std::string shader_path = resolveShaderPath(kShaderPath);
        m_scene_shader_program = m_renderer.createGraphicsShaderProgram({
            .path = sampleStringView(shader_path),
            .vertex_entry_point = stringView("sceneVertexMain"),
            .fragment_entry_point = stringView("sceneFragmentMain"),
            .source = EShaderSourceKind::SLANG_FILE,
            .debug_name = stringView("Multi-Pass Scene Shader"),
        });
        m_shadow_shader_program = m_renderer.createGraphicsShaderProgram({
            .path = sampleStringView(shader_path),
            .vertex_entry_point = stringView("shadowVertexMain"),
            .fragment_entry_point = stringView("shadowFragmentMain"),
            .source = EShaderSourceKind::SLANG_FILE,
            .debug_name = stringView("Multi-Pass Shadow Shader"),
        });
        m_composite_shader_program = m_renderer.createGraphicsShaderProgram({
            .path = sampleStringView(shader_path),
            .vertex_entry_point = stringView("compositeVertexMain"),
            .fragment_entry_point = stringView("compositeFragmentMain"),
            .source = EShaderSourceKind::SLANG_FILE,
            .debug_name = stringView("Multi-Pass Composite Shader"),
        });
        m_shadow_preview_shader_program = m_renderer.createGraphicsShaderProgram({
            .path = sampleStringView(shader_path),
            .vertex_entry_point = stringView("compositeVertexMain"),
            .fragment_entry_point = stringView("shadowPreviewFragmentMain"),
            .source = EShaderSourceKind::SLANG_FILE,
            .debug_name = stringView("Multi-Pass Shadow Preview Shader"),
        });
        m_shadow_inset_shader_program = m_renderer.createGraphicsShaderProgram({
            .path = sampleStringView(shader_path),
            .vertex_entry_point = stringView("compositeVertexMain"),
            .fragment_entry_point = stringView("shadowInsetFragmentMain"),
            .source = EShaderSourceKind::SLANG_FILE,
            .debug_name = stringView("Multi-Pass Shadow Inset Shader"),
        });
        m_msaa_comparison_shader_program = m_renderer.createGraphicsShaderProgram({
            .path = sampleStringView(shader_path),
            .vertex_entry_point = stringView("compositeVertexMain"),
            .fragment_entry_point = stringView("msaaComparisonFragmentMain"),
            .source = EShaderSourceKind::SLANG_FILE,
            .debug_name = stringView("Multi-Pass MSAA Comparison Shader"),
        });
        m_shadow_comparison_shader_program = m_renderer.createGraphicsShaderProgram({
            .path = sampleStringView(shader_path),
            .vertex_entry_point = stringView("compositeVertexMain"),
            .fragment_entry_point = stringView("shadowComparisonFragmentMain"),
            .source = EShaderSourceKind::SLANG_FILE,
            .debug_name = stringView("Multi-Pass Shadow Comparison Shader"),
        });
        return m_scene_shader_program.isValid() && m_shadow_shader_program.isValid() &&
               m_composite_shader_program.isValid() && m_shadow_preview_shader_program.isValid() &&
               m_shadow_inset_shader_program.isValid() && m_msaa_comparison_shader_program.isValid() &&
               m_shadow_comparison_shader_program.isValid();
    }

    bool MultiPassRenderingSample::loadSponzaSceneIfAvailable()
    {
        const char* override_path = std::getenv("KERA_SPONZA_ASSET_PATH");
        const std::string asset_path = override_path && override_path[0] != '\0'
                                           ? std::string(override_path)
                                           : resolveSampleAssetPath(kSponzaAssetPath);
        std::error_code file_error;
        if (!std::filesystem::is_regular_file(asset_path, file_error))
        {
            sampleLogInfo("Multi-Pass Sponza asset is not packaged; using the procedural attachment fallback.");
            return true;
        }
        if (!m_renderer.supportsGltfSceneLoading())
        {
            sampleLogError("Multi-Pass Sponza requires the V1 static glTF scene-loader capability.");
            return false;
        }
        if (!m_renderer.loadGltfScene(
                {
                    .path = sampleStringView(asset_path),
                    .debug_name = stringView("Sponza"),
                    .require_material_textures = 1,
                },
                m_sponza_scene))
        {
            sampleLogError("Multi-Pass failed to load the packaged Sponza glTF scene.");
            return false;
        }
        if (m_sponza_scene.draw_count != kExpectedSponzaDrawCount)
        {
            sampleLogError("Multi-Pass Sponza draw count did not match the approved asset manifest.");
            m_renderer.destroyGltfScene(m_sponza_scene);
            return false;
        }

        m_sponza_scene_uniform_buffers.reserve(m_sponza_scene.draw_count);
        m_sponza_shadow_diagnostic_uniform_buffers.reserve(m_sponza_scene.draw_count);
        m_sponza_shadow_uniform_buffers.reserve(m_sponza_scene.draw_count);
        for (uint32_t draw_index = 0; draw_index < m_sponza_scene.draw_count; ++draw_index)
        {
            m_sponza_scene_uniform_buffers.push_back(
                m_renderer.createUniformRingBuffer(sizeof(SponzaSceneUniforms), kSponzaUniformRingSlots));
            m_sponza_shadow_diagnostic_uniform_buffers.push_back(
                m_renderer.createUniformRingBuffer(sizeof(SponzaSceneUniforms), kSponzaUniformRingSlots));
            m_sponza_shadow_uniform_buffers.push_back(
                m_renderer.createUniformRingBuffer(sizeof(SponzaShadowUniforms), kSponzaUniformRingSlots));
            if (!m_sponza_scene_uniform_buffers.back().isValid() ||
                !m_sponza_shadow_diagnostic_uniform_buffers.back().isValid() ||
                !m_sponza_shadow_uniform_buffers.back().isValid())
            {
                sampleLogError("Multi-Pass failed to allocate aligned Sponza per-draw uniform buffers.");
                destroySponzaResources();
                return false;
            }
        }

        m_uses_sponza = true;
        sampleLogInfo("Multi-Pass Sponza scene loaded: 103 draw items.");
        return true;
    }

    bool MultiPassRenderingSample::createGeometry()
    {
        const std::array<SceneVertex, 6> scene_vertices = {{
            {{-0.95f, -0.82f, 0.85f}, {0.22f, 0.34f, 0.88f}},
            {{0.95f, -0.82f, 0.85f}, {0.24f, 0.52f, 0.92f}},
            {{0.00f, 0.92f, 0.85f}, {0.44f, 0.30f, 0.76f}},
            {{-0.65f, -0.50f, 0.20f}, {0.98f, 0.18f, 0.12f}},
            {{0.05f, -0.42f, 0.20f}, {0.96f, 0.74f, 0.12f}},
            {{-0.28f, 0.28f, 0.20f}, {0.16f, 0.86f, 0.36f}},
        }};
        const std::array<uint16_t, 6> scene_indices = {0, 1, 2, 3, 4, 5};
        const auto& fullscreen_vertices = fullscreenTriangleVertices();
        const auto& fullscreen_indices = fullscreenTriangleIndices();

        m_scene_index_count = static_cast<uint32_t>(scene_indices.size());
        m_fullscreen_index_count = static_cast<uint32_t>(fullscreen_indices.size());
        m_scene_vertex_buffer = m_renderer.createBuffer({
            .size = sizeof(scene_vertices),
            .usage = EBufferUsageKind::VERTEX,
            .memory_access = EMemoryAccess::CPU_WRITE,
            .debug_name = stringView("Multi-Pass Scene Vertex Buffer"),
        });
        m_scene_index_buffer = m_renderer.createBuffer({
            .size = sizeof(scene_indices),
            .usage = EBufferUsageKind::INDEX,
            .memory_access = EMemoryAccess::CPU_WRITE,
            .debug_name = stringView("Multi-Pass Scene Index Buffer"),
        });
        m_fullscreen_vertex_buffer = m_renderer.createBuffer({
            .size = sizeof(fullscreen_vertices),
            .usage = EBufferUsageKind::VERTEX,
            .memory_access = EMemoryAccess::CPU_WRITE,
            .debug_name = stringView("Multi-Pass Fullscreen Vertex Buffer"),
        });
        m_fullscreen_index_buffer = m_renderer.createBuffer({
            .size = sizeof(fullscreen_indices),
            .usage = EBufferUsageKind::INDEX,
            .memory_access = EMemoryAccess::CPU_WRITE,
            .debug_name = stringView("Multi-Pass Fullscreen Index Buffer"),
        });

        return m_scene_vertex_buffer.isValid() && m_scene_index_buffer.isValid() &&
               m_fullscreen_vertex_buffer.isValid() && m_fullscreen_index_buffer.isValid() &&
               m_renderer.uploadBuffer(m_scene_vertex_buffer, scene_vertices.data(), sizeof(scene_vertices)) &&
               m_renderer.uploadBuffer(m_scene_index_buffer, scene_indices.data(), sizeof(scene_indices)) &&
               m_renderer.uploadBuffer(m_fullscreen_vertex_buffer, fullscreen_vertices.data(),
                                       sizeof(fullscreen_vertices)) &&
               m_renderer.uploadBuffer(m_fullscreen_index_buffer, fullscreen_indices.data(),
                                       sizeof(fullscreen_indices));
    }

    bool MultiPassRenderingSample::createPipelines()
    {
        const VertexInputLayout shadow_vertex_input =
            VertexInputLayoutBuilder{}
                .vertexBinding<SceneVertex>(0)
                .field(KERA_VERTEX_FIELD(SceneVertex, position, 0, EVertexFormat::FLOAT3))
                .layout();
        KeraAttachmentError error{};
        m_shadow_pipeline = m_renderer.createAttachmentGraphicsPipeline(
            {
                .struct_size = sizeof(KeraAttachmentGraphicsPipelineDesc),
                .shader_program = m_shadow_shader_program,
                .vertex_input = shadow_vertex_input.view(),
                .color_formats = nullptr,
                .color_format_count = 0,
                .depth_format = KERA_TEXTURE_FORMAT_DEPTH32,
                .has_depth_attachment = 1,
                .sample_count = KERA_ATTACHMENT_SAMPLE_COUNT_1,
                .topology = KERA_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                .cull_mode = KERA_CULL_MODE_NONE,
                .front_face = KERA_FRONT_FACE_COUNTER_CLOCKWISE,
                .blend_mode = KERA_BLEND_MODE_OPAQUE,
                .depth_test = 1,
                .depth_write = 1,
                .debug_name = stringView("Attachment Playground Shadow Depth Pipeline"),
            },
            &error);
        if (!m_shadow_pipeline.isValid())
        {
            sampleLogError("Failed to create Attachment Playground shadow pipeline: " + attachmentErrorText(error));
            return false;
        }

        const VertexInputLayout composite_vertex_input =
            VertexInputLayoutBuilder{}
                .vertexBinding<FullscreenTriangleVertex>(0)
                .field(KERA_VERTEX_FIELD(FullscreenTriangleVertex, position, 0, EVertexFormat::FLOAT2))
                .field(KERA_VERTEX_FIELD(FullscreenTriangleVertex, uv, 0, EVertexFormat::FLOAT2))
                .layout();
        m_composite_pipeline = m_renderer.createGraphicsPipeline({
            .shader_program = m_composite_shader_program,
            .vertex_input = composite_vertex_input,
            .cull_mode = ECullModeKind::NONE,
            .debug_name = stringView("Attachment Playground Composite Pipeline"),
        });
        m_shadow_preview_pipeline = m_renderer.createGraphicsPipeline({
            .shader_program = m_shadow_preview_shader_program,
            .vertex_input = composite_vertex_input,
            .cull_mode = ECullModeKind::NONE,
            .debug_name = stringView("Attachment Playground Shadow Preview Pipeline"),
        });
        m_shadow_inset_pipeline = m_renderer.createGraphicsPipeline({
            .shader_program = m_shadow_inset_shader_program,
            .vertex_input = composite_vertex_input,
            .cull_mode = ECullModeKind::NONE,
            .debug_name = stringView("Attachment Playground Shadow Inset Pipeline"),
        });
        m_msaa_comparison_pipeline = m_renderer.createGraphicsPipeline({
            .shader_program = m_msaa_comparison_shader_program,
            .vertex_input = composite_vertex_input,
            .cull_mode = ECullModeKind::NONE,
            .debug_name = stringView("Attachment Playground MSAA Comparison Pipeline"),
        });
        m_shadow_comparison_pipeline = m_renderer.createGraphicsPipeline({
            .shader_program = m_shadow_comparison_shader_program,
            .vertex_input = composite_vertex_input,
            .cull_mode = ECullModeKind::NONE,
            .debug_name = stringView("Attachment Playground Shadow Comparison Pipeline"),
        });
        m_scene_sampler = m_renderer.createSampler({});
        m_shadow_inset_sampler = m_renderer.createSampler({
            .min_filter = KERA_SAMPLER_FILTER_NEAREST,
            .mag_filter = KERA_SAMPLER_FILTER_NEAREST,
            .mip_filter = KERA_SAMPLER_MIP_FILTER_NEAREST,
            .debug_name = stringView("Attachment Playground Shadow Inset Sampler"),
        });
        if (!m_composite_pipeline.isValid() || !m_shadow_preview_pipeline.isValid() ||
            !m_shadow_inset_pipeline.isValid() || !m_msaa_comparison_pipeline.isValid() ||
            !m_shadow_comparison_pipeline.isValid() || !m_scene_sampler.isValid() || !m_shadow_inset_sampler.isValid())
        {
            return false;
        }
        return !m_uses_sponza || createSponzaPipelines();
    }

    bool MultiPassRenderingSample::createSponzaPipelines()
    {
        const std::string shader_path = resolveShaderPath(kShaderPath);
        m_sponza_scene_shader_program = m_renderer.createGraphicsShaderProgram({
            .path = sampleStringView(shader_path),
            .vertex_entry_point = stringView(multiPassSponzaShader::kSceneVertexEntryPoint),
            .fragment_entry_point = stringView(multiPassSponzaShader::kSceneFragmentEntryPoint),
            .source = EShaderSourceKind::SLANG_FILE,
            .debug_name = stringView("Attachment Playground Sponza Scene Shader"),
        });
        m_sponza_shadow_shader_program = m_renderer.createGraphicsShaderProgram({
            .path = sampleStringView(shader_path),
            .vertex_entry_point = stringView(multiPassSponzaShader::kShadowVertexEntryPoint),
            .fragment_entry_point = stringView(multiPassSponzaShader::kShadowFragmentEntryPoint),
            .source = EShaderSourceKind::SLANG_FILE,
            .debug_name = stringView("Attachment Playground Sponza Shadow Shader"),
        });
        if (!m_sponza_scene_shader_program.isValid() || !m_sponza_shadow_shader_program.isValid())
        {
            sampleLogError("Failed to create Attachment Playground Sponza shader programs.");
            return false;
        }

        const VertexInputLayout shadow_vertex_input =
            VertexInputLayoutBuilder{}
                .vertexBinding<GltfVertex>(0)
                .field(KERA_VERTEX_FIELD(GltfVertex, position, 0, EVertexFormat::FLOAT3))
                .field(KERA_VERTEX_FIELD(GltfVertex, uv, 0, EVertexFormat::FLOAT2))
                .layout();
        KeraAttachmentError error{};
        const auto create_shadow_pipeline = [&](ECullModeKind cull_mode, const char* debug_name)
        {
            return m_renderer.createAttachmentGraphicsPipeline(
                {
                    .struct_size = sizeof(KeraAttachmentGraphicsPipelineDesc),
                    .shader_program = m_sponza_shadow_shader_program,
                    .vertex_input = shadow_vertex_input.view(),
                    .color_formats = nullptr,
                    .color_format_count = 0,
                    .depth_format = KERA_TEXTURE_FORMAT_DEPTH32,
                    .has_depth_attachment = 1,
                    .sample_count = KERA_ATTACHMENT_SAMPLE_COUNT_1,
                    .topology = KERA_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                    .cull_mode = static_cast<KeraCullModeKind>(cull_mode),
                    .front_face = KERA_FRONT_FACE_CLOCKWISE,
                    .blend_mode = KERA_BLEND_MODE_OPAQUE,
                    .depth_test = 1,
                    .depth_write = 1,
                    .debug_name = stringView(debug_name),
                },
                &error);
        };
        m_sponza_shadow_pipeline =
            create_shadow_pipeline(ECullModeKind::NONE, "Attachment Playground Sponza Shadow Pipeline");
        m_sponza_shadow_double_sided_pipeline =
            create_shadow_pipeline(ECullModeKind::NONE, "Attachment Playground Sponza Shadow Double-Sided Pipeline");
        if (!m_sponza_shadow_pipeline.isValid() || !m_sponza_shadow_double_sided_pipeline.isValid())
        {
            sampleLogError("Failed to create Attachment Playground Sponza shadow pipeline: " +
                           attachmentErrorText(error));
            return false;
        }
        return true;
    }

    bool MultiPassRenderingSample::createAttachmentPipelines(KeraAttachmentSampleCount sample_count,
                                                             AttachmentPipelines& pipelines)
    {
        KeraAttachmentError error{};
        const KeraTextureFormat scene_color_format = KERA_TEXTURE_FORMAT_RGBA8;
        const auto create_scene_pipeline = [&](ShaderProgramHandle shader_program,
                                               const VertexInputLayout& vertex_input, ECullModeKind cull_mode,
                                               KeraFrontFaceKind front_face, const char* debug_name)
        {
            return m_renderer.createAttachmentGraphicsPipeline(
                {
                    .struct_size = sizeof(KeraAttachmentGraphicsPipelineDesc),
                    .shader_program = shader_program,
                    .vertex_input = vertex_input.view(),
                    .color_formats = &scene_color_format,
                    .color_format_count = 1,
                    .depth_format = KERA_TEXTURE_FORMAT_DEPTH32,
                    .has_depth_attachment = 1,
                    .sample_count = sample_count,
                    .topology = KERA_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                    .cull_mode = static_cast<KeraCullModeKind>(cull_mode),
                    .front_face = front_face,
                    .blend_mode = KERA_BLEND_MODE_OPAQUE,
                    .depth_test = 1,
                    .depth_write = 1,
                    .debug_name = stringView(debug_name),
                },
                &error);
        };

        if (m_uses_sponza)
        {
            const VertexInputLayout vertex_input =
                VertexInputLayoutBuilder{}
                    .vertexBinding<GltfVertex>(0)
                    .field(KERA_VERTEX_FIELD(GltfVertex, position, 0, EVertexFormat::FLOAT3))
                    .field(KERA_VERTEX_FIELD(GltfVertex, normal, 0, EVertexFormat::FLOAT3))
                    .field(KERA_VERTEX_FIELD(GltfVertex, uv, 0, EVertexFormat::FLOAT2))
                    .field(KERA_VERTEX_FIELD(GltfVertex, tangent, 0, EVertexFormat::FLOAT4))
                    .layout();
            pipelines.sponza_scene =
                create_scene_pipeline(m_sponza_scene_shader_program, vertex_input, ECullModeKind::BACK,
                                      KERA_FRONT_FACE_CLOCKWISE, "Attachment Playground Sponza Scene Pipeline");
            pipelines.sponza_scene_double_sided = create_scene_pipeline(
                m_sponza_scene_shader_program, vertex_input, ECullModeKind::NONE, KERA_FRONT_FACE_CLOCKWISE,
                "Attachment Playground Sponza Scene Double-Sided Pipeline");
            if (!pipelines.sponza_scene.isValid() || !pipelines.sponza_scene_double_sided.isValid())
            {
                sampleLogError("Failed to create Attachment Playground Sponza scene pipeline: " +
                               attachmentErrorText(error));
                destroyAttachmentPipelines(pipelines);
                return false;
            }
            return true;
        }

        const VertexInputLayout vertex_input =
            VertexInputLayoutBuilder{}
                .vertexBinding<SceneVertex>(0)
                .field(KERA_VERTEX_FIELD(SceneVertex, position, 0, EVertexFormat::FLOAT3))
                .field(KERA_VERTEX_FIELD(SceneVertex, color, 0, EVertexFormat::FLOAT3))
                .layout();
        pipelines.scene =
            create_scene_pipeline(m_scene_shader_program, vertex_input, ECullModeKind::NONE,
                                  KERA_FRONT_FACE_COUNTER_CLOCKWISE, "Attachment Playground Scene Pipeline");
        if (!pipelines.scene.isValid())
        {
            sampleLogError("Failed to create Attachment Playground scene pipeline: " + attachmentErrorText(error));
            destroyAttachmentPipelines(pipelines);
            return false;
        }
        return true;
    }

    bool MultiPassRenderingSample::createMsaaReferenceAttachmentPipelines(AttachmentPipelines& pipelines)
    {
        KeraAttachmentError error{};
        const KeraTextureFormat scene_color_format = KERA_TEXTURE_FORMAT_RGBA8;
        const auto create_scene_pipeline = [&](ShaderProgramHandle shader_program,
                                               const VertexInputLayout& vertex_input, ECullModeKind cull_mode,
                                               KeraFrontFaceKind front_face, const char* debug_name)
        {
            return m_renderer.createAttachmentGraphicsPipeline(
                {
                    .struct_size = sizeof(KeraAttachmentGraphicsPipelineDesc),
                    .shader_program = shader_program,
                    .vertex_input = vertex_input.view(),
                    .color_formats = &scene_color_format,
                    .color_format_count = 1,
                    .depth_format = KERA_TEXTURE_FORMAT_DEPTH32,
                    .has_depth_attachment = 1,
                    .sample_count = KERA_ATTACHMENT_SAMPLE_COUNT_1,
                    .topology = KERA_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                    .cull_mode = static_cast<KeraCullModeKind>(cull_mode),
                    .front_face = front_face,
                    .blend_mode = KERA_BLEND_MODE_OPAQUE,
                    .depth_test = 1,
                    .depth_write = 1,
                    .debug_name = stringView(debug_name),
                },
                &error);
        };

        if (m_uses_sponza)
        {
            const VertexInputLayout vertex_input =
                VertexInputLayoutBuilder{}
                    .vertexBinding<GltfVertex>(0)
                    .field(KERA_VERTEX_FIELD(GltfVertex, position, 0, EVertexFormat::FLOAT3))
                    .field(KERA_VERTEX_FIELD(GltfVertex, normal, 0, EVertexFormat::FLOAT3))
                    .field(KERA_VERTEX_FIELD(GltfVertex, uv, 0, EVertexFormat::FLOAT2))
                    .field(KERA_VERTEX_FIELD(GltfVertex, tangent, 0, EVertexFormat::FLOAT4))
                    .layout();
            pipelines.msaa_reference_sponza_scene = create_scene_pipeline(
                m_sponza_scene_shader_program, vertex_input, ECullModeKind::BACK, KERA_FRONT_FACE_CLOCKWISE,
                "Attachment Playground MSAA Reference Sponza Scene Pipeline");
            pipelines.msaa_reference_sponza_scene_double_sided = create_scene_pipeline(
                m_sponza_scene_shader_program, vertex_input, ECullModeKind::NONE, KERA_FRONT_FACE_CLOCKWISE,
                "Attachment Playground MSAA Reference Sponza Double-Sided Pipeline");
            if (pipelines.msaa_reference_sponza_scene.isValid() &&
                pipelines.msaa_reference_sponza_scene_double_sided.isValid())
            {
                return true;
            }
        }
        else
        {
            const VertexInputLayout vertex_input =
                VertexInputLayoutBuilder{}
                    .vertexBinding<SceneVertex>(0)
                    .field(KERA_VERTEX_FIELD(SceneVertex, position, 0, EVertexFormat::FLOAT3))
                    .field(KERA_VERTEX_FIELD(SceneVertex, color, 0, EVertexFormat::FLOAT3))
                    .layout();
            pipelines.msaa_reference_scene = create_scene_pipeline(
                m_scene_shader_program, vertex_input, ECullModeKind::NONE, KERA_FRONT_FACE_COUNTER_CLOCKWISE,
                "Attachment Playground MSAA Reference Scene Pipeline");
            if (pipelines.msaa_reference_scene.isValid())
            {
                return true;
            }
        }

        sampleLogError("Failed to create Attachment Playground MSAA reference scene pipeline: " +
                       attachmentErrorText(error));
        return false;
    }

    bool MultiPassRenderingSample::recreateAttachmentResources(Extent2D extent)
    {
        return applyAttachmentConfiguration(extent, m_active_config, false);
    }

    bool MultiPassRenderingSample::createSceneDescriptor(const AttachmentPipelines& pipelines,
                                                         AttachmentResources& resources)
    {
        resources.scene_descriptor_set = m_renderer.createDescriptorSet(pipelines.scene);
        return resources.scene_descriptor_set.isValid() && m_renderer.updateDescriptors(resources.scene_descriptor_set)
                                                               .sampledImage("shadowTexture", resources.shadow)
                                                               .sampler("shadowSampler", m_scene_sampler)
                                                               .ok();
    }

    bool MultiPassRenderingSample::createMsaaReferenceSceneDescriptor(const AttachmentPipelines& pipelines,
                                                                      AttachmentResources& resources)
    {
        resources.msaa_reference_scene_descriptor_set = m_renderer.createDescriptorSet(pipelines.msaa_reference_scene);
        return resources.msaa_reference_scene_descriptor_set.isValid() &&
               m_renderer.updateDescriptors(resources.msaa_reference_scene_descriptor_set)
                   .sampledImage("shadowTexture", resources.shadow)
                   .sampler("shadowSampler", m_scene_sampler)
                   .ok();
    }

    bool MultiPassRenderingSample::createShadowComparisonSceneDescriptor(const AttachmentPipelines& pipelines,
                                                                         AttachmentResources& resources)
    {
        resources.shadow_comparison_scene_descriptor_set = m_renderer.createDescriptorSet(pipelines.scene);
        return resources.shadow_comparison_scene_descriptor_set.isValid() &&
               m_renderer.updateDescriptors(resources.shadow_comparison_scene_descriptor_set)
                   .sampledImage("shadowTexture", resources.shadow_comparison_reference)
                   .sampler("shadowSampler", m_scene_sampler)
                   .ok();
    }

    bool MultiPassRenderingSample::createSponzaDescriptors(const AttachmentPipelines& pipelines,
                                                           AttachmentResources& resources)
    {
        if (m_sponza_scene_uniform_buffers.size() != m_sponza_scene.draw_count ||
            m_sponza_shadow_diagnostic_uniform_buffers.size() != m_sponza_scene.draw_count ||
            m_sponza_shadow_uniform_buffers.size() != m_sponza_scene.draw_count)
        {
            sampleLogError("Attachment Playground Sponza per-draw uniform-buffer allocation is incomplete.");
            return false;
        }
        const KeraUniformRingBufferInfo scene_ring_info =
            m_renderer.getUniformRingBufferInfo(m_sponza_scene_uniform_buffers.front());
        const KeraUniformRingBufferInfo diagnostic_ring_info =
            m_renderer.getUniformRingBufferInfo(m_sponza_shadow_diagnostic_uniform_buffers.front());
        const KeraUniformRingBufferInfo shadow_ring_info =
            m_renderer.getUniformRingBufferInfo(m_sponza_shadow_uniform_buffers.front());
        if (scene_ring_info.slot_count == 0 || scene_ring_info.slot_count != diagnostic_ring_info.slot_count ||
            scene_ring_info.slot_count != shadow_ring_info.slot_count)
        {
            sampleLogError("Attachment Playground Sponza uniform-ring configuration is invalid.");
            return false;
        }

        const size_t draw_count = m_sponza_scene.draw_count;
        const auto create_scene_sets = [&](GraphicsPipelineHandle pipeline, std::vector<DescriptorSetHandle>& sets,
                                           const std::vector<BufferHandle>& uniform_buffers,
                                           TextureHandle shadow_texture)
        {
            sets.reserve(draw_count * scene_ring_info.slot_count);
            for (size_t draw_index = 0; draw_index < draw_count; ++draw_index)
            {
                const KeraGltfLoadedModel& draw = m_sponza_scene.draw_items[draw_index];
                for (uint32_t slot = 0; slot < scene_ring_info.slot_count; ++slot)
                {
                    const DescriptorSetHandle descriptor_set = m_renderer.createDescriptorSet(pipeline);
                    const BufferHandle uniform_buffer = uniform_buffers[draw_index];
                    const size_t uniform_offset = m_renderer.getUniformRingBufferSlotOffset(uniform_buffer, slot);
                    if (!descriptor_set.isValid() ||
                        !m_renderer.updateDescriptors(descriptor_set)
                             .uniform<SponzaSceneUniforms>(multiPassSponzaShader::kSceneParams, uniform_buffer,
                                                           uniform_offset)
                             .sampledImage(multiPassSponzaShader::kBaseColorTexture, draw.material_textures.base_color)
                             .sampledImage(multiPassSponzaShader::kMetalRoughnessTexture,
                                           draw.material_textures.metal_roughness)
                             .sampledImage(multiPassSponzaShader::kEmissiveTexture, draw.material_textures.emissive)
                             .sampledImage(multiPassSponzaShader::kOcclusionTexture, draw.material_textures.occlusion)
                             .sampledImage(multiPassSponzaShader::kNormalTexture, draw.material_textures.normal)
                             .sampler(multiPassSponzaShader::kMaterialSampler, draw.material_sampler)
                             .sampledImage(multiPassSponzaShader::kShadowTexture, shadow_texture)
                             .sampler(multiPassSponzaShader::kShadowSampler, m_shadow_inset_sampler)
                             .ok())
                    {
                        return false;
                    }
                    sets.push_back(descriptor_set);
                }
            }
            return true;
        };
        const auto create_shadow_sets = [&](GraphicsPipelineHandle pipeline, std::vector<DescriptorSetHandle>& sets)
        {
            sets.reserve(draw_count * shadow_ring_info.slot_count);
            for (size_t draw_index = 0; draw_index < draw_count; ++draw_index)
            {
                const KeraGltfLoadedModel& draw = m_sponza_scene.draw_items[draw_index];
                for (uint32_t slot = 0; slot < shadow_ring_info.slot_count; ++slot)
                {
                    const DescriptorSetHandle descriptor_set = m_renderer.createDescriptorSet(pipeline);
                    const BufferHandle uniform_buffer = m_sponza_shadow_uniform_buffers[draw_index];
                    const size_t uniform_offset = m_renderer.getUniformRingBufferSlotOffset(uniform_buffer, slot);
                    if (!descriptor_set.isValid() ||
                        !m_renderer.updateDescriptors(descriptor_set)
                             .uniform<SponzaShadowUniforms>(multiPassSponzaShader::kShadowParams, uniform_buffer,
                                                            uniform_offset)
                             .sampledImage(multiPassSponzaShader::kBaseColorTexture, draw.material_textures.base_color)
                             .sampler(multiPassSponzaShader::kMaterialSampler, draw.material_sampler)
                             .ok())
                    {
                        return false;
                    }
                    sets.push_back(descriptor_set);
                }
            }
            return true;
        };

        const bool primary_scene_descriptors_ok =
            create_scene_sets(pipelines.sponza_scene, resources.sponza_scene_descriptor_sets,
                              m_sponza_scene_uniform_buffers, resources.shadow) &&
            create_scene_sets(pipelines.sponza_scene_double_sided, resources.sponza_scene_double_sided_descriptor_sets,
                              m_sponza_scene_uniform_buffers, resources.shadow);
        const bool msaa_reference_scene_descriptors_ok =
            !pipelines.msaa_reference_sponza_scene.isValid() ||
            (create_scene_sets(pipelines.msaa_reference_sponza_scene,
                               resources.msaa_reference_sponza_scene_descriptor_sets, m_sponza_scene_uniform_buffers,
                               resources.shadow) &&
             create_scene_sets(pipelines.msaa_reference_sponza_scene_double_sided,
                               resources.msaa_reference_sponza_scene_double_sided_descriptor_sets,
                               m_sponza_scene_uniform_buffers, resources.shadow));
        const bool shadow_comparison_scene_descriptors_ok =
            !resources.shadow_comparison_reference.isValid() ||
            (create_scene_sets(pipelines.sponza_scene, resources.shadow_comparison_active_sponza_scene_descriptor_sets,
                               m_sponza_shadow_diagnostic_uniform_buffers, resources.shadow) &&
             create_scene_sets(pipelines.sponza_scene_double_sided,
                               resources.shadow_comparison_active_sponza_scene_double_sided_descriptor_sets,
                               m_sponza_shadow_diagnostic_uniform_buffers, resources.shadow) &&
             create_scene_sets(pipelines.sponza_scene, resources.shadow_comparison_sponza_scene_descriptor_sets,
                               m_sponza_shadow_diagnostic_uniform_buffers, resources.shadow_comparison_reference) &&
             create_scene_sets(pipelines.sponza_scene_double_sided,
                               resources.shadow_comparison_sponza_scene_double_sided_descriptor_sets,
                               m_sponza_shadow_diagnostic_uniform_buffers, resources.shadow_comparison_reference));
        return primary_scene_descriptors_ok && msaa_reference_scene_descriptors_ok &&
               shadow_comparison_scene_descriptors_ok &&
               create_shadow_sets(m_sponza_shadow_pipeline, resources.sponza_shadow_descriptor_sets) &&
               create_shadow_sets(m_sponza_shadow_double_sided_pipeline,
                                  resources.sponza_shadow_double_sided_descriptor_sets);
    }

    bool MultiPassRenderingSample::createCompositeDescriptor(AttachmentResources& resources)
    {
        resources.composite_descriptor_set = m_renderer.createDescriptorSet(m_composite_pipeline);
        return resources.composite_descriptor_set.isValid() &&
               m_renderer.updateDescriptors(resources.composite_descriptor_set)
                   .sampledImage("sceneTexture", resources.scene)
                   .sampler("sceneSampler", m_scene_sampler)
                   .ok();
    }

    bool MultiPassRenderingSample::createShadowPreviewDescriptor(AttachmentResources& resources)
    {
        resources.shadow_preview_descriptor_set = m_renderer.createDescriptorSet(m_shadow_preview_pipeline);
        return resources.shadow_preview_descriptor_set.isValid() &&
               m_renderer.updateDescriptors(resources.shadow_preview_descriptor_set)
                   .sampledImage("shadowPreviewTexture", resources.shadow)
                   .sampler("shadowPreviewSampler", m_shadow_inset_sampler)
                   .ok();
    }

    bool MultiPassRenderingSample::createShadowInsetDescriptor(AttachmentResources& resources)
    {
        resources.shadow_inset_descriptor_set = m_renderer.createDescriptorSet(m_shadow_inset_pipeline);
        return resources.shadow_inset_descriptor_set.isValid() &&
               m_renderer.updateDescriptors(resources.shadow_inset_descriptor_set)
                   .sampledImage("sceneTexture", resources.scene)
                   .sampler("sceneSampler", m_scene_sampler)
                   .sampledImage("shadowPreviewTexture", resources.shadow)
                   .sampler("shadowPreviewSampler", m_shadow_inset_sampler)
                   .ok();
    }

    bool MultiPassRenderingSample::createMsaaComparisonDescriptor(AttachmentResources& resources)
    {
        resources.msaa_comparison_descriptor_set = m_renderer.createDescriptorSet(m_msaa_comparison_pipeline);
        return resources.msaa_comparison_descriptor_set.isValid() &&
               m_renderer.updateDescriptors(resources.msaa_comparison_descriptor_set)
                   .sampledImage("sceneTexture", resources.scene)
                   .sampler("sceneSampler", m_shadow_inset_sampler)
                   .sampledImage("msaaReferenceTexture", resources.msaa_reference_scene)
                   .sampler("msaaReferenceSampler", m_shadow_inset_sampler)
                   .ok();
    }

    bool MultiPassRenderingSample::createShadowComparisonDescriptor(AttachmentResources& resources)
    {
        resources.shadow_comparison_descriptor_set = m_renderer.createDescriptorSet(m_shadow_comparison_pipeline);
        return resources.shadow_comparison_descriptor_set.isValid() &&
               m_renderer.updateDescriptors(resources.shadow_comparison_descriptor_set)
                   .sampledImage("sceneTexture", resources.scene)
                   .sampler("sceneSampler", m_scene_sampler)
                   .sampledImage("shadowActiveTexture", resources.shadow_comparison_active_scene)
                   .sampler("shadowActiveSampler", m_shadow_inset_sampler)
                   .sampledImage("shadowReferenceTexture", resources.msaa_reference_scene)
                   .sampler("shadowReferenceSampler", m_shadow_inset_sampler)
                   .ok();
    }

    bool MultiPassRenderingSample::buildAttachmentResources(Extent2D extent, KeraAttachmentSampleCount sample_count,
                                                            const AttachmentPlaygroundConfig& config,
                                                            AttachmentPipelines& pipelines,
                                                            AttachmentResources& resources)
    {
        if (extent.width == 0 || extent.height == 0 || config.shadow_resolution == 0)
        {
            return false;
        }

        KeraAttachmentError error{};
        resources.scene = m_renderer.createAttachmentTexture(
            {
                .struct_size = sizeof(KeraAttachmentTextureDesc),
                .width = extent.width,
                .height = extent.height,
                .format = KERA_TEXTURE_FORMAT_RGBA8,
                .usage_flags = KERA_ATTACHMENT_TEXTURE_USAGE_COLOR_ATTACHMENT | KERA_ATTACHMENT_TEXTURE_USAGE_SAMPLED |
                               KERA_ATTACHMENT_TEXTURE_USAGE_TRANSFER_SRC,
                .sample_count = KERA_ATTACHMENT_SAMPLE_COUNT_1,
                .debug_name = stringView("Attachment Playground Resolved Colour"),
            },
            &error);
        if (!resources.scene.isValid())
        {
            sampleLogError("Failed to create Attachment Playground resolved colour: " + attachmentErrorText(error));
            return false;
        }

        if (sample_count != KERA_ATTACHMENT_SAMPLE_COUNT_1)
        {
            resources.scene_msaa = m_renderer.createAttachmentTexture(
                {
                    .struct_size = sizeof(KeraAttachmentTextureDesc),
                    .width = extent.width,
                    .height = extent.height,
                    .format = KERA_TEXTURE_FORMAT_RGBA8,
                    .usage_flags = KERA_ATTACHMENT_TEXTURE_USAGE_COLOR_ATTACHMENT,
                    .sample_count = sample_count,
                    .debug_name = stringView("Attachment Playground MSAA Colour"),
                },
                &error);
            if (!resources.scene_msaa.isValid())
            {
                sampleLogError("Failed to create Attachment Playground MSAA colour: " + attachmentErrorText(error));
                destroyAttachmentResources(resources);
                return false;
            }
        }

        resources.scene_depth = m_renderer.createAttachmentTexture(
            {
                .struct_size = sizeof(KeraAttachmentTextureDesc),
                .width = extent.width,
                .height = extent.height,
                .format = KERA_TEXTURE_FORMAT_DEPTH32,
                .usage_flags = KERA_ATTACHMENT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                .sample_count = sample_count,
                .debug_name = stringView("Attachment Playground Scene Depth"),
            },
            &error);
        if (!resources.scene_depth.isValid())
        {
            sampleLogError("Failed to create Attachment Playground scene depth: " + attachmentErrorText(error));
            destroyAttachmentResources(resources);
            return false;
        }

        resources.shadow = m_renderer.createAttachmentTexture(
            {
                .struct_size = sizeof(KeraAttachmentTextureDesc),
                .width = config.shadow_resolution,
                .height = config.shadow_resolution,
                .format = KERA_TEXTURE_FORMAT_DEPTH32,
                .usage_flags =
                    KERA_ATTACHMENT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | KERA_ATTACHMENT_TEXTURE_USAGE_SAMPLED,
                .sample_count = KERA_ATTACHMENT_SAMPLE_COUNT_1,
                .debug_name = stringView("Attachment Playground Shadow Map"),
            },
            &error);
        if (!resources.shadow.isValid())
        {
            sampleLogError("Failed to create Attachment Playground shadow map: " + attachmentErrorText(error));
            destroyAttachmentResources(resources);
            return false;
        }

        const bool needs_msaa_reference =
            config.preview_msaa_comparison && sample_count != KERA_ATTACHMENT_SAMPLE_COUNT_1;
        const bool needs_shadow_comparison = config.preview_shadow_comparison && config.shadows_enabled &&
                                             config.shadow_resolution > kShadowComparisonReferenceResolution;
        if (needs_shadow_comparison)
        {
            resources.shadow_comparison_reference = m_renderer.createAttachmentTexture(
                {
                    .struct_size = sizeof(KeraAttachmentTextureDesc),
                    .width = kShadowComparisonReferenceResolution,
                    .height = kShadowComparisonReferenceResolution,
                    .format = KERA_TEXTURE_FORMAT_DEPTH32,
                    .usage_flags =
                        KERA_ATTACHMENT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | KERA_ATTACHMENT_TEXTURE_USAGE_SAMPLED,
                    .sample_count = KERA_ATTACHMENT_SAMPLE_COUNT_1,
                    .debug_name = stringView("Attachment Playground Shadow Comparison Reference"),
                },
                &error);
            if (!resources.shadow_comparison_reference.isValid())
            {
                sampleLogError("Failed to create Attachment Playground shadow comparison reference: " +
                               attachmentErrorText(error));
                destroyAttachmentResources(resources);
                return false;
            }
        }

        const bool needs_reference_scene = needs_msaa_reference || needs_shadow_comparison;
        if (needs_reference_scene)
        {
            resources.msaa_reference_scene = m_renderer.createAttachmentTexture(
                {
                    .struct_size = sizeof(KeraAttachmentTextureDesc),
                    .width = extent.width,
                    .height = extent.height,
                    .format = KERA_TEXTURE_FORMAT_RGBA8,
                    .usage_flags =
                        KERA_ATTACHMENT_TEXTURE_USAGE_COLOR_ATTACHMENT | KERA_ATTACHMENT_TEXTURE_USAGE_SAMPLED,
                    .sample_count = KERA_ATTACHMENT_SAMPLE_COUNT_1,
                    .debug_name = stringView("Attachment Playground Diagnostic Reference Colour"),
                },
                &error);
            resources.msaa_reference_scene_depth = m_renderer.createAttachmentTexture(
                {
                    .struct_size = sizeof(KeraAttachmentTextureDesc),
                    .width = extent.width,
                    .height = extent.height,
                    .format = KERA_TEXTURE_FORMAT_DEPTH32,
                    .usage_flags = KERA_ATTACHMENT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                    .sample_count = KERA_ATTACHMENT_SAMPLE_COUNT_1,
                    .debug_name = stringView("Attachment Playground Diagnostic Reference Depth"),
                },
                &error);
            if (!resources.msaa_reference_scene.isValid() || !resources.msaa_reference_scene_depth.isValid() ||
                (needs_msaa_reference && !createMsaaReferenceAttachmentPipelines(pipelines)))
            {
                sampleLogError("Failed to create Attachment Playground diagnostic reference attachments: " +
                               attachmentErrorText(error));
                destroyAttachmentResources(resources);
                return false;
            }
        }

        if (needs_shadow_comparison)
        {
            resources.shadow_comparison_active_scene = m_renderer.createAttachmentTexture(
                {
                    .struct_size = sizeof(KeraAttachmentTextureDesc),
                    .width = extent.width,
                    .height = extent.height,
                    .format = KERA_TEXTURE_FORMAT_RGBA8,
                    .usage_flags =
                        KERA_ATTACHMENT_TEXTURE_USAGE_COLOR_ATTACHMENT | KERA_ATTACHMENT_TEXTURE_USAGE_SAMPLED,
                    .sample_count = KERA_ATTACHMENT_SAMPLE_COUNT_1,
                    .debug_name = stringView("Attachment Playground Shadow Comparison Active Colour"),
                },
                &error);
            resources.shadow_comparison_active_scene_depth = m_renderer.createAttachmentTexture(
                {
                    .struct_size = sizeof(KeraAttachmentTextureDesc),
                    .width = extent.width,
                    .height = extent.height,
                    .format = KERA_TEXTURE_FORMAT_DEPTH32,
                    .usage_flags = KERA_ATTACHMENT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                    .sample_count = sample_count,
                    .debug_name = stringView("Attachment Playground Shadow Comparison Active Depth"),
                },
                &error);
            if (!resources.shadow_comparison_active_scene.isValid() ||
                !resources.shadow_comparison_active_scene_depth.isValid())
            {
                sampleLogError("Failed to create Attachment Playground shadow comparison active attachments: " +
                               attachmentErrorText(error));
                destroyAttachmentResources(resources);
                return false;
            }
        }

        if (needs_shadow_comparison && sample_count != KERA_ATTACHMENT_SAMPLE_COUNT_1)
        {
            resources.shadow_comparison_active_scene_msaa = m_renderer.createAttachmentTexture(
                {
                    .struct_size = sizeof(KeraAttachmentTextureDesc),
                    .width = extent.width,
                    .height = extent.height,
                    .format = KERA_TEXTURE_FORMAT_RGBA8,
                    .usage_flags = KERA_ATTACHMENT_TEXTURE_USAGE_COLOR_ATTACHMENT,
                    .sample_count = sample_count,
                    .debug_name = stringView("Attachment Playground Shadow Comparison Active MSAA Colour"),
                },
                &error);
            resources.shadow_comparison_scene_msaa = m_renderer.createAttachmentTexture(
                {
                    .struct_size = sizeof(KeraAttachmentTextureDesc),
                    .width = extent.width,
                    .height = extent.height,
                    .format = KERA_TEXTURE_FORMAT_RGBA8,
                    .usage_flags = KERA_ATTACHMENT_TEXTURE_USAGE_COLOR_ATTACHMENT,
                    .sample_count = sample_count,
                    .debug_name = stringView("Attachment Playground Shadow Comparison Reference MSAA Colour"),
                },
                &error);
            resources.shadow_comparison_scene_depth = m_renderer.createAttachmentTexture(
                {
                    .struct_size = sizeof(KeraAttachmentTextureDesc),
                    .width = extent.width,
                    .height = extent.height,
                    .format = KERA_TEXTURE_FORMAT_DEPTH32,
                    .usage_flags = KERA_ATTACHMENT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                    .sample_count = sample_count,
                    .debug_name = stringView("Attachment Playground Shadow Comparison Reference MSAA Depth"),
                },
                &error);
            if (!resources.shadow_comparison_active_scene_msaa.isValid() ||
                !resources.shadow_comparison_scene_msaa.isValid() || !resources.shadow_comparison_scene_depth.isValid())
            {
                sampleLogError("Failed to create Attachment Playground shadow comparison MSAA attachments: " +
                               attachmentErrorText(error));
                destroyAttachmentResources(resources);
                return false;
            }
        }

        const bool reference_scene_descriptors_ok =
            !needs_reference_scene || m_uses_sponza ||
            ((!needs_msaa_reference || createMsaaReferenceSceneDescriptor(pipelines, resources)) &&
             (!needs_shadow_comparison || createShadowComparisonSceneDescriptor(pipelines, resources)));
        const bool descriptors_ok = (m_uses_sponza ? createSponzaDescriptors(pipelines, resources)
                                                   : createSceneDescriptor(pipelines, resources)) &&
                                    reference_scene_descriptors_ok && createCompositeDescriptor(resources) &&
                                    createShadowPreviewDescriptor(resources) &&
                                    createShadowInsetDescriptor(resources) &&
                                    (!needs_msaa_reference || createMsaaComparisonDescriptor(resources)) &&
                                    (!needs_shadow_comparison || createShadowComparisonDescriptor(resources));
        if (!descriptors_ok)
        {
            sampleLogError("Failed to create Attachment Playground descriptor sets.");
            destroyAttachmentResources(resources);
        }
        return descriptors_ok;
    }

    KeraAttachmentSampleCount MultiPassRenderingSample::selectAttachmentSampleCount(
        Extent2D extent, const AttachmentPlaygroundConfig& config, AttachmentPipelines& pipelines,
        AttachmentResources& resources)
    {
        constexpr std::array<KeraAttachmentSampleCount, 4> kCandidates = {
            KERA_ATTACHMENT_SAMPLE_COUNT_8,
            KERA_ATTACHMENT_SAMPLE_COUNT_4,
            KERA_ATTACHMENT_SAMPLE_COUNT_2,
            KERA_ATTACHMENT_SAMPLE_COUNT_1,
        };

        const KeraAttachmentCapabilities capabilities = m_renderer.getAttachmentCapabilities();
        for (const KeraAttachmentSampleCount candidate : kCandidates)
        {
            const uint32_t candidate_value = attachmentSampleCountValue(candidate);
            if (config.requested_msaa_samples != 0 && candidate_value > config.requested_msaa_samples)
            {
                continue;
            }

            if ((capabilities.supported_sample_counts & candidate) == 0 ||
                (candidate != KERA_ATTACHMENT_SAMPLE_COUNT_1 && !m_renderer.supportsAttachmentResolve()))
            {
                continue;
            }

            AttachmentPipelines candidate_pipelines{};
            AttachmentResources candidate_resources{};
            if (!createAttachmentPipelines(candidate, candidate_pipelines))
            {
                continue;
            }

            if (!buildAttachmentResources(extent, candidate, config, candidate_pipelines, candidate_resources))
            {
                destroyAttachmentPipelines(candidate_pipelines);
                continue;
            }

            pipelines = std::move(candidate_pipelines);
            resources = std::move(candidate_resources);
            return candidate;
        }
        return static_cast<KeraAttachmentSampleCount>(0);
    }

    bool MultiPassRenderingSample::applyAttachmentConfiguration(Extent2D extent,
                                                                const AttachmentPlaygroundConfig& config,
                                                                bool wait_for_idle)
    {
        if (extent.width == 0 || extent.height == 0)
        {
            return false;
        }
        if (wait_for_idle && !m_renderer.resize(extent))
        {
            sampleLogError("Failed to reach a frame-safe boundary for Attachment Playground reconfiguration.");
            return false;
        }

        AttachmentPipelines candidate_pipelines{};
        AttachmentResources candidate_resources{};
        const KeraAttachmentSampleCount selected_sample_count =
            selectAttachmentSampleCount(extent, config, candidate_pipelines, candidate_resources);
        if (selected_sample_count == static_cast<KeraAttachmentSampleCount>(0))
        {
            sampleLogError("Attachment Playground could not build a complete requested MSAA/shadow configuration.");
            return false;
        }

        if (!destroyAttachmentResources(m_attachment_resources) || !destroyAttachmentPipelines(m_attachment_pipelines))
        {
            sampleLogError("Attachment Playground could not retire the prior configuration safely.");
            destroyAttachmentResources(candidate_resources);
            destroyAttachmentPipelines(candidate_pipelines);
            m_initialized = false;
            return false;
        }

        m_attachment_pipelines = std::move(candidate_pipelines);
        m_attachment_resources = std::move(candidate_resources);
        m_render_extent = extent;
        m_scene_sample_count = selected_sample_count;
        m_active_config = config;

        const uint32_t actual_samples = attachmentSampleCountValue(m_scene_sample_count);
        if (config.requested_msaa_samples != 0 && actual_samples != config.requested_msaa_samples)
        {
            sampleLogWarning("Attachment Playground requested " + std::to_string(config.requested_msaa_samples) +
                             "x MSAA and selected " + std::to_string(actual_samples) + "x.");
        }
        else
        {
            sampleLogInfo("Attachment Playground selected " + std::to_string(actual_samples) + "x MSAA.");
        }
        logAttachmentConfiguration(config, actual_samples, extent);
        return true;
    }

    bool MultiPassRenderingSample::applyPendingConfiguration()
    {
        if (!m_config_dirty || attachmentConfigsEqual(m_active_config, m_requested_config))
        {
            m_config_dirty = false;
            return true;
        }

        if (!attachmentResourcesDiffer(m_active_config, m_requested_config))
        {
            m_active_config = m_requested_config;
            m_config_dirty = false;
            m_configuration_error.clear();
            sampleLogInfo("Attachment Playground updated shadows and diagnostic view without recreating attachments.");
            logAttachmentConfiguration(m_active_config, attachmentSampleCountValue(m_scene_sample_count),
                                       m_render_extent);
            return true;
        }

        if (!applyAttachmentConfiguration(m_render_extent, m_requested_config, true))
        {
            if (m_initialized)
            {
                m_configuration_error = "Requested configuration was rejected; active settings were retained.";
                m_requested_config = m_active_config;
                m_config_dirty = false;
                sampleLogError(
                    "Attachment Playground configuration change was rejected; active settings were retained.");
            }
            return false;
        }
        m_config_dirty = false;
        m_configuration_error.clear();
        return true;
    }

    bool MultiPassRenderingSample::destroyAttachmentResources(AttachmentResources& resources)
    {
        bool success = true;
        const auto destroy_descriptor_sets = [this, &success](std::vector<DescriptorSetHandle>& descriptor_sets)
        {
            for (DescriptorSetHandle& descriptor_set : descriptor_sets)
            {
                if (descriptor_set.isValid() && !m_renderer.destroyDescriptorSet(descriptor_set))
                {
                    success = false;
                }
            }
            if (success)
            {
                descriptor_sets.clear();
            }
        };
        destroy_descriptor_sets(resources.shadow_comparison_active_sponza_scene_descriptor_sets);
        destroy_descriptor_sets(resources.shadow_comparison_active_sponza_scene_double_sided_descriptor_sets);
        destroy_descriptor_sets(resources.shadow_comparison_sponza_scene_descriptor_sets);
        destroy_descriptor_sets(resources.shadow_comparison_sponza_scene_double_sided_descriptor_sets);
        destroy_descriptor_sets(resources.msaa_reference_sponza_scene_descriptor_sets);
        destroy_descriptor_sets(resources.msaa_reference_sponza_scene_double_sided_descriptor_sets);
        destroy_descriptor_sets(resources.sponza_scene_descriptor_sets);
        destroy_descriptor_sets(resources.sponza_scene_double_sided_descriptor_sets);
        destroy_descriptor_sets(resources.sponza_shadow_descriptor_sets);
        destroy_descriptor_sets(resources.sponza_shadow_double_sided_descriptor_sets);

        const auto destroy_descriptor_set = [this, &success](DescriptorSetHandle& descriptor_set)
        {
            if (descriptor_set.isValid() && !m_renderer.destroyDescriptorSet(descriptor_set))
            {
                success = false;
                return;
            }
            descriptor_set = {};
        };
        destroy_descriptor_set(resources.shadow_comparison_scene_descriptor_set);
        destroy_descriptor_set(resources.msaa_reference_scene_descriptor_set);
        destroy_descriptor_set(resources.shadow_comparison_descriptor_set);
        destroy_descriptor_set(resources.msaa_comparison_descriptor_set);
        destroy_descriptor_set(resources.scene_descriptor_set);
        destroy_descriptor_set(resources.shadow_inset_descriptor_set);
        destroy_descriptor_set(resources.shadow_preview_descriptor_set);
        destroy_descriptor_set(resources.composite_descriptor_set);

        const auto destroy_texture = [this, &success](TextureHandle& texture)
        {
            if (texture.isValid() && !m_renderer.destroyTexture(texture))
            {
                success = false;
                return;
            }
            texture = {};
        };
        destroy_texture(resources.shadow_comparison_scene_depth);
        destroy_texture(resources.shadow_comparison_scene_msaa);
        destroy_texture(resources.shadow_comparison_active_scene_depth);
        destroy_texture(resources.shadow_comparison_active_scene_msaa);
        destroy_texture(resources.shadow_comparison_active_scene);
        destroy_texture(resources.msaa_reference_scene_depth);
        destroy_texture(resources.msaa_reference_scene);
        destroy_texture(resources.shadow_comparison_reference);
        destroy_texture(resources.shadow);
        destroy_texture(resources.scene_depth);
        destroy_texture(resources.scene_msaa);
        destroy_texture(resources.scene);
        return success;
    }

    bool MultiPassRenderingSample::destroyAttachmentPipelines(AttachmentPipelines& pipelines)
    {
        bool success = true;
        const auto destroy_pipeline = [this, &success](GraphicsPipelineHandle& pipeline)
        {
            if (pipeline.isValid() && !m_renderer.destroyGraphicsPipeline(pipeline))
            {
                success = false;
                return;
            }
            pipeline = {};
        };
        destroy_pipeline(pipelines.msaa_reference_sponza_scene_double_sided);
        destroy_pipeline(pipelines.msaa_reference_sponza_scene);
        destroy_pipeline(pipelines.msaa_reference_scene);
        destroy_pipeline(pipelines.sponza_scene_double_sided);
        destroy_pipeline(pipelines.sponza_scene);
        destroy_pipeline(pipelines.scene);
        return success;
    }

    void MultiPassRenderingSample::update(float delta_time)
    {
        m_elapsed_seconds += delta_time;
        if (m_uses_sponza && m_sun_orbit_enabled)
        {
            constexpr float kFullTurnRadians = 6.28318530718f;
            const float elapsed_seconds = std::clamp(delta_time, 0.0f, 0.25f);
            const float period_seconds = std::max(m_sun_orbit_period_seconds, 1.0f);
            m_sun_orbit_phase_radians = std::fmod(
                m_sun_orbit_phase_radians + elapsed_seconds * kFullTurnRadians / period_seconds, kFullTurnRadians);
        }
        ++m_update_count;

        if (m_reconfigure_smoke && !m_config_dirty)
        {
            if (m_reconfigure_smoke_stage == 0 && m_update_count >= 2)
            {
                m_requested_config.requested_msaa_samples = 1;
                m_config_dirty = true;
                ++m_reconfigure_smoke_stage;
            }
            else if (m_reconfigure_smoke_stage == 1 && m_update_count >= 3)
            {
                m_requested_config.requested_msaa_samples = 4;
                m_config_dirty = true;
                ++m_reconfigure_smoke_stage;
            }
            else if (m_reconfigure_smoke_stage == 2 && m_update_count >= 4)
            {
                m_requested_config.shadow_resolution = 1024;
                m_config_dirty = true;
                ++m_reconfigure_smoke_stage;
            }
            else if (m_reconfigure_smoke_stage == 3 && m_update_count >= 5)
            {
                m_requested_config.shadows_enabled = false;
                m_config_dirty = true;
                ++m_reconfigure_smoke_stage;
            }
            else if (m_reconfigure_smoke_stage == 4 && m_update_count >= 6)
            {
                m_requested_config.shadows_enabled = true;
                m_requested_config.preview_shadow_map = true;
                m_config_dirty = true;
                ++m_reconfigure_smoke_stage;
            }
            else if (m_reconfigure_smoke_stage == 5 && m_update_count >= 7)
            {
                m_requested_config.preview_shadow_map = false;
                m_config_dirty = true;
                ++m_reconfigure_smoke_stage;
            }
        }

        const bool configuration_applied = applyPendingConfiguration();
        if (m_reconfigure_smoke && m_reconfigure_smoke_stage == 6 && configuration_applied)
        {
            sampleLogInfo("ATTACHMENT_PLAYGROUND_RECONFIGURE_SMOKE complete active_msaa=" +
                          std::to_string(attachmentSampleCountValue(m_scene_sample_count)) +
                          " shadow_resolution=" + std::to_string(m_active_config.shadow_resolution) +
                          " shadows=" + std::string(m_active_config.shadows_enabled ? "on" : "off") +
                          " preview=" + std::string(m_active_config.preview_shadow_map ? "on" : "off"));
            ++m_reconfigure_smoke_stage;
        }
    }

    void MultiPassRenderingSample::resize(Extent2D extent)
    {
        if (extent.width == 0 || extent.height == 0 || !m_initialized)
        {
            return;
        }
        if (!recreateAttachmentResources(extent))
        {
            sampleLogError(
                "Attachment Playground retained its prior configuration after resize reconfiguration failed.");
            return;
        }
        ++m_non_zero_resize_count;
        m_capture_after_resize_frame_pending = m_capture_after_resize_smoke && m_non_zero_resize_count >= 3;
    }

    void MultiPassRenderingSample::drawUi()
    {
#ifdef KERA_HAS_IMGUI
        const ImVec2 display_size = ImGui::GetIO().DisplaySize;
        const ImVec2 default_position(12.0f, 180.0f);
        const ImVec2 maximum_size(std::max(320.0f, display_size.x - 24.0f),
                                  std::max(300.0f, display_size.y - default_position.y - 12.0f));
        const ImVec2 default_size(std::min(620.0f, maximum_size.x), std::min(520.0f, maximum_size.y));
        const ImVec2 minimum_size(std::min(520.0f, maximum_size.x), std::min(320.0f, maximum_size.y));
        ImGui::SetNextWindowPos(default_position, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(default_size, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(minimum_size, maximum_size);
        ImGui::Begin("Attachment Playground###attachment-playground-v2");

        const ImVec2 panel_position = ImGui::GetWindowPos();
        const ImVec2 panel_size = ImGui::GetWindowSize();
        const bool panel_is_offscreen =
            panel_position.x + panel_size.x < 32.0f || panel_position.y + panel_size.y < 32.0f ||
            panel_position.x > display_size.x - 32.0f || panel_position.y > display_size.y - 32.0f;
        if (panel_is_offscreen)
        {
            ImGui::SetWindowPos(default_position, ImGuiCond_Always);
            ImGui::SetWindowSize(default_size, ImGuiCond_Always);
        }

        constexpr const char* kMsaaOptions[] = {"Auto", "1x", "2x", "4x", "8x"};
        int msaa_index = 3;
        switch (m_requested_config.requested_msaa_samples)
        {
            case 0:
                msaa_index = 0;
                break;
            case 1:
                msaa_index = 1;
                break;
            case 2:
                msaa_index = 2;
                break;
            case 8:
                msaa_index = 4;
                break;
            default:
                break;
        }

        if (ImGui::Combo("Requested MSAA", &msaa_index, kMsaaOptions, IM_ARRAYSIZE(kMsaaOptions)))
        {
            constexpr uint32_t kMsaaValues[] = {0, 1, 2, 4, 8};
            m_requested_config.requested_msaa_samples = kMsaaValues[msaa_index];
        }

        ImGui::Checkbox("Shadows", &m_requested_config.shadows_enabled);
        if (ImGui::Checkbox("Preview Shadow Map Full Screen", &m_requested_config.preview_shadow_map) &&
            m_requested_config.preview_shadow_map)
        {
            m_requested_config.preview_shadow_inset = false;
            m_requested_config.preview_msaa_comparison = false;
            m_requested_config.preview_shadow_comparison = false;
        }

        if (ImGui::Checkbox("Preview Shadow Map", &m_requested_config.preview_shadow_inset) &&
            m_requested_config.preview_shadow_inset)
        {
            m_requested_config.preview_shadow_map = false;
            m_requested_config.preview_msaa_comparison = false;
            m_requested_config.preview_shadow_comparison = false;
        }

        if (ImGui::Checkbox("Preview MSAA Comparison", &m_requested_config.preview_msaa_comparison) &&
            m_requested_config.preview_msaa_comparison)
        {
            m_requested_config.preview_shadow_map = false;
            m_requested_config.preview_shadow_inset = false;
            m_requested_config.preview_shadow_comparison = false;
        }

        if (ImGui::Checkbox("Prewview Shadow Comparison", &m_requested_config.preview_shadow_comparison) &&
            m_requested_config.preview_shadow_comparison)
        {
            m_requested_config.preview_shadow_map = false;
            m_requested_config.preview_shadow_inset = false;
            m_requested_config.preview_msaa_comparison = false;
        }

        constexpr const char* kShadowOptions[] = {"1024 px", "2048 px", "4096 px"};
        int shadow_index =
            m_requested_config.shadow_resolution == 1024 ? 0 : (m_requested_config.shadow_resolution == 4096 ? 2 : 1);
        if (ImGui::Combo("Shadow Map Resolution", &shadow_index, kShadowOptions, IM_ARRAYSIZE(kShadowOptions)))
        {
            constexpr uint32_t kShadowResolutions[] = {1024, 2048, 4096};
            m_requested_config.shadow_resolution = kShadowResolutions[shadow_index];
        }

        if (m_uses_sponza)
        {
            ImGui::Checkbox("Animate sun", &m_sun_orbit_enabled);
            ImGui::SliderFloat("Sun orbit period", &m_sun_orbit_period_seconds, 30.0f, 300.0f, "%.0f s",
                               ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            if (ImGui::Button("Reset sun"))
            {
                m_sun_orbit_phase_radians = 0.0f;
            }
            ImGui::Text("Sun orbit: %s (%.0f s)", m_sun_orbit_enabled ? "running" : "paused",
                        m_sun_orbit_period_seconds);
        }

        if (ImGui::Button("Reset"))
        {
            m_requested_config = {};
        }
        ImGui::Separator();
        ImGui::Text("Active MSAA: %ux", attachmentSampleCountValue(m_scene_sample_count));
        ImGui::Text("Active Shadow Map: %u px", m_active_config.shadow_resolution);
        if (m_active_config.preview_msaa_comparison)
        {
            if (m_attachment_resources.msaa_reference_scene.isValid())
            {
                ImGui::Text("MSAA lens: 1x reference / %ux resolved", attachmentSampleCountValue(m_scene_sample_count));
            }
            else
            {
                ImGui::TextDisabled("MSAA lens needs active MSAA above 1x");
            }
        }

        if (m_active_config.preview_shadow_comparison)
        {
            if (m_attachment_resources.shadow_comparison_reference.isValid())
            {
                ImGui::Text("Shadow lens: 1024 px reference / %u px active", m_active_config.shadow_resolution);
                ImGui::TextUnformatted("Target: Sponza brick wall shadow transition");
            }
            else
            {
                ImGui::TextDisabled("Shadow lens needs Shadows on and a map above 1024 px");
            }
        }

        ImGui::TextUnformatted(m_uses_sponza ? "Scene: Sponza" : "Scene: procedural fallback");
        if (m_config_dirty)
        {
            ImGui::TextDisabled("Applying requested configuration...");
        }
        else if (!m_configuration_error.empty())
        {
            ImGui::TextDisabled("%s", m_configuration_error.c_str());
        }

        ImGui::End();

        const bool configuration_changed = !attachmentConfigsEqual(m_active_config, m_requested_config);
        if (configuration_changed)
        {
            m_configuration_error.clear();
        }
        m_config_dirty = configuration_changed;
#endif
    }

    bool MultiPassRenderingSample::uploadSponzaUniforms(FrameHandle frame)
    {
        if (!m_uses_sponza)
        {
            return true;
        }

        const float aspect = m_render_extent.height == 0 ? 16.0f / 9.0f
                                                         : static_cast<float>(m_render_extent.width) /
                                                               static_cast<float>(m_render_extent.height);
        // The canonical Sponza node applies a 0.008 scale to the raw accessor bounds.
        const glm::vec3 camera_target(8.50f, 4.25f, -0.31f);
        const glm::vec3 camera_position(-9.50f, 1.80f, -0.31f);
        // Match the axial Sponza view that exposes the far brick receiver's cast-shadow boundary.
        const glm::vec3 shadow_diagnostic_camera_target(10.50f, 4.25f, -0.31f);
        const glm::vec3 shadow_diagnostic_camera_position(-9.50f, 1.80f, -0.31f);
        const glm::vec3 shadow_center(-0.48f, 4.50f, -0.31f);
        const glm::mat4 view = glm::lookAt(camera_position, camera_target, glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 projection = glm::perspectiveRH_ZO(glm::radians(52.0f), aspect, 0.1f, 100.0f);
        const glm::mat4 shadow_diagnostic_view = glm::lookAt(
            shadow_diagnostic_camera_position, shadow_diagnostic_camera_target, glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 shadow_diagnostic_projection = glm::perspectiveRH_ZO(glm::radians(52.0f), aspect, 0.1f, 100.0f);
        const glm::vec3 initial_light_direction = glm::normalize(glm::vec3(-0.45f, 0.78f, -0.43f));
        const float orbit_sine = std::sin(m_sun_orbit_phase_radians);
        const float orbit_cosine = std::cos(m_sun_orbit_phase_radians);
        const glm::vec3 light_direction(
            initial_light_direction.x * orbit_cosine - initial_light_direction.z * orbit_sine,
            initial_light_direction.y,
            initial_light_direction.x * orbit_sine + initial_light_direction.z * orbit_cosine);
        const glm::vec3 light_position = shadow_center + light_direction * 32.0f;
        const glm::mat4 light_view = glm::lookAt(light_position, shadow_center, glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 light_projection = glm::orthoRH_ZO(-20.0f, 20.0f, -20.0f, 20.0f, 0.1f, 100.0f);

        std::vector<SponzaSceneUniforms> scene_uniforms;
        std::vector<SponzaSceneUniforms> shadow_diagnostic_uniforms;
        std::vector<SponzaShadowUniforms> shadow_uniforms;
        scene_uniforms.reserve(m_sponza_scene.draw_count);
        shadow_diagnostic_uniforms.reserve(m_sponza_scene.draw_count);
        shadow_uniforms.reserve(m_sponza_scene.draw_count);
        const auto append_scene_uniforms = [&](std::vector<SponzaSceneUniforms>& uniforms, const glm::mat4& camera_view,
                                               const glm::mat4& camera_projection,
                                               const glm::vec3& frame_camera_position)
        {
            for (uint32_t draw_index = 0; draw_index < m_sponza_scene.draw_count; ++draw_index)
            {
                const KeraGltfLoadedModel& draw = m_sponza_scene.draw_items[draw_index];
                const glm::mat4 model = glm::make_mat4(draw.transform);
                uniforms.push_back({
                    .model = model,
                    .normal_matrix = glm::transpose(glm::inverse(model)),
                    .view = camera_view,
                    .projection = camera_projection,
                    .light_view = light_view,
                    .light_projection = light_projection,
                    .camera_position = glm::vec4(frame_camera_position, 1.0f),
                    .light_direction_shadow_bias = glm::vec4(light_direction, 0.0015f),
                    .base_color_factor =
                        glm::vec4(draw.material_factors.base_color[0], draw.material_factors.base_color[1],
                                  draw.material_factors.base_color[2], draw.material_factors.base_color[3]),
                    .emissive_factor_normal_scale =
                        glm::vec4(draw.material_factors.emissive[0], draw.material_factors.emissive[1],
                                  draw.material_factors.emissive[2], draw.material_factors.normal_scale),
                    .metallic_roughness_occlusion =
                        glm::vec4(draw.material_factors.metallic, draw.material_factors.roughness,
                                  draw.material_factors.occlusion_strength, 0.0f),
                    .alpha_mode_cutoff_double_sided = glm::vec4(
                        toShaderAlphaMode(draw.material_factors.alpha_mode), draw.material_factors.alpha_cutoff,
                        draw.material_factors.double_sided != 0 ? 1.0f : 0.0f, 0.0f),
                });
            }
        };
        append_scene_uniforms(scene_uniforms, view, projection, camera_position);
        append_scene_uniforms(shadow_diagnostic_uniforms, shadow_diagnostic_view, shadow_diagnostic_projection,
                              shadow_diagnostic_camera_position);

        for (uint32_t draw_index = 0; draw_index < m_sponza_scene.draw_count; ++draw_index)
        {
            const KeraGltfLoadedModel& draw = m_sponza_scene.draw_items[draw_index];
            shadow_uniforms.push_back({
                .model = glm::make_mat4(draw.transform),
                .light_view = light_view,
                .light_projection = light_projection,
                .alpha_mode_cutoff_base_alpha =
                    glm::vec4(toShaderAlphaMode(draw.material_factors.alpha_mode), draw.material_factors.alpha_cutoff,
                              draw.material_factors.base_color[3], 0.0f),
            });
        }

        for (uint32_t draw_index = 0; draw_index < m_sponza_scene.draw_count; ++draw_index)
        {
            if (!m_renderer.uploadUniformRingBuffer(m_sponza_scene_uniform_buffers[draw_index], frame,
                                                    &scene_uniforms[draw_index], sizeof(SponzaSceneUniforms)) ||
                !m_renderer.uploadUniformRingBuffer(m_sponza_shadow_diagnostic_uniform_buffers[draw_index], frame,
                                                    &shadow_diagnostic_uniforms[draw_index],
                                                    sizeof(SponzaSceneUniforms)) ||
                !m_renderer.uploadUniformRingBuffer(m_sponza_shadow_uniform_buffers[draw_index], frame,
                                                    &shadow_uniforms[draw_index], sizeof(SponzaShadowUniforms)))
            {
                return false;
            }
        }
        return true;
    }

    void MultiPassRenderingSample::drawSponzaScene(FrameHandle frame, bool shadow_pass, SponzaSceneRenderMode mode)
    {
        const bool shadow_diagnostic = mode == SponzaSceneRenderMode::SHADOW_COMPARISON_ACTIVE ||
                                       mode == SponzaSceneRenderMode::SHADOW_COMPARISON_REFERENCE;
        const std::vector<BufferHandle>& uniform_buffers =
            shadow_pass
                ? m_sponza_shadow_uniform_buffers
                : (shadow_diagnostic ? m_sponza_shadow_diagnostic_uniform_buffers : m_sponza_scene_uniform_buffers);
        if (uniform_buffers.empty())
        {
            sampleLogError("Attachment Playground Sponza has no per-draw uniform buffers.");
            m_initialized = false;
            return;
        }
        const uint32_t slot = m_renderer.getUniformRingBufferSlot(uniform_buffers.front(), frame);
        const uint32_t slot_count = m_renderer.getUniformRingBufferInfo(uniform_buffers.front()).slot_count;
        if (slot >= slot_count)
        {
            sampleLogError("Attachment Playground Sponza selected an invalid uniform-ring slot.");
            m_initialized = false;
            return;
        }

        for (uint32_t draw_index = 0; draw_index < m_sponza_scene.draw_count; ++draw_index)
        {
            const KeraGltfLoadedModel& draw = m_sponza_scene.draw_items[draw_index];
            const bool double_sided = draw.material_factors.double_sided != 0;
            const GraphicsPipelineHandle pipeline =
                shadow_pass ? (double_sided ? m_sponza_shadow_double_sided_pipeline : m_sponza_shadow_pipeline)
                : mode == SponzaSceneRenderMode::MSAA_REFERENCE
                    ? (double_sided ? m_attachment_pipelines.msaa_reference_sponza_scene_double_sided
                                    : m_attachment_pipelines.msaa_reference_sponza_scene)
                    : (double_sided ? m_attachment_pipelines.sponza_scene_double_sided
                                    : m_attachment_pipelines.sponza_scene);
            const std::vector<DescriptorSetHandle>& descriptor_sets =
                shadow_pass ? (double_sided ? m_attachment_resources.sponza_shadow_double_sided_descriptor_sets
                                            : m_attachment_resources.sponza_shadow_descriptor_sets)
                : mode == SponzaSceneRenderMode::MSAA_REFERENCE
                    ? (double_sided ? m_attachment_resources.msaa_reference_sponza_scene_double_sided_descriptor_sets
                                    : m_attachment_resources.msaa_reference_sponza_scene_descriptor_sets)
                : mode == SponzaSceneRenderMode::SHADOW_COMPARISON_ACTIVE
                    ? (double_sided
                           ? m_attachment_resources.shadow_comparison_active_sponza_scene_double_sided_descriptor_sets
                           : m_attachment_resources.shadow_comparison_active_sponza_scene_descriptor_sets)
                : mode == SponzaSceneRenderMode::SHADOW_COMPARISON_REFERENCE
                    ? (double_sided ? m_attachment_resources.shadow_comparison_sponza_scene_double_sided_descriptor_sets
                                    : m_attachment_resources.shadow_comparison_sponza_scene_descriptor_sets)
                    : (double_sided ? m_attachment_resources.sponza_scene_double_sided_descriptor_sets
                                    : m_attachment_resources.sponza_scene_descriptor_sets);
            const size_t descriptor_index = static_cast<size_t>(draw_index) * slot_count + slot;
            if (descriptor_index >= descriptor_sets.size())
            {
                sampleLogError("Attachment Playground Sponza descriptor-set layout is incomplete.");
                m_initialized = false;
                return;
            }

            m_renderer.bindPipeline(frame, pipeline);
            m_renderer.bindVertexBuffer(frame, 0, draw.vertex_buffer);
            m_renderer.bindIndexBuffer(frame, draw.index_buffer, static_cast<EIndexFormat>(draw.index_format));
            m_renderer.bindDescriptorSet(frame, pipeline, descriptor_sets[descriptor_index]);
            m_renderer.drawIndexed(frame, draw.index_count);
        }
    }

    bool MultiPassRenderingSample::verifyCapture()
    {
        test::AttachmentCapture capture{};
        if (!test::takeAttachmentCapture(m_renderer.native(), kCaptureName, capture))
        {
            sampleLogError("Failed to retire the Multi-Pass offscreen capture.");
            return false;
        }
        const size_t expected_size = static_cast<size_t>(capture.width) * static_cast<size_t>(capture.height) * 4u;
        if (capture.width != m_render_extent.width || capture.height != m_render_extent.height ||
            capture.format != KERA_TEXTURE_FORMAT_RGBA8 || capture.bytes.size() != expected_size ||
            capture.bytes.empty())
        {
            sampleLogError("Multi-Pass offscreen capture returned unexpected dimensions, format, or byte count.");
            return false;
        }
        const size_t row_stride = static_cast<size_t>(capture.width) * 4u;
        const std::array<size_t, 4> corner_offsets = {
            0u,
            (static_cast<size_t>(capture.width) - 1u) * 4u,
            (static_cast<size_t>(capture.height) - 1u) * row_stride,
            (static_cast<size_t>(capture.height) - 1u) * row_stride + (static_cast<size_t>(capture.width) - 1u) * 4u,
        };
        std::array<uint32_t, 3> background_sum = {};
        for (const size_t corner_offset : corner_offsets)
        {
            for (size_t channel = 0; channel < background_sum.size(); ++channel)
            {
                background_sum[channel] += capture.bytes[corner_offset + channel];
            }
        }
        std::array<uint8_t, 3> background_rgb = {};
        for (size_t channel = 0; channel < background_rgb.size(); ++channel)
        {
            background_rgb[channel] = static_cast<uint8_t>(background_sum[channel] / corner_offsets.size());
        }

        size_t scene_pixel_count = 0;
        size_t shadowed_scene_pixel_count = 0;
        for (size_t offset = 0; offset < capture.bytes.size(); offset += 4u)
        {
            const uint32_t colour_distance =
                static_cast<uint32_t>(
                    std::abs(static_cast<int>(capture.bytes[offset + 0u]) - static_cast<int>(background_rgb[0]))) +
                static_cast<uint32_t>(
                    std::abs(static_cast<int>(capture.bytes[offset + 1u]) - static_cast<int>(background_rgb[1]))) +
                static_cast<uint32_t>(
                    std::abs(static_cast<int>(capture.bytes[offset + 2u]) - static_cast<int>(background_rgb[2])));
            if (colour_distance <= 18u)
            {
                continue;
            }
            ++scene_pixel_count;
            const uint32_t luma =
                54u * capture.bytes[offset + 0u] + 182u * capture.bytes[offset + 1u] + 18u * capture.bytes[offset + 2u];
            // Exclude partially covered edge pixels blended with the dark clear colour.
            shadowed_scene_pixel_count += colour_distance > 96u && luma < 51u * 255u ? 1u : 0u;
        }
        const size_t pixel_count = capture.bytes.size() / 4u;
        sampleLogInfo("Multi-Pass capture metrics: scene_pixels=" + std::to_string(scene_pixel_count) +
                      " shadowed_scene_pixels=" + std::to_string(shadowed_scene_pixel_count) +
                      " total_pixels=" + std::to_string(pixel_count));
        const size_t min_scene_coverage_denominator = m_uses_sponza ? 7u : 5u;
        if (scene_pixel_count * min_scene_coverage_denominator < pixel_count)
        {
            sampleLogError("Multi-Pass offscreen capture did not contain sufficient scene coverage.");
            return false;
        }
        if (!m_uses_sponza && m_active_config.shadows_enabled && shadowed_scene_pixel_count * 200u < pixel_count)
        {
            sampleLogError("Multi-Pass offscreen capture did not contain sufficient shadow coverage.");
            return false;
        }
        // The procedural fallback has no intentionally dark albedo, while Sponza does.
        // Allow a small antialiasing fringe without accepting visible fallback shadow coverage.
        if (!m_uses_sponza && !m_active_config.shadows_enabled && shadowed_scene_pixel_count * 1000u > pixel_count)
        {
            sampleLogError("Multi-Pass shadows-disabled capture contained excessive shadowed scene pixels.");
            return false;
        }
        const std::string capture_suffix = m_capture_after_resize_smoke ? " after final resize" : "";
        sampleLogInfo("Multi-Pass offscreen capture verified" + capture_suffix + ": " + std::to_string(capture.width) +
                      "x" + std::to_string(capture.height) + " RGBA8 " + std::to_string(capture.bytes.size()) +
                      " bytes.");
        if (!m_active_config.shadows_enabled)
        {
            sampleLogInfo(m_uses_sponza ? "Multi-Pass Sponza shadows-disabled scene capture verified."
                                        : "Multi-Pass shadows-disabled capture verified.");
        }
        else if (m_uses_sponza)
        {
            sampleLogInfo("Multi-Pass Sponza scene capture verified.");
        }
        else
        {
            sampleLogInfo("Multi-Pass shadow coverage verified.");
        }
        return true;
    }

    void MultiPassRenderingSample::render(RenderContext& context)
    {
        if (!m_initialized)
        {
            return;
        }
        if (m_capture_requested && !m_capture_verified)
        {
            m_capture_verified = verifyCapture();
            if (!m_capture_verified)
            {
                m_initialized = false;
                return;
            }
        }

        if (!uploadSponzaUniforms(context.frame()))
        {
            sampleLogError("Failed to upload Multi-Pass Sponza per-draw uniforms.");
            m_initialized = false;
            return;
        }

        KeraAttachmentError error{};
        const KeraDepthAttachmentDesc shadow_depth_attachment{
            .texture = m_attachment_resources.shadow,
            .load_op = KERA_ATTACHMENT_LOAD_OP_CLEAR,
            .store_op = KERA_ATTACHMENT_STORE_OP_STORE,
            .clear_depth = 1.0f,
        };
        const KeraAttachmentRenderingDesc shadow_rendering_desc{
            .struct_size = sizeof(KeraAttachmentRenderingDesc),
            .color_attachments = nullptr,
            .color_attachment_count = 0,
            .depth_attachment = &shadow_depth_attachment,
        };
        {
            if (!m_renderer.beginAttachmentRendering(context.frame(), shadow_rendering_desc, &error))
            {
                sampleLogError("Failed to begin Multi-Pass shadow rendering: " + attachmentErrorText(error));
                m_initialized = false;
                return;
            }
            if (m_active_config.shadows_enabled)
            {
                if (m_uses_sponza)
                {
                    drawSponzaScene(context.frame(), true);
                }
                else
                {
                    m_renderer.bindPipeline(context.frame(), m_shadow_pipeline);
                    m_renderer.bindVertexBuffer(context.frame(), 0, m_scene_vertex_buffer);
                    m_renderer.bindIndexBuffer(context.frame(), m_scene_index_buffer, EIndexFormat::U_INT16);
                    m_renderer.drawIndexed(context.frame(), m_scene_index_count);
                }
            }
            if (!m_renderer.endAttachmentRendering(context.frame(), &error))
            {
                sampleLogError("Failed to end Multi-Pass shadow rendering: " + attachmentErrorText(error));
                m_initialized = false;
                return;
            }
        }

        if (m_active_config.shadows_enabled && m_attachment_resources.shadow_comparison_reference.isValid())
        {
            const KeraDepthAttachmentDesc shadow_reference_depth_attachment{
                .texture = m_attachment_resources.shadow_comparison_reference,
                .load_op = KERA_ATTACHMENT_LOAD_OP_CLEAR,
                .store_op = KERA_ATTACHMENT_STORE_OP_STORE,
                .clear_depth = 1.0f,
            };
            const KeraAttachmentRenderingDesc shadow_reference_rendering_desc{
                .struct_size = sizeof(KeraAttachmentRenderingDesc),
                .color_attachments = nullptr,
                .color_attachment_count = 0,
                .depth_attachment = &shadow_reference_depth_attachment,
            };
            {
                if (!m_renderer.beginAttachmentRendering(context.frame(), shadow_reference_rendering_desc, &error))
                {
                    sampleLogError("Failed to begin Multi-Pass shadow comparison reference rendering: " +
                                   attachmentErrorText(error));
                    m_initialized = false;
                    return;
                }
                if (m_uses_sponza)
                {
                    drawSponzaScene(context.frame(), true);
                }
                else
                {
                    m_renderer.bindPipeline(context.frame(), m_shadow_pipeline);
                    m_renderer.bindVertexBuffer(context.frame(), 0, m_scene_vertex_buffer);
                    m_renderer.bindIndexBuffer(context.frame(), m_scene_index_buffer, EIndexFormat::U_INT16);
                    m_renderer.drawIndexed(context.frame(), m_scene_index_count);
                }
                if (!m_renderer.endAttachmentRendering(context.frame(), &error))
                {
                    sampleLogError("Failed to end Multi-Pass shadow comparison reference rendering: " +
                                   attachmentErrorText(error));
                    m_initialized = false;
                    return;
                }
            }
        }

        const TextureHandle scene_color_texture = m_scene_sample_count == KERA_ATTACHMENT_SAMPLE_COUNT_1
                                                      ? m_attachment_resources.scene
                                                      : m_attachment_resources.scene_msaa;
        const KeraColorAttachmentDesc color_attachment{
            .texture = scene_color_texture,
            .load_op = KERA_ATTACHMENT_LOAD_OP_CLEAR,
            .store_op = KERA_ATTACHMENT_STORE_OP_STORE,
            .clear_color = {0.025f, 0.035f, 0.085f, 1.0f},
        };
        const KeraDepthAttachmentDesc depth_attachment{
            .texture = m_attachment_resources.scene_depth,
            .load_op = KERA_ATTACHMENT_LOAD_OP_CLEAR,
            .store_op = KERA_ATTACHMENT_STORE_OP_DONT_CARE,
            .clear_depth = 1.0f,
        };
        const KeraAttachmentRenderingDesc rendering_desc{
            .struct_size = sizeof(KeraAttachmentRenderingDesc),
            .color_attachments = &color_attachment,
            .color_attachment_count = 1,
            .depth_attachment = &depth_attachment,
        };
        {
            if (!m_renderer.beginAttachmentRendering(context.frame(), rendering_desc, &error))
            {
                sampleLogError("Failed to begin Multi-Pass attachment rendering: " + attachmentErrorText(error));
                m_initialized = false;
                return;
            }

            if (m_uses_sponza)
            {
                drawSponzaScene(context.frame(), false);
            }
            else
            {
                m_renderer.bindPipeline(context.frame(), m_attachment_pipelines.scene);
                m_renderer.bindDescriptorSet(context.frame(), m_attachment_pipelines.scene,
                                             m_attachment_resources.scene_descriptor_set);
                m_renderer.bindVertexBuffer(context.frame(), 0, m_scene_vertex_buffer);
                m_renderer.bindIndexBuffer(context.frame(), m_scene_index_buffer, EIndexFormat::U_INT16);
                m_renderer.drawIndexed(context.frame(), m_scene_index_count);
            }

            if (!m_renderer.endAttachmentRendering(context.frame(), &error))
            {
                sampleLogError("Failed to end Multi-Pass attachment rendering: " + attachmentErrorText(error));
                m_initialized = false;
                return;
            }
        }

        if (m_scene_sample_count != KERA_ATTACHMENT_SAMPLE_COUNT_1)
        {
            if (!m_renderer.resolveAttachmentTexture(context.frame(), m_attachment_resources.scene_msaa,
                                                     m_attachment_resources.scene, &error))
            {
                sampleLogError("Failed to resolve Multi-Pass 4x MSAA scene attachment: " + attachmentErrorText(error));
                m_initialized = false;
                return;
            }
        }

        const auto render_diagnostic_scene =
            [this, &context, &error](TextureHandle color_texture, TextureHandle depth_texture,
                                     SponzaSceneRenderMode mode, AttachmentPassTimingScope timing_scope,
                                     const char* name)
        {
            const KeraColorAttachmentDesc diagnostic_color_attachment{
                .texture = color_texture,
                .load_op = KERA_ATTACHMENT_LOAD_OP_CLEAR,
                .store_op = KERA_ATTACHMENT_STORE_OP_STORE,
                .clear_color = {0.025f, 0.035f, 0.085f, 1.0f},
            };
            const KeraDepthAttachmentDesc diagnostic_depth_attachment{
                .texture = depth_texture,
                .load_op = KERA_ATTACHMENT_LOAD_OP_CLEAR,
                .store_op = KERA_ATTACHMENT_STORE_OP_DONT_CARE,
                .clear_depth = 1.0f,
            };
            const KeraAttachmentRenderingDesc diagnostic_rendering_desc{
                .struct_size = sizeof(KeraAttachmentRenderingDesc),
                .color_attachments = &diagnostic_color_attachment,
                .color_attachment_count = 1,
                .depth_attachment = &diagnostic_depth_attachment,
            };
            if (!m_renderer.beginAttachmentRendering(context.frame(), diagnostic_rendering_desc, &error))
            {
                sampleLogError(std::string("Failed to begin Multi-Pass ") + name +
                               " rendering: " + attachmentErrorText(error));
                return false;
            }
            if (m_uses_sponza)
            {
                drawSponzaScene(context.frame(), false, mode);
            }
            else
            {
                const bool msaa_reference = mode == SponzaSceneRenderMode::MSAA_REFERENCE;
                const bool shadow_reference = mode == SponzaSceneRenderMode::SHADOW_COMPARISON_REFERENCE;
                const GraphicsPipelineHandle pipeline =
                    msaa_reference ? m_attachment_pipelines.msaa_reference_scene : m_attachment_pipelines.scene;
                const DescriptorSetHandle descriptor_set =
                    shadow_reference ? m_attachment_resources.shadow_comparison_scene_descriptor_set
                    : msaa_reference ? m_attachment_resources.msaa_reference_scene_descriptor_set
                                     : m_attachment_resources.scene_descriptor_set;
                m_renderer.bindPipeline(context.frame(), pipeline);
                m_renderer.bindDescriptorSet(context.frame(), pipeline, descriptor_set);
                m_renderer.bindVertexBuffer(context.frame(), 0, m_scene_vertex_buffer);
                m_renderer.bindIndexBuffer(context.frame(), m_scene_index_buffer, EIndexFormat::U_INT16);
                m_renderer.drawIndexed(context.frame(), m_scene_index_count);
            }
            if (!m_renderer.endAttachmentRendering(context.frame(), &error))
            {
                sampleLogError(std::string("Failed to end Multi-Pass ") + name +
                               " rendering: " + attachmentErrorText(error));
                return false;
            }
            return true;
        };

        const bool render_shadow_comparison = m_active_config.shadows_enabled &&
                                              m_active_config.preview_shadow_comparison &&
                                              m_attachment_resources.shadow_comparison_reference.isValid() &&
                                              m_attachment_resources.shadow_comparison_active_scene.isValid();
        if (render_shadow_comparison)
        {
            const bool diagnostic_is_multisampled =
                m_attachment_resources.shadow_comparison_active_scene_msaa.isValid();

            const TextureHandle active_color_texture = diagnostic_is_multisampled
                                                           ? m_attachment_resources.shadow_comparison_active_scene_msaa
                                                           : m_attachment_resources.shadow_comparison_active_scene;
            if (!render_diagnostic_scene(active_color_texture,
                                         m_attachment_resources.shadow_comparison_active_scene_depth,
                                         SponzaSceneRenderMode::SHADOW_COMPARISON_ACTIVE,
                                         AttachmentPassTimingScope::SHADOW_LENS_ACTIVE, "shadow comparison active"))
            {
                m_initialized = false;
                return;
            }
            if (diagnostic_is_multisampled)
            {
                if (!m_renderer.resolveAttachmentTexture(context.frame(),
                                                         m_attachment_resources.shadow_comparison_active_scene_msaa,
                                                         m_attachment_resources.shadow_comparison_active_scene, &error))
                {
                    sampleLogError("Failed to resolve Multi-Pass shadow comparison active view: " +
                                   attachmentErrorText(error));
                    m_initialized = false;
                    return;
                }
            }

            const TextureHandle reference_color_texture = diagnostic_is_multisampled
                                                              ? m_attachment_resources.shadow_comparison_scene_msaa
                                                              : m_attachment_resources.msaa_reference_scene;
            const TextureHandle reference_depth_texture = diagnostic_is_multisampled
                                                              ? m_attachment_resources.shadow_comparison_scene_depth
                                                              : m_attachment_resources.msaa_reference_scene_depth;
            if (!render_diagnostic_scene(reference_color_texture, reference_depth_texture,
                                         SponzaSceneRenderMode::SHADOW_COMPARISON_REFERENCE,
                                         AttachmentPassTimingScope::SHADOW_LENS_REFERENCE,
                                         "shadow comparison reference"))
            {
                m_initialized = false;
                return;
            }
            if (diagnostic_is_multisampled)
            {
                if (!m_renderer.resolveAttachmentTexture(context.frame(),
                                                         m_attachment_resources.shadow_comparison_scene_msaa,
                                                         m_attachment_resources.msaa_reference_scene, &error))
                {
                    sampleLogError("Failed to resolve Multi-Pass shadow comparison reference: " +
                                   attachmentErrorText(error));
                    m_initialized = false;
                    return;
                }
            }
        }
        else if (m_attachment_resources.msaa_reference_scene.isValid())
        {
            if (!render_diagnostic_scene(
                    m_attachment_resources.msaa_reference_scene, m_attachment_resources.msaa_reference_scene_depth,
                    SponzaSceneRenderMode::MSAA_REFERENCE, AttachmentPassTimingScope::MSAA_REFERENCE, "MSAA reference"))
            {
                m_initialized = false;
                return;
            }
        }

        {
            context.renderToBackbuffer(
                getClearColor(),
                [this](FrameHandle frame)
                {
                    const bool show_shadow_map = m_active_config.shadows_enabled && m_active_config.preview_shadow_map;
                    const bool show_shadow_inset =
                        m_active_config.shadows_enabled && m_active_config.preview_shadow_inset;
                    const bool show_msaa_comparison = m_active_config.preview_msaa_comparison &&
                                                      m_attachment_resources.msaa_reference_scene.isValid();
                    const bool show_shadow_comparison =
                        m_active_config.shadows_enabled && m_active_config.preview_shadow_comparison &&
                        m_attachment_resources.shadow_comparison_reference.isValid() &&
                        m_attachment_resources.shadow_comparison_active_scene.isValid() &&
                        m_attachment_resources.msaa_reference_scene.isValid();
                    const GraphicsPipelineHandle pipeline = show_shadow_map          ? m_shadow_preview_pipeline
                                                            : show_shadow_inset      ? m_shadow_inset_pipeline
                                                            : show_shadow_comparison ? m_shadow_comparison_pipeline
                                                            : show_msaa_comparison   ? m_msaa_comparison_pipeline
                                                                                     : m_composite_pipeline;
                    const DescriptorSetHandle descriptor_set =
                        show_shadow_map          ? m_attachment_resources.shadow_preview_descriptor_set
                        : show_shadow_inset      ? m_attachment_resources.shadow_inset_descriptor_set
                        : show_shadow_comparison ? m_attachment_resources.shadow_comparison_descriptor_set
                        : show_msaa_comparison   ? m_attachment_resources.msaa_comparison_descriptor_set
                                                 : m_attachment_resources.composite_descriptor_set;
                    m_renderer.bindPipeline(frame, pipeline);
                    m_renderer.bindVertexBuffer(frame, 0, m_fullscreen_vertex_buffer);
                    m_renderer.bindIndexBuffer(frame, m_fullscreen_index_buffer, EIndexFormat::U_INT16);
                    m_renderer.bindDescriptorSet(frame, pipeline, descriptor_set);
                    m_renderer.drawIndexed(frame, m_fullscreen_index_count);
                });
        }

        if (m_capture_after_resize_frame_pending)
        {
            // Render one complete frame at the final extent before scheduling the readback.
            m_capture_after_resize_frame_pending = false;
        }
        else if (m_capture_smoke && !m_capture_requested &&
                 (!m_capture_after_resize_smoke || m_non_zero_resize_count >= 3) &&
                 (!m_reconfigure_smoke || m_reconfigure_smoke_stage >= 6))
        {
            if (!test::requestAttachmentCapture(m_renderer.native(), context.frame(), m_attachment_resources.scene,
                                                kCaptureName))
            {
                sampleLogError("Failed to request the Multi-Pass offscreen capture.");
                m_initialized = false;
                return;
            }
            m_capture_requested = true;
        }
    }

    void MultiPassRenderingSample::destroySponzaResources()
    {
        for (BufferHandle uniform_buffer : m_sponza_shadow_uniform_buffers)
        {
            if (uniform_buffer.isValid())
            {
                m_renderer.destroyBuffer(uniform_buffer);
            }
        }
        m_sponza_shadow_uniform_buffers.clear();
        for (BufferHandle uniform_buffer : m_sponza_shadow_diagnostic_uniform_buffers)
        {
            if (uniform_buffer.isValid())
            {
                m_renderer.destroyBuffer(uniform_buffer);
            }
        }
        m_sponza_shadow_diagnostic_uniform_buffers.clear();
        for (BufferHandle uniform_buffer : m_sponza_scene_uniform_buffers)
        {
            if (uniform_buffer.isValid())
            {
                m_renderer.destroyBuffer(uniform_buffer);
            }
        }
        m_sponza_scene_uniform_buffers.clear();
        if (m_sponza_scene.draw_items)
        {
            m_renderer.destroyGltfScene(m_sponza_scene);
        }
        m_sponza_scene = {};
        m_uses_sponza = false;
    }

    void MultiPassRenderingSample::cleanup()
    {
        m_initialized = false;
        destroyAttachmentResources(m_attachment_resources);
        destroyAttachmentPipelines(m_attachment_pipelines);
        destroySponzaResources();
        if (m_shadow_inset_sampler.isValid())
        {
            m_renderer.destroySampler(m_shadow_inset_sampler);
            m_shadow_inset_sampler = {};
        }
        if (m_scene_sampler.isValid())
        {
            m_renderer.destroySampler(m_scene_sampler);
            m_scene_sampler = {};
        }
        if (m_sponza_shadow_double_sided_pipeline.isValid())
        {
            m_renderer.destroyGraphicsPipeline(m_sponza_shadow_double_sided_pipeline);
            m_sponza_shadow_double_sided_pipeline = {};
        }
        if (m_sponza_shadow_pipeline.isValid())
        {
            m_renderer.destroyGraphicsPipeline(m_sponza_shadow_pipeline);
            m_sponza_shadow_pipeline = {};
        }
        if (m_shadow_comparison_pipeline.isValid())
        {
            m_renderer.destroyGraphicsPipeline(m_shadow_comparison_pipeline);
            m_shadow_comparison_pipeline = {};
        }
        if (m_msaa_comparison_pipeline.isValid())
        {
            m_renderer.destroyGraphicsPipeline(m_msaa_comparison_pipeline);
            m_msaa_comparison_pipeline = {};
        }
        if (m_shadow_inset_pipeline.isValid())
        {
            m_renderer.destroyGraphicsPipeline(m_shadow_inset_pipeline);
            m_shadow_inset_pipeline = {};
        }
        if (m_shadow_preview_pipeline.isValid())
        {
            m_renderer.destroyGraphicsPipeline(m_shadow_preview_pipeline);
            m_shadow_preview_pipeline = {};
        }
        if (m_composite_pipeline.isValid())
        {
            m_renderer.destroyGraphicsPipeline(m_composite_pipeline);
            m_composite_pipeline = {};
        }
        if (m_shadow_pipeline.isValid())
        {
            m_renderer.destroyGraphicsPipeline(m_shadow_pipeline);
            m_shadow_pipeline = {};
        }
        if (m_fullscreen_index_buffer.isValid())
        {
            m_renderer.destroyBuffer(m_fullscreen_index_buffer);
            m_fullscreen_index_buffer = {};
        }
        if (m_fullscreen_vertex_buffer.isValid())
        {
            m_renderer.destroyBuffer(m_fullscreen_vertex_buffer);
            m_fullscreen_vertex_buffer = {};
        }
        if (m_scene_index_buffer.isValid())
        {
            m_renderer.destroyBuffer(m_scene_index_buffer);
            m_scene_index_buffer = {};
        }
        if (m_scene_vertex_buffer.isValid())
        {
            m_renderer.destroyBuffer(m_scene_vertex_buffer);
            m_scene_vertex_buffer = {};
        }
        if (m_sponza_shadow_shader_program.isValid())
        {
            m_renderer.destroyShaderProgram(m_sponza_shadow_shader_program);
            m_sponza_shadow_shader_program = {};
        }
        if (m_sponza_scene_shader_program.isValid())
        {
            m_renderer.destroyShaderProgram(m_sponza_scene_shader_program);
            m_sponza_scene_shader_program = {};
        }
        if (m_shadow_comparison_shader_program.isValid())
        {
            m_renderer.destroyShaderProgram(m_shadow_comparison_shader_program);
            m_shadow_comparison_shader_program = {};
        }
        if (m_msaa_comparison_shader_program.isValid())
        {
            m_renderer.destroyShaderProgram(m_msaa_comparison_shader_program);
            m_msaa_comparison_shader_program = {};
        }
        if (m_shadow_inset_shader_program.isValid())
        {
            m_renderer.destroyShaderProgram(m_shadow_inset_shader_program);
            m_shadow_inset_shader_program = {};
        }
        if (m_shadow_preview_shader_program.isValid())
        {
            m_renderer.destroyShaderProgram(m_shadow_preview_shader_program);
            m_shadow_preview_shader_program = {};
        }
        if (m_composite_shader_program.isValid())
        {
            m_renderer.destroyShaderProgram(m_composite_shader_program);
            m_composite_shader_program = {};
        }
        if (m_shadow_shader_program.isValid())
        {
            m_renderer.destroyShaderProgram(m_shadow_shader_program);
            m_shadow_shader_program = {};
        }
        if (m_scene_shader_program.isValid())
        {
            m_renderer.destroyShaderProgram(m_scene_shader_program);
            m_scene_shader_program = {};
        }
    }
}  // namespace kera
