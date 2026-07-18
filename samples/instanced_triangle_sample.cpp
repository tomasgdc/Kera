// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "instanced_triangle_sample.h"

#include "render_context.h"
#include "sample_utils.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstddef>
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

        struct Uniforms
        {
            glm::mat4 projection;
            glm::mat4 view;
        };

        struct BasicInstanceData
        {
            glm::mat4 model_matrix;
        };

        namespace instanced_triangle_shader
        {
            constexpr const char* kPath = "shaders/instanced_triangle.slang";
            constexpr const char* kVertexEntryPoint = "vertexMain";
            constexpr const char* kFragmentEntryPoint = "fragmentMain";
            constexpr const char* kGlobalParams = "globalParams";
        }  // namespace InstancedTriangleShader

        constexpr uint32_t kUniformRingSlots = 3;
    }  // namespace

    InstancedTriangleSample::InstancedTriangleSample(Renderer& renderer)
        : Sample("Instanced Triangle"), m_renderer(renderer), m_index_count(0), m_instance_count(0), m_rotation_angle(0.0f)
    {
    }

    void InstancedTriangleSample::initialize()
    {
        sampleLogInfo("Initializing " + std::string(getName()));

        if (!createShaderProgram())
        {
            sampleLogError("Failed to create Instanced Triangle shader program");
            return;
        }

        if (!createGeometry())
        {
            sampleLogError("Failed to create Instanced Triangle geometry");
            return;
        }

        if (!createPipeline())
        {
            sampleLogError("Failed to create Instanced Triangle pipeline");
            return;
        }

        sampleLogInfo("Instanced Triangle sample initialized successfully");
    }

    bool InstancedTriangleSample::createShaderProgram()
    {
        const std::string shader_path = resolveShaderPath(instanced_triangle_shader::kPath);
        m_shader_program = m_renderer.createGraphicsShaderProgram({
            .path = sampleStringView(shader_path),
            .vertex_entry_point = stringView(instanced_triangle_shader::kVertexEntryPoint),
            .fragment_entry_point = stringView(instanced_triangle_shader::kFragmentEntryPoint),
            .source = EShaderSourceKind::SLANG_FILE,
            .debug_name = {},
        });
        return static_cast<bool>(m_shader_program.isValid());
    }

    bool InstancedTriangleSample::createGeometry()
    {
        std::vector<Vertex> vertices = {
            {{-0.35f, -0.35f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{0.35f, -0.35f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{0.0f, 0.35f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        };

        std::vector<uint16_t> indices = {0, 1, 2};
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

        m_instance_count = 50;
        std::vector<BasicInstanceData> instance_data(m_instance_count);
        for (uint32_t i = 0; i < m_instance_count; ++i)
        {
            const float x = (static_cast<float>(i % 10) - 4.5f) * 0.9f;
            const float y = (static_cast<float>(i / 10) - 2.0f) * 0.8f;
            const float z = 0.0f;
            instance_data[i].model_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
        }

        m_instance_buffer = m_renderer.createBuffer({
            .size = m_instance_count * sizeof(BasicInstanceData),
            .usage = EBufferUsageKind::VERTEX,
            .memory_access = EMemoryAccess::CPU_WRITE,
        });

        if (!m_instance_buffer.isValid() || !m_renderer.uploadBuffer(m_instance_buffer, instance_data.data(),
                                                                    instance_data.size() * sizeof(BasicInstanceData)))
        {
            return false;
        }

        m_uniform_buffer = m_renderer.createUniformRingBuffer(sizeof(Uniforms), kUniformRingSlots);

        return m_uniform_buffer.isValid();
    }

    bool InstancedTriangleSample::createPipeline()
    {
        const VertexInputLayout vertex_input =
            VertexInputLayoutBuilder{}
                .vertexBinding<Vertex>(0)
                .vertexBinding<BasicInstanceData>(1, static_cast<KeraVertexInputRate>(EVertexInputRate::INSTANCE))
                .field(KERA_VERTEX_FIELD(Vertex, position, 0, EVertexFormat::FLOAT3))
                .field(KERA_VERTEX_FIELD(Vertex, color, 0, EVertexFormat::FLOAT3))
                .field(KERA_VERTEX_FIELD(BasicInstanceData, model_matrix, 1, EVertexFormat::FLOAT4))
                .layout();

        m_pipeline = m_renderer.createGraphicsPipeline({
            .shader_program = m_shader_program,
            .vertex_input = vertex_input,
            .topology = EPrimitiveTopologyKind::TRIANGLE_LIST,
            .cull_mode = ECullModeKind::NONE,
        });

        if (!m_pipeline.isValid())
        {
            return false;
        }

        KeraUniformRingBufferInfo ring_info = m_renderer.getUniformRingBufferInfo(m_uniform_buffer);

        m_uniform_descriptor_sets.clear();
        m_uniform_descriptor_sets.reserve(ring_info.slot_count);

        for (uint32_t slot = 0; slot < ring_info.slot_count; ++slot)
        {
            DescriptorSetHandle descriptor_set = m_renderer.createDescriptorSet(m_pipeline);
            if (!descriptor_set.isValid())
            {
                return false;
            }

                const std::size_t uniform_offset = m_renderer.getUniformRingBufferSlotOffset(m_uniform_buffer, slot);
                if (!m_renderer.updateDescriptors(descriptor_set)
                     .uniform<Uniforms>(instanced_triangle_shader::kGlobalParams, m_uniform_buffer, uniform_offset)
                     .ok())
            {
                return false;
            }
            m_uniform_descriptor_sets.push_back(descriptor_set);
        }

        return true;
    }

    void InstancedTriangleSample::update(float delta_time)
    {
        m_rotation_angle += delta_time * 45.0f;
        if (m_rotation_angle >= 360.0f)
        {
            m_rotation_angle -= 360.0f;
        }

        const float angle_radians = glm::radians(m_rotation_angle);
        void* mapped_data = nullptr;
        if (m_renderer.mapBuffer(m_instance_buffer, &mapped_data))
        {
            BasicInstanceData* data = static_cast<BasicInstanceData*>(mapped_data);

            for (uint32_t i = 0; i < m_instance_count; ++i)
            {
                const float base_x = (static_cast<float>(i % 10) - 4.5f) * 0.9f;
                const float base_y = (static_cast<float>(i / 10) - 2.0f) * 0.8f;
                const float phase = static_cast<float>(i) * 0.31f;
                const float bob = std::sin(angle_radians * 2.0f + phase) * 0.16f;
                const float drift = std::cos(angle_radians * 1.3f + phase) * 0.08f;
                const float spin = angle_radians + phase;
                const float scale = 0.78f + 0.08f * std::sin(angle_radians * 1.7f + phase);

                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(base_x + drift, base_y + bob, 0.0f));
                model = glm::rotate(model, spin, glm::vec3(0.0f, 0.0f, 1.0f));
                model = glm::scale(model, glm::vec3(scale, scale, 1.0f));
                data[i].model_matrix = model;
            }

            m_renderer.unmapBuffer(m_instance_buffer);
        }
        else
        {
            sampleLogError("Failed to map instance buffer for update.");
        }
    }

    void InstancedTriangleSample::render(RenderContext& context)
    {
        if (!m_pipeline.isValid() || !m_vertex_buffer.isValid() || !m_index_buffer.isValid() ||
            !m_instance_buffer.isValid() || !m_uniform_buffer.isValid() || m_uniform_descriptor_sets.empty())
        {
            sampleLogWarning("Render called before Instanced Triangle resources were initialized");
            return;
        }

        context.renderToBackbuffer(
            getClearColor(),
            [this](FrameHandle frame)
            {
                const float angle_radians = glm::radians(m_rotation_angle);
                const float time_factor = std::sin(angle_radians * 0.5f) * 0.05f;
                Uniforms uniforms{};
                uniforms.view =
                    glm::lookAt(glm::vec3(0.0f, 0.0f, -7.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                uniforms.projection = glm::perspective(glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 100.0f) *
                                      glm::scale(glm::mat4(1.0f), glm::vec3(1.0f + time_factor, 1.0f, 1.0f));
                if (!m_renderer.uploadUniformRingBuffer(m_uniform_buffer, frame, &uniforms, sizeof(Uniforms)))
                {
                    sampleLogError("Failed to upload instanced triangle uniforms.");
                    return;
                }

                uint32_t uniform_buffer_slot = m_renderer.getUniformRingBufferSlot(m_uniform_buffer, frame);
                const std::size_t uniform_offset =
                    m_renderer.getUniformRingBufferSlotOffset(m_uniform_buffer, uniform_buffer_slot);

                DescriptorSetHandle uniform_descriptor_set = m_uniform_descriptor_sets[uniform_buffer_slot];
                if (!m_renderer.updateDescriptors(uniform_descriptor_set)
                         .uniform<Uniforms>(instanced_triangle_shader::kGlobalParams, m_uniform_buffer, uniform_offset)
                         .ok())
                {
                    sampleLogError("Failed to update instanced triangle uniform descriptor.");
                    return;
                }

                m_renderer.bindPipeline(frame, m_pipeline);
                m_renderer.bindVertexBuffer(frame, 0, m_vertex_buffer);
                m_renderer.bindVertexBuffer(frame, 1, m_instance_buffer);
                m_renderer.bindIndexBuffer(frame, m_index_buffer, EIndexFormat::U_INT16);
                m_renderer.bindDescriptorSet(frame, m_pipeline, uniform_descriptor_set);
                m_renderer.drawIndexed(frame, m_index_count, m_instance_count);
            });
    }

    void InstancedTriangleSample::cleanup()
    {
        sampleLogInfo("Cleaning up " + std::string(getName()));
        for (DescriptorSetHandle descriptor_set : m_uniform_descriptor_sets)
        {
            if (descriptor_set.isValid())
            {
                m_renderer.destroyDescriptorSet(descriptor_set);
            }
        }
        m_uniform_descriptor_sets.clear();

        if (m_pipeline.isValid())
        {
            m_renderer.destroyGraphicsPipeline(m_pipeline);
            m_pipeline = {};
        }

        if (m_uniform_buffer.isValid())
        {
            m_renderer.destroyBuffer(m_uniform_buffer);
            m_uniform_buffer = {};
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

        if (m_shader_program.isValid())
        {
            m_renderer.destroyShaderProgram(m_shader_program);
            m_shader_program = {};
        }
    }
}  // namespace kera
