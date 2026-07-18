// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "instanced_triangle_many_lights_sample.h"

#include "render_context.h"
#include "sample_utils.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace kera
{

    namespace
    {

        struct Vertex
        {
            glm::vec3 position;
            glm::vec3 color;
        };

        struct InstanceData
        {
            glm::mat4 model_matrix;
        };

        struct GeometryUniforms
        {
            glm::mat4 projection;
            glm::mat4 view;
        };

        struct LightData
        {
            glm::vec4 position_radius;
            glm::vec4 color_intensity;
        };

        struct LightingUniforms
        {
            glm::vec4 ambient_time_light_count;
            std::array<LightData, 64> lights;
        };

        namespace many_lights_shader
        {
            constexpr const char* kPath = "shaders/instanced_triangle_many_lights.slang";
            constexpr const char* kGeometryVertexEntryPoint = "geometryVertexMain";
            constexpr const char* kGeometryFragmentEntryPoint = "geometryFragmentMain";
            constexpr const char* kFullscreenVertexEntryPoint = "fullscreenVertexMain";
            constexpr const char* kLightingFragmentEntryPoint = "lightingFragmentMain";

            constexpr const char* kGeometryParams = "geometryParams";
            constexpr const char* kLightingParams = "lightingParams";
            constexpr const char* kSceneTexture = "sceneTexture";
            constexpr const char* kSceneSampler = "sceneSampler";
        }  // namespace ManyLightsShader

    }  // namespace

    InstancedTriangleManyLightsSample::InstancedTriangleManyLightsSample(Renderer& renderer)
        : Sample("Instanced Triangle Many Lights"), m_renderer(renderer)
    {
    }

    void InstancedTriangleManyLightsSample::initialize()
    {
        sampleLogInfo("Initializing " + std::string(getName()));
        m_initialized = false;

        if (!createShaderPrograms())
        {
            sampleLogError("Failed to create many-lights shader programs");
            cleanup();
            return;
        }

        if (!createGeometry())
        {
            sampleLogError("Failed to create many-lights geometry");
            cleanup();
            return;
        }

        if (!recreateRenderResources(m_renderer.getDrawableExtent()))
        {
            sampleLogError("Failed to create many-lights render resources");
            cleanup();
            return;
        }

        m_initialized = true;
        sampleLogInfo("Many-lights sample initialized successfully");
    }

    bool InstancedTriangleManyLightsSample::createShaderPrograms()
    {
        const std::string shader_path = resolveShaderPath(many_lights_shader::kPath);

        m_geometry_shader_program = m_renderer.createGraphicsShaderProgram({
            .path = sampleStringView(shader_path),
            .vertex_entry_point = stringView(many_lights_shader::kGeometryVertexEntryPoint),
            .fragment_entry_point = stringView(many_lights_shader::kGeometryFragmentEntryPoint),
            .source = EShaderSourceKind::SLANG_FILE,
            .debug_name = {},
        });

        if (!m_geometry_shader_program.isValid())
        {
            return false;
        }

        m_lighting_shader_program = m_renderer.createGraphicsShaderProgram({
            .path = sampleStringView(shader_path),
            .vertex_entry_point = stringView(many_lights_shader::kFullscreenVertexEntryPoint),
            .fragment_entry_point = stringView(many_lights_shader::kLightingFragmentEntryPoint),
            .source = EShaderSourceKind::SLANG_FILE,
            .debug_name = {},
        });

        return m_lighting_shader_program.isValid();
    }

    bool InstancedTriangleManyLightsSample::createGeometry()
    {
        const std::vector<Vertex> vertices = {
            {{-0.32f, -0.32f, 0.0f}, {1.0f, 0.1f, 0.0f}},
            {{0.32f, -0.32f, 0.0f}, {0.0f, 1.0f, 0.35f}},
            {{0.0f, 0.32f, 0.0f}, {0.15f, 0.3f, 1.0f}},
        };
        const std::vector<uint16_t> indices = {0, 1, 2};
        m_index_count = static_cast<uint32_t>(indices.size());

        m_vertex_buffer = m_renderer.createBuffer({
            .size = vertices.size() * sizeof(Vertex),
            .usage = EBufferUsageKind::VERTEX,
            .memory_access = EMemoryAccess::CPU_WRITE,
        });
        if (!m_vertex_buffer.isValid() ||
            !m_renderer.uploadBuffer(m_vertex_buffer, vertices.data(), vertices.size() * sizeof(Vertex)))
        {
            return false;
        }

        m_index_buffer = m_renderer.createBuffer({
            .size = indices.size() * sizeof(uint16_t),
            .usage = EBufferUsageKind::INDEX,
            .memory_access = EMemoryAccess::CPU_WRITE,
        });

        if (!m_index_buffer.isValid() ||
            !m_renderer.uploadBuffer(m_index_buffer, indices.data(), indices.size() * sizeof(uint16_t)))
        {
            return false;
        }

        m_instance_count = 120;
        std::vector<InstanceData> instances(m_instance_count);
        m_instance_buffer = m_renderer.createBuffer({
            .size = instances.size() * sizeof(InstanceData),
            .usage = EBufferUsageKind::VERTEX,
            .memory_access = EMemoryAccess::CPU_WRITE,
        });

        if (!m_instance_buffer.isValid())
        {
            return false;
        }

        const auto& fullscreen_vertices = fullscreenTriangleVertices();
        const auto& fullscreen_indices = fullscreenTriangleIndices();
        m_fullscreen_index_count = static_cast<uint32_t>(fullscreen_indices.size());

        m_fullscreen_vertex_buffer = m_renderer.createBuffer({
            .size = fullscreen_vertices.size() * sizeof(FullscreenTriangleVertex),
            .usage = EBufferUsageKind::VERTEX,
            .memory_access = EMemoryAccess::CPU_WRITE,
        });

        if (!m_fullscreen_vertex_buffer.isValid() ||
            !m_renderer.uploadBuffer(m_fullscreen_vertex_buffer, fullscreen_vertices.data(),
                                     fullscreen_vertices.size() * sizeof(FullscreenTriangleVertex)))
        {
            return false;
        }

        m_fullscreen_index_buffer = m_renderer.createBuffer({
            .size = fullscreen_indices.size() * sizeof(uint16_t),
            .usage = EBufferUsageKind::INDEX,
            .memory_access = EMemoryAccess::CPU_WRITE,
        });

        if (!m_fullscreen_index_buffer.isValid() ||
            !m_renderer.uploadBuffer(m_fullscreen_index_buffer, fullscreen_indices.data(),
                                     fullscreen_indices.size() * sizeof(uint16_t)))
        {
            return false;
        }

        m_geometry_uniform_buffer = m_renderer.createBuffer({
            .size = sizeof(GeometryUniforms),
            .usage = EBufferUsageKind::UNIFORM,
            .memory_access = EMemoryAccess::CPU_WRITE,
        });

        m_lighting_uniform_buffer = m_renderer.createBuffer({
            .size = sizeof(LightingUniforms),
            .usage = EBufferUsageKind::UNIFORM,
            .memory_access = EMemoryAccess::CPU_WRITE,
        });

        return m_geometry_uniform_buffer.isValid() && m_lighting_uniform_buffer.isValid();
    }

    bool InstancedTriangleManyLightsSample::recreateRenderResources(Extent2D extent)
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
        });

        if (!m_scene_texture.isValid())
        {
            return false;
        }

        m_scene_depth_texture = m_renderer.createTexture({
            .width = extent.width,
            .height = extent.height,
            .format = ETextureFormat::DEPTH32,
            .render_target = true,
            .sampled = false,
            .depth_stencil = true,
        });

        if (!m_scene_depth_texture.isValid())
        {
            return false;
        }

        m_scene_render_target = m_renderer.createRenderTarget({
            .color_texture = m_scene_texture,
            .depth_texture = m_scene_depth_texture,
        });

        if (!m_scene_render_target.isValid())
        {
            return false;
        }

        if (!m_scene_sampler.isValid())
        {
            m_scene_sampler = m_renderer.createSampler({});
            if (!m_scene_sampler.isValid())
            {
                return false;
            }
        }

        return createPipelinesAndDescriptors();
    }

    bool InstancedTriangleManyLightsSample::createPipelinesAndDescriptors()
    {
        const VertexInputLayout geometry_vertex_input =
            VertexInputLayoutBuilder{}
                .vertexBinding<Vertex>(0)
                .vertexBinding<InstanceData>(1, static_cast<KeraVertexInputRate>(EVertexInputRate::INSTANCE))
                .field(KERA_VERTEX_FIELD(Vertex, position, 0, EVertexFormat::FLOAT3))
                .field(KERA_VERTEX_FIELD(Vertex, color, 0, EVertexFormat::FLOAT3))
                .field(KERA_VERTEX_FIELD(InstanceData, model_matrix, 1, EVertexFormat::FLOAT4))
                .layout();

        m_geometry_pipeline = m_renderer.createGraphicsPipeline({
            .shader_program = m_geometry_shader_program,
            .vertex_input = geometry_vertex_input,
            .render_target = m_scene_render_target,
            .cull_mode = ECullModeKind::NONE,
            .depth_test = true,
            .depth_write = true,
        });

        if (!m_geometry_pipeline.isValid())
        {
            return false;
        }

        m_geometry_descriptor_set = m_renderer.createDescriptorSet(m_geometry_pipeline);
        if (!m_geometry_descriptor_set.isValid() ||
            !m_renderer.updateDescriptors(m_geometry_descriptor_set)
                 .uniform<GeometryUniforms>(many_lights_shader::kGeometryParams, m_geometry_uniform_buffer)
                 .ok())
        {
            return false;
        }

        const VertexInputLayout lighting_vertex_input =
            VertexInputLayoutBuilder{}
                .vertexBinding<FullscreenTriangleVertex>(0)
                .field(KERA_VERTEX_FIELD(FullscreenTriangleVertex, position, 0, EVertexFormat::FLOAT2))
                .field(KERA_VERTEX_FIELD(FullscreenTriangleVertex, uv, 0, EVertexFormat::FLOAT2))
                .layout();

        m_lighting_pipeline = m_renderer.createGraphicsPipeline({
            .shader_program = m_lighting_shader_program,
            .vertex_input = lighting_vertex_input,
            .cull_mode = ECullModeKind::NONE,
        });

        if (!m_lighting_pipeline.isValid())
        {
            return false;
        }

        m_lighting_descriptor_set = m_renderer.createDescriptorSet(m_lighting_pipeline);
        if (!m_lighting_descriptor_set.isValid())
        {
            return false;
        }

        return m_renderer.updateDescriptors(m_lighting_descriptor_set)
            .uniform<LightingUniforms>(many_lights_shader::kLightingParams, m_lighting_uniform_buffer)
            .sampledImage(many_lights_shader::kSceneTexture, m_scene_texture)
            .sampler(many_lights_shader::kSceneSampler, m_scene_sampler)
            .ok();
    }

    void InstancedTriangleManyLightsSample::resize(Extent2D extent)
    {
        if (extent == m_render_extent || extent.width == 0 || extent.height == 0)
        {
            return;
        }

        if (!recreateRenderResources(extent))
        {
            sampleLogError("Failed to resize many-lights render resources.");
            m_initialized = false;
        }
    }

    void InstancedTriangleManyLightsSample::update(float delta_time)
    {
        if (!m_initialized)
        {
            return;
        }

        m_elapsed_time += delta_time;
        const float angle_radians = m_elapsed_time;
        updateInstances(angle_radians);
        updateGeometryUniforms();
        updateLightingUniforms(angle_radians);
    }

    void InstancedTriangleManyLightsSample::updateInstances(float angle_radians)
    {
        void* mapped_data = nullptr;
        if (!m_renderer.mapBuffer(m_instance_buffer, &mapped_data))
        {
            sampleLogError("Failed to map many-lights instance buffer.");
            return;
        }

        InstanceData* data = static_cast<InstanceData*>(mapped_data);
        for (uint32_t i = 0; i < m_instance_count; ++i)
        {
            const float x = (static_cast<float>(i % 15) - 7.0f) * 0.62f;
            const float y = (static_cast<float>(i / 15) - 3.5f) * 0.62f;
            const float phase = static_cast<float>(i) * 0.37f;
            const float bob = std::sin(angle_radians * 1.8f + phase) * 0.08f;
            const float drift = std::cos(angle_radians * 1.1f + phase) * 0.05f;
            const float spin = angle_radians * (0.55f + 0.02f * static_cast<float>(i % 7)) + phase;
            const float scale = 0.58f + 0.08f * std::sin(angle_radians + phase);

            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x + drift, y + bob, 0.0f));
            model = glm::rotate(model, spin, glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, glm::vec3(scale, scale, 1.0f));
            data[i].model_matrix = model;
        }

        m_renderer.unmapBuffer(m_instance_buffer);
    }

    void InstancedTriangleManyLightsSample::updateGeometryUniforms()
    {
        void* mapped_data = nullptr;
        if (!m_renderer.mapBuffer(m_geometry_uniform_buffer, &mapped_data))
        {
            sampleLogError("Failed to map many-lights geometry uniforms.");
            return;
        }

        GeometryUniforms* uniforms = static_cast<GeometryUniforms*>(mapped_data);
        const float aspect = m_render_extent.height == 0
                                 ? 16.0f / 9.0f
                                 : static_cast<float>(m_render_extent.width) / static_cast<float>(m_render_extent.height);
        uniforms->view =
            glm::lookAt(glm::vec3(0.0f, 0.0f, -8.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        uniforms->projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

        m_renderer.unmapBuffer(m_geometry_uniform_buffer);
    }

    void InstancedTriangleManyLightsSample::updateLightingUniforms(float angle_radians)
    {
        void* mapped_data = nullptr;
        if (!m_renderer.mapBuffer(m_lighting_uniform_buffer, &mapped_data))
        {
            sampleLogError("Failed to map many-lights lighting uniforms.");
            return;
        }

        LightingUniforms* uniforms = static_cast<LightingUniforms*>(mapped_data);
        uniforms->ambient_time_light_count = glm::vec4(0.08f, angle_radians, static_cast<float>(kLightCount), 0.0f);

        for (uint32_t i = 0; i < kLightCount; ++i)
        {
            const float ring = static_cast<float>(i % 8);
            const float row = static_cast<float>(i / 8);
            const float phase = static_cast<float>(i) * 0.41f;
            const float radius = 0.13f + 0.035f * std::sin(angle_radians + phase);
            const float center_x = 0.12f + ring * 0.11f;
            const float center_y = 0.15f + row * 0.10f;
            const float x = center_x + 0.055f * std::cos(angle_radians * 0.9f + phase);
            const float y = center_y + 0.045f * std::sin(angle_radians * 1.2f + phase);

            const glm::vec3 color(0.55f + 0.45f * std::sin(phase + 0.0f), 0.55f + 0.45f * std::sin(phase + 2.1f),
                                  0.55f + 0.45f * std::sin(phase + 4.2f));
            uniforms->lights[i].position_radius = glm::vec4(x, y, radius, 0.0f);
            uniforms->lights[i].color_intensity = glm::vec4(color, 0.75f + 0.2f * std::sin(angle_radians + phase));
        }

        m_renderer.unmapBuffer(m_lighting_uniform_buffer);
    }

    void InstancedTriangleManyLightsSample::render(RenderContext& context)
    {
        if (!m_initialized)
        {
            return;
        }

        if (!m_geometry_pipeline.isValid() || !m_lighting_pipeline.isValid() || !m_scene_render_target.isValid() ||
            !m_geometry_descriptor_set.isValid() || !m_lighting_descriptor_set.isValid())
        {
            sampleLogWarning("Render called before many-lights resources were initialized");
            return;
        }

        context.renderToTexture(m_scene_render_target, {0.0f, 0.0f, 0.0f, 1.0f},
                                [this](FrameHandle frame)
                                {
                                    m_renderer.bindPipeline(frame, m_geometry_pipeline);
                                    m_renderer.bindVertexBuffer(frame, 0, m_vertex_buffer);
                                    m_renderer.bindVertexBuffer(frame, 1, m_instance_buffer);
                                    m_renderer.bindIndexBuffer(frame, m_index_buffer, EIndexFormat::U_INT16);
                                    m_renderer.bindDescriptorSet(frame, m_geometry_pipeline, m_geometry_descriptor_set);
                                    m_renderer.drawIndexed(frame, m_index_count, m_instance_count);
                                });

        context.renderToBackbuffer(getClearColor(),
                                   [this](FrameHandle frame)
                                   {
                                       m_renderer.bindPipeline(frame, m_lighting_pipeline);
                                       m_renderer.bindVertexBuffer(frame, 0, m_fullscreen_vertex_buffer);
                                       m_renderer.bindIndexBuffer(frame, m_fullscreen_index_buffer, EIndexFormat::U_INT16);
                                       m_renderer.bindDescriptorSet(frame, m_lighting_pipeline, m_lighting_descriptor_set);
                                       m_renderer.drawIndexed(frame, m_fullscreen_index_count);
                                   });
    }

    void InstancedTriangleManyLightsSample::destroyRenderResources()
    {
        if (m_lighting_descriptor_set.isValid())
        {
            m_renderer.destroyDescriptorSet(m_lighting_descriptor_set);
            m_lighting_descriptor_set = {};
        }

        if (m_geometry_descriptor_set.isValid())
        {
            m_renderer.destroyDescriptorSet(m_geometry_descriptor_set);
            m_geometry_descriptor_set = {};
        }

        if (m_lighting_pipeline.isValid())
        {
            m_renderer.destroyGraphicsPipeline(m_lighting_pipeline);
            m_lighting_pipeline = {};
        }

        if (m_geometry_pipeline.isValid())
        {
            m_renderer.destroyGraphicsPipeline(m_geometry_pipeline);
            m_geometry_pipeline = {};
        }

        if (m_scene_render_target.isValid())
        {
            m_renderer.destroyRenderTarget(m_scene_render_target);
            m_scene_render_target = {};
        }

        if (m_scene_texture.isValid())
        {
            m_renderer.destroyTexture(m_scene_texture);
            m_scene_texture = {};
        }

        if (m_scene_depth_texture.isValid())
        {
            m_renderer.destroyTexture(m_scene_depth_texture);
            m_scene_depth_texture = {};
        }
    }

    void InstancedTriangleManyLightsSample::cleanup()
    {
        sampleLogInfo("Cleaning up " + std::string(getName()));
        m_initialized = false;

        destroyRenderResources();

        if (m_scene_sampler.isValid())
        {
            m_renderer.destroySampler(m_scene_sampler);
            m_scene_sampler = {};
        }

        if (m_lighting_uniform_buffer.isValid())
        {
            m_renderer.destroyBuffer(m_lighting_uniform_buffer);
            m_lighting_uniform_buffer = {};
        }

        if (m_geometry_uniform_buffer.isValid())
        {
            m_renderer.destroyBuffer(m_geometry_uniform_buffer);
            m_geometry_uniform_buffer = {};
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

        if (m_instance_buffer.isValid())
        {
            m_renderer.destroyBuffer(m_instance_buffer);
            m_instance_buffer = {};
        }

        if (m_index_buffer.isValid())
        {
            m_renderer.destroyBuffer(m_index_buffer);
            m_index_buffer = {};
        }

        if (m_vertex_buffer.isValid())
        {
            m_renderer.destroyBuffer(m_vertex_buffer);
            m_vertex_buffer = {};
        }

        if (m_lighting_shader_program.isValid())
        {
            m_renderer.destroyShaderProgram(m_lighting_shader_program);
            m_lighting_shader_program = {};
        }

        if (m_geometry_shader_program.isValid())
        {
            m_renderer.destroyShaderProgram(m_geometry_shader_program);
            m_geometry_shader_program = {};
        }
    }

}  // namespace kera
