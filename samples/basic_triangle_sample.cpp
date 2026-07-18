// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "basic_triangle_sample.h"

#include "render_context.h"
#include "sample_utils.h"

#include <glm/glm.hpp>

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

        namespace basic_triangle_shader
        {
            constexpr const char* kPath = "shaders/triangle.slang";
            constexpr const char* kVertexEntryPoint = "vertexMain";
            constexpr const char* kFragmentEntryPoint = "fragmentMain";
        }  // namespace basic_triangle_shader
    }  // namespace

    BasicTriangleSample::BasicTriangleSample(Renderer& renderer)
        : Sample("Basic Triangle"), m_renderer(renderer), m_index_count(0), m_rotation_angle(0.0f)
    {
    }

    void BasicTriangleSample::initialize()
    {
        sampleLogInfo("Initializing " + std::string(getName()));

        if (!createShaderProgram())
        {
            sampleLogError("Failed to create Basic Triangle shader program");
            return;
        }

        if (!createGeometry())
        {
            sampleLogError("Failed to create Basic Triangle geometry");
            return;
        }

        if (!createPipeline())
        {
            sampleLogError("Failed to create Basic Triangle pipeline");
            return;
        }

        sampleLogInfo("Basic Triangle sample initialized successfully");
    }

    bool BasicTriangleSample::createShaderProgram()
    {
        const std::string shader_path = resolveShaderPath(basic_triangle_shader::kPath);
        m_shader_program = m_renderer.createGraphicsShaderProgram({
            .path = sampleStringView(shader_path),
            .vertex_entry_point = stringView(basic_triangle_shader::kVertexEntryPoint),
            .fragment_entry_point = stringView(basic_triangle_shader::kFragmentEntryPoint),
            .source = EShaderSourceKind::SLANG_FILE,
            .debug_name = {},
        });
        return static_cast<bool>(m_shader_program.isValid());
    }

    bool BasicTriangleSample::createGeometry()
    {
        std::vector<Vertex> vertices = {
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{0.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        };

        std::vector<uint16_t> indices = {0, 1, 2};
        m_index_count = static_cast<uint32_t>(indices.size());

        m_vertex_buffer = m_renderer.createBuffer({
            .size = vertices.size() * sizeof(Vertex),
            .usage = EBufferUsageKind::VERTEX,
            .memory_access = EMemoryAccess::CPU_WRITE,
        });

        if (!m_vertex_buffer.isValid())
        {
            return false;
        }

        if (!m_renderer.uploadBuffer(m_vertex_buffer, vertices.data(), vertices.size() * sizeof(Vertex)))
        {
            return false;
        }

        m_index_buffer = m_renderer.createBuffer({
            .size = indices.size() * sizeof(uint16_t),
            .usage = EBufferUsageKind::INDEX,
            .memory_access = EMemoryAccess::CPU_WRITE,
        });

        if (!m_index_buffer.isValid())
        {
            return false;
        }

        return m_renderer.uploadBuffer(m_index_buffer, indices.data(), indices.size() * sizeof(uint16_t));
    }

    bool BasicTriangleSample::createPipeline()
    {
        const VertexInputLayout vertex_input = VertexInputLayoutBuilder{}
                                                   .vertexBinding<Vertex>(0)
                                                   .field(KERA_VERTEX_FIELD(Vertex, position, 0, EVertexFormat::FLOAT3))
                                                   .field(KERA_VERTEX_FIELD(Vertex, color, 0, EVertexFormat::FLOAT3))
                                                   .layout();

        m_pipeline = m_renderer.createGraphicsPipeline({
            .shader_program = m_shader_program,
            .vertex_input = vertex_input,
            .topology = EPrimitiveTopologyKind::TRIANGLE_LIST,
        });
        return static_cast<bool>(m_pipeline.isValid());
    }

    void BasicTriangleSample::update(float delta_time)
    {
        m_rotation_angle += delta_time * 45.0f;
        if (m_rotation_angle >= 360.0f)
        {
            m_rotation_angle -= 360.0f;
        }
    }

    void BasicTriangleSample::render(RenderContext& context)
    {
        if (!m_pipeline.isValid() || !m_vertex_buffer.isValid() || !m_index_buffer.isValid())
        {
            sampleLogWarning("Render called before Basic Triangle resources were initialized");
            return;
        }

        context.renderToBackbuffer(getClearColor(),
                                   [this](FrameHandle frame)
                                   {
                                       m_renderer.bindPipeline(frame, m_pipeline);
                                       m_renderer.bindVertexBuffer(frame, 0, m_vertex_buffer);
                                       m_renderer.bindIndexBuffer(frame, m_index_buffer, EIndexFormat::U_INT16);
                                       m_renderer.drawIndexed(frame, m_index_count);
                                   });
    }

    void BasicTriangleSample::cleanup()
    {
        sampleLogInfo("Cleaning up " + std::string(getName()));
        if (m_pipeline.isValid())
        {
            m_renderer.destroyGraphicsPipeline(m_pipeline);
            m_pipeline = {};
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
