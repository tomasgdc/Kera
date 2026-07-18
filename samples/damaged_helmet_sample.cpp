// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "damaged_helmet_sample.h"

#include "render_context.h"
#include "sample_utils.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstddef>
#include <string>

namespace kera
{
    namespace
    {
        struct HelmetUniforms
        {
            glm::mat4 model;
            glm::mat4 view;
            glm::mat4 projection;
            glm::vec4 camera_position;
            glm::vec4 light_direction_ambient;
            glm::vec4 base_color_factor;
            glm::vec4 emissive_factor_normal_scale;
            glm::vec4 metallic_roughness_occlusion;
            glm::vec4 alpha_mode_cutoff_reflection_exposure;
            glm::vec4 debug_view_gamma;
            glm::vec4 padding2;
        };

        namespace damaged_helmet_shader
        {
            constexpr const char* kPath = "shaders/damaged_helmet.slang";
            constexpr const char* kMeshVertexEntryPoint = "helmetVertexMain";
            constexpr const char* kMeshFragmentEntryPoint = "helmetFragmentMain";
            constexpr const char* kFullscreenVertexEntryPoint = "fullscreenVertexMain";
            constexpr const char* kFullscreenFragmentEntryPoint = "fullscreenFragmentMain";
            constexpr const char* kHelmetParams = "helmetParams";
            constexpr const char* kBaseColorTexture = "baseColorTexture";
            constexpr const char* kMetalRoughnessTexture = "metalRoughnessTexture";
            constexpr const char* kEmissiveTexture = "emissiveTexture";
            constexpr const char* kOcclusionTexture = "occlusionTexture";
            constexpr const char* kNormalTexture = "normalTexture";
            constexpr const char* kSceneTexture = "sceneTexture";
            constexpr const char* kMaterialSampler = "materialSampler";
        }  // namespace damaged_helmet_shader

        constexpr uint32_t kUniformRingSlots = 3;
        constexpr uint32_t kMaxDamagedHelmetDebugView = 11;

        float toShaderAlphaMode(KeraGltfAlphaMode mode)
        {
            switch (mode)
            {
                case KERA_GLTF_ALPHA_MASK:
                    return 1.0f;
                case KERA_GLTF_ALPHA_BLEND:
                    return 2.0f;
                case KERA_GLTF_ALPHA_OPAQUE:
                default:
                    return 0.0f;
            }
        }
    }  // namespace

    DamagedHelmetSample::DamagedHelmetSample(Renderer& renderer)
        : Sample("Damaged Helmet Texture Loading"), m_renderer(renderer)
    {
    }

    DamagedHelmetSample::DamagedHelmetSample(Renderer& renderer, uint32_t debug_view)
        : Sample("Damaged Helmet Texture Loading")
        , m_renderer(renderer)
        , m_debug_view(debug_view <= kMaxDamagedHelmetDebugView ? debug_view : 0)
    {
    }

    DamagedHelmetSample::DamagedHelmetSample(Renderer& renderer, uint32_t debug_view, bool fixed_yaw, float yaw_radians)
        : Sample("Damaged Helmet Texture Loading")
        , m_renderer(renderer)
        , m_initial_yaw_radians(fixed_yaw ? yaw_radians : 2.2f)
        , m_debug_view(debug_view <= kMaxDamagedHelmetDebugView ? debug_view : 0)
        , m_fixed_yaw(fixed_yaw)
    {
    }

    void DamagedHelmetSample::initialize()
    {
        sampleLogInfo("Initializing " + std::string(getName()));
        m_initialized = false;

        if (!createShaderPrograms())
        {
            sampleLogError("Failed to create DamagedHelmet shader programs.");
            cleanup();
            return;
        }

        if (!loadGltfModelResources())
        {
            sampleLogError("Failed to load DamagedHelmet glTF model resources.");
            cleanup();
            return;
        }

        if (!createFullscreenGeometry())
        {
            sampleLogError("Failed to create DamagedHelmet fullscreen geometry.");
            cleanup();
            return;
        }

        if (!recreateRenderResources(m_renderer.getDrawableExtent()))
        {
            sampleLogError("Failed to create DamagedHelmet render resources.");
            cleanup();
            return;
        }

        m_initialized = true;
        sampleLogInfo("DamagedHelmet sample initialized successfully.");
    }

    bool DamagedHelmetSample::createShaderPrograms()
    {
        const std::string shader_path = resolveShaderPath(damaged_helmet_shader::kPath);
        m_mesh_shader_program = m_renderer.createGraphicsShaderProgram({
            .path = sampleStringView(shader_path),
            .vertex_entry_point = stringView(damaged_helmet_shader::kMeshVertexEntryPoint),
            .fragment_entry_point = stringView(damaged_helmet_shader::kMeshFragmentEntryPoint),
            .source = EShaderSourceKind::SLANG_FILE,
            .debug_name = {},
        });

        if (!m_mesh_shader_program.isValid())
        {
            return false;
        }

        m_display_shader_program = m_renderer.createGraphicsShaderProgram({
            .path = sampleStringView(shader_path),
            .vertex_entry_point = stringView(damaged_helmet_shader::kFullscreenVertexEntryPoint),
            .fragment_entry_point = stringView(damaged_helmet_shader::kFullscreenFragmentEntryPoint),
            .source = EShaderSourceKind::SLANG_FILE,
            .debug_name = {},
        });

        return m_display_shader_program.isValid();
    }

    bool DamagedHelmetSample::loadGltfModelResources()
    {
        const std::string asset_path = resolveSampleAssetPath("assets/gltf/DamagedHelmet/DamagedHelmet.gltf");
        if (!m_renderer.loadGltfModel(
                {
                    .path = sampleStringView(asset_path),
                    .debug_name = stringView("DamagedHelmet"),
                    .require_material_textures = 1,
                },
                m_model))
        {
            sampleLogError("DamagedHelmet glTF load failed.");
            return false;
        }

        m_uniform_buffer = m_renderer.createUniformRingBuffer(sizeof(HelmetUniforms), kUniformRingSlots);
        return m_uniform_buffer.isValid();
    }

    bool DamagedHelmetSample::createFullscreenGeometry()
    {
        const auto& vertices = fullscreenTriangleVertices();
        const auto& indices = fullscreenTriangleIndices();
        m_fullscreen_index_count = static_cast<uint32_t>(indices.size());

        m_fullscreen_vertex_buffer = m_renderer.createBuffer({
            .size = vertices.size() * sizeof(FullscreenTriangleVertex),
            .usage = EBufferUsageKind::VERTEX,
            .memory_access = EMemoryAccess::CPU_WRITE,
            .debug_name = stringView("DamagedHelmet Fullscreen Vertex Buffer"),
        });

        m_fullscreen_index_buffer = m_renderer.createBuffer({
            .size = indices.size() * sizeof(uint16_t),
            .usage = EBufferUsageKind::INDEX,
            .memory_access = EMemoryAccess::CPU_WRITE,
            .debug_name = stringView("DamagedHelmet Fullscreen Index Buffer"),
        });

        return m_fullscreen_vertex_buffer.isValid() && m_fullscreen_index_buffer.isValid() &&
               m_renderer.uploadBuffer(m_fullscreen_vertex_buffer, vertices.data(),
                                       vertices.size() * sizeof(FullscreenTriangleVertex)) &&
               m_renderer.uploadBuffer(m_fullscreen_index_buffer, indices.data(), indices.size() * sizeof(uint16_t));
    }

    bool DamagedHelmetSample::recreateRenderResources(Extent2D extent)
    {
        if (extent.width == 0 || extent.height == 0)
        {
            return false;
        }

        destroyRenderResources();
        m_render_extent = extent;

        m_scene_texture = m_renderer.createTexture({
            .width = extent.width,
            .height = extent.height,
            .format = ETextureFormat::RGBA8,
            .render_target = true,
            .sampled = true,
            .debug_name = stringView("DamagedHelmet Scene Texture"),
        });

        m_scene_depth_texture = m_renderer.createTexture({
            .width = extent.width,
            .height = extent.height,
            .format = ETextureFormat::DEPTH32,
            .render_target = true,
            .sampled = false,
            .depth_stencil = true,
            .debug_name = stringView("DamagedHelmet Depth Texture"),
        });

        if (!m_scene_texture.isValid() || !m_scene_depth_texture.isValid())
        {
            return false;
        }

        m_scene_render_target = m_renderer.createRenderTarget({
            .color_texture = m_scene_texture,
            .depth_texture = m_scene_depth_texture,
            .debug_name = stringView("DamagedHelmet Render Target"),
        });

        return m_scene_render_target.isValid() && createPipelinesAndDescriptors();
    }

    bool DamagedHelmetSample::createPipelinesAndDescriptors()
    {
        const VertexInputLayout mesh_vertex_input =
            VertexInputLayoutBuilder{}
                .vertexBinding<GltfVertex>(0)
                .field(KERA_VERTEX_FIELD(GltfVertex, position, 0, EVertexFormat::FLOAT3))
                .field(KERA_VERTEX_FIELD(GltfVertex, normal, 0, EVertexFormat::FLOAT3))
                .field(KERA_VERTEX_FIELD(GltfVertex, uv, 0, EVertexFormat::FLOAT2))
                .field(KERA_VERTEX_FIELD(GltfVertex, tangent, 0, EVertexFormat::FLOAT4))
                .layout();

        m_mesh_pipeline = m_renderer.createGraphicsPipeline({
            .shader_program = m_mesh_shader_program,
            .vertex_input = mesh_vertex_input,
            .render_target = m_scene_render_target,
            .cull_mode = m_model.material_factors.double_sided ? ECullModeKind::NONE : ECullModeKind::BACK,
            .front_face = EFrontFaceKind::CLOCKWISE,
            .blend_mode = m_model.material_factors.alpha_mode == KERA_GLTF_ALPHA_BLEND ? EBlendModeKind::ALPHA
                                                                                       : EBlendModeKind::OPAQUE,
            .depth_test = true,
            .depth_write = m_model.material_factors.alpha_mode != KERA_GLTF_ALPHA_BLEND,
            .debug_name = stringView("DamagedHelmet Mesh Pipeline"),
        });

        if (!m_mesh_pipeline.isValid())
        {
            return false;
        }

        KeraUniformRingBufferInfo ring_info = m_renderer.getUniformRingBufferInfo(m_uniform_buffer);

        m_mesh_descriptor_sets.clear();
        m_mesh_descriptor_sets.reserve(ring_info.slot_count);

        for (uint32_t slot = 0; slot < ring_info.slot_count; ++slot)
        {
            DescriptorSetHandle descriptor_set = m_renderer.createDescriptorSet(m_mesh_pipeline);
            if (!descriptor_set.isValid())
            {
                return false;
            }

            const std::size_t uniform_offset = m_renderer.getUniformRingBufferSlotOffset(m_uniform_buffer, slot);
            if (!m_renderer.updateDescriptors(descriptor_set)
                     .uniform<HelmetUniforms>(damaged_helmet_shader::kHelmetParams, m_uniform_buffer, uniform_offset)
                     .sampledImage(damaged_helmet_shader::kBaseColorTexture, m_model.material_textures.base_color)
                     .sampledImage(damaged_helmet_shader::kMetalRoughnessTexture, m_model.material_textures.metal_roughness)
                     .sampledImage(damaged_helmet_shader::kEmissiveTexture, m_model.material_textures.emissive)
                     .sampledImage(damaged_helmet_shader::kOcclusionTexture, m_model.material_textures.occlusion)
                     .sampledImage(damaged_helmet_shader::kNormalTexture, m_model.material_textures.normal)
                     .sampler(damaged_helmet_shader::kMaterialSampler, m_model.material_sampler)
                     .ok())
            {
                return false;
            }
            m_mesh_descriptor_sets.push_back(descriptor_set);
        }

        const VertexInputLayout display_vertex_input =
            VertexInputLayoutBuilder{}
                .vertexBinding<FullscreenTriangleVertex>(0)
                .field(KERA_VERTEX_FIELD(FullscreenTriangleVertex, position, 0, EVertexFormat::FLOAT2))
                .field(KERA_VERTEX_FIELD(FullscreenTriangleVertex, uv, 0, EVertexFormat::FLOAT2))
                .layout();

        m_display_pipeline = m_renderer.createGraphicsPipeline({
            .shader_program = m_display_shader_program,
            .vertex_input = display_vertex_input,
            .cull_mode = ECullModeKind::NONE,
            .debug_name = stringView("DamagedHelmet Display Pipeline"),
        });

        if (!m_display_pipeline.isValid())
        {
            return false;
        }

        m_display_descriptor_set = m_renderer.createDescriptorSet(m_display_pipeline);
        return m_display_descriptor_set.isValid() &&
               m_renderer.updateDescriptors(m_display_descriptor_set)
                   .sampledImage(damaged_helmet_shader::kSceneTexture, m_scene_texture)
                   .sampler(damaged_helmet_shader::kMaterialSampler, m_model.material_sampler)
                   .ok();
    }

    void DamagedHelmetSample::update(float delta_time)
    {
        if (m_initialized)
        {
            m_elapsed_time += delta_time;
        }
    }

    void DamagedHelmetSample::resize(Extent2D extent)
    {
        if (extent == m_render_extent || extent.width == 0 || extent.height == 0)
        {
            return;
        }

        if (!recreateRenderResources(extent))
        {
            sampleLogError("Failed to resize DamagedHelmet render resources.");
            m_initialized = false;
        }
    }

    void DamagedHelmetSample::render(RenderContext& context)
    {
        if (!m_initialized)
        {
            return;
        }

        if (!m_mesh_pipeline.isValid() || !m_display_pipeline.isValid() || !m_scene_render_target.isValid() ||
            m_mesh_descriptor_sets.empty() || !m_display_descriptor_set.isValid())
        {
            sampleLogWarning("Render called before DamagedHelmet resources were initialized.");
            return;
        }

        context.renderToTexture(
            m_scene_render_target, getClearColor(),
            [this](FrameHandle frame)
            {
                HelmetUniforms uniforms{};
                const float aspect = m_render_extent.height == 0 ? 16.0f / 9.0f
                                                                : static_cast<float>(m_render_extent.width) /
                                                                      static_cast<float>(m_render_extent.height);
                const glm::vec3 camera_position(0.36f, 0.08f, -3.0f);
                const float yaw_radians = m_fixed_yaw ? m_initial_yaw_radians : m_initial_yaw_radians + m_elapsed_time * 0.25f;
                uniforms.model = glm::rotate(glm::mat4(1.0f), yaw_radians, glm::vec3(0.0f, 1.0f, 0.0f)) *
                                 glm::make_mat4(m_model.transform);
                uniforms.view = glm::lookAt(camera_position, glm::vec3(0.0f, -0.05f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                uniforms.projection = glm::perspective(glm::radians(42.0f), aspect, 0.1f, 100.0f);
                uniforms.camera_position = glm::vec4(camera_position, 1.0f);
                uniforms.light_direction_ambient = glm::vec4(glm::normalize(glm::vec3(-0.42f, -0.72f, -0.48f)), 0.08f);
                uniforms.base_color_factor =
                    glm::vec4(m_model.material_factors.base_color[0], m_model.material_factors.base_color[1],
                              m_model.material_factors.base_color[2], m_model.material_factors.base_color[3]);
                uniforms.emissive_factor_normal_scale =
                    glm::vec4(m_model.material_factors.emissive[0], m_model.material_factors.emissive[1],
                              m_model.material_factors.emissive[2], m_model.material_factors.normal_scale);
                uniforms.metallic_roughness_occlusion =
                    glm::vec4(m_model.material_factors.metallic, m_model.material_factors.roughness,
                              m_model.material_factors.occlusion_strength, 0.0f);
                uniforms.alpha_mode_cutoff_reflection_exposure =
                    glm::vec4(toShaderAlphaMode(m_model.material_factors.alpha_mode), m_model.material_factors.alpha_cutoff,
                              1.22f, 0.86f);
                uniforms.debug_view_gamma = glm::vec4(static_cast<float>(m_debug_view), 2.2f, 1.0f / 2.2f, 0.0f);
                uniforms.padding2 = glm::vec4(m_model.material_factors.double_sided ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);

                if (!m_renderer.uploadUniformRingBuffer(m_uniform_buffer, frame, &uniforms, sizeof(uniforms)))
                {
                    sampleLogError("Failed to upload DamagedHelmet uniforms.");
                    return;
                }

                uint32_t uniform_buffer_slot = m_renderer.getUniformRingBufferSlot(m_uniform_buffer, frame);

                m_renderer.bindPipeline(frame, m_mesh_pipeline);
                m_renderer.bindVertexBuffer(frame, 0, m_model.vertex_buffer);
                m_renderer.bindIndexBuffer(frame, m_model.index_buffer, static_cast<EIndexFormat>(m_model.index_format));
                m_renderer.bindDescriptorSet(frame, m_mesh_pipeline, m_mesh_descriptor_sets[uniform_buffer_slot]);
                m_renderer.drawIndexed(frame, m_model.index_count);
            });

        context.renderToBackbuffer(getClearColor(),
                                   [this](FrameHandle frame)
                                   {
                                       m_renderer.bindPipeline(frame, m_display_pipeline);
                                       m_renderer.bindVertexBuffer(frame, 0, m_fullscreen_vertex_buffer);
                                       m_renderer.bindIndexBuffer(frame, m_fullscreen_index_buffer, EIndexFormat::U_INT16);
                                       m_renderer.bindDescriptorSet(frame, m_display_pipeline, m_display_descriptor_set);
                                       m_renderer.drawIndexed(frame, m_fullscreen_index_count);
                                   });
    }

    ClearColorValue DamagedHelmetSample::getClearColor() const
    {
        return {0.9f, 0.9f, 0.88f, 1.0f};
    }

    void DamagedHelmetSample::destroyRenderResources()
    {
        if (m_display_descriptor_set.isValid())
        {
            m_renderer.destroyDescriptorSet(m_display_descriptor_set);
            m_display_descriptor_set = {};
        }
        for (DescriptorSetHandle descriptor_set : m_mesh_descriptor_sets)
        {
            if (descriptor_set.isValid())
            {
                m_renderer.destroyDescriptorSet(descriptor_set);
            }
        }

        m_mesh_descriptor_sets.clear();

        if (m_display_pipeline.isValid())
        {
            m_renderer.destroyGraphicsPipeline(m_display_pipeline);
            m_display_pipeline = {};
        }

        if (m_mesh_pipeline.isValid())
        {
            m_renderer.destroyGraphicsPipeline(m_mesh_pipeline);
            m_mesh_pipeline = {};
        }

        if (m_scene_render_target.isValid())
        {
            m_renderer.destroyRenderTarget(m_scene_render_target);
            m_scene_render_target = {};
        }

        if (m_scene_depth_texture.isValid())
        {
            m_renderer.destroyTexture(m_scene_depth_texture);
            m_scene_depth_texture = {};
        }

        if (m_scene_texture.isValid())
        {
            m_renderer.destroyTexture(m_scene_texture);
            m_scene_texture = {};
        }
    }

    void DamagedHelmetSample::destroyLoadedResources()
    {
        if (m_uniform_buffer.isValid())
        {
            m_renderer.destroyBuffer(m_uniform_buffer);
            m_uniform_buffer = {};
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
        m_renderer.destroyGltfModel(m_model);
    }

    void DamagedHelmetSample::cleanup()
    {
        sampleLogInfo("Cleaning up " + std::string(getName()));
        m_initialized = false;

        destroyRenderResources();
        destroyLoadedResources();

        if (m_display_shader_program.isValid())
        {
            m_renderer.destroyShaderProgram(m_display_shader_program);
            m_display_shader_program = {};
        }

        if (m_mesh_shader_program.isValid())
        {
            m_renderer.destroyShaderProgram(m_mesh_shader_program);
            m_mesh_shader_program = {};
        }
    }

}  // namespace kera
