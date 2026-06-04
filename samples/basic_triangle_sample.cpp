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

        namespace BasicTriangleShader
        {
            constexpr const char* Path = "shaders/triangle.slang";
            constexpr const char* VertexEntryPoint = "vertexMain";
            constexpr const char* FragmentEntryPoint = "fragmentMain";
        }  // namespace BasicTriangleShader
    }  // namespace

    BasicTriangleSample::BasicTriangleSample(Renderer& renderer)
        : Sample("Basic Triangle"), m_renderer(renderer), m_indexCount(0), m_rotationAngle(0.0f)
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
        const std::string shaderPath = resolveShaderPath(BasicTriangleShader::Path);
        m_shaderProgram = m_renderer.createGraphicsShaderProgram({
            .path = sampleStringView(shaderPath),
            .vertexEntryPoint = stringView(BasicTriangleShader::VertexEntryPoint),
            .fragmentEntryPoint = stringView(BasicTriangleShader::FragmentEntryPoint),
            .source = KERA_SHADER_SOURCE_SLANG_FILE,
            .debugName = {},
        });
        return static_cast<bool>(m_shaderProgram.isValid());
    }

    bool BasicTriangleSample::createGeometry()
    {
        std::vector<Vertex> vertices = {
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{0.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        };

        std::vector<uint16_t> indices = {0, 1, 2};
        m_indexCount = static_cast<uint32_t>(indices.size());

        m_vertexBuffer = m_renderer.createBuffer({
            .size = vertices.size() * sizeof(Vertex),
            .usage = BufferUsageKind::Vertex,
            .memoryAccess = MemoryAccess::CpuWrite,
        });

        if (!m_vertexBuffer.isValid())
        {
            return false;
        }

        if (!m_renderer.uploadBuffer(m_vertexBuffer, vertices.data(), vertices.size() * sizeof(Vertex)))
        {
            return false;
        }

        m_indexBuffer = m_renderer.createBuffer({
            .size = indices.size() * sizeof(uint16_t),
            .usage = BufferUsageKind::Index,
            .memoryAccess = MemoryAccess::CpuWrite,
        });

        if (!m_indexBuffer.isValid())
        {
            return false;
        }

        return m_renderer.uploadBuffer(m_indexBuffer, indices.data(), indices.size() * sizeof(uint16_t));
    }

    bool BasicTriangleSample::createPipeline()
    {
        const VertexInputLayout vertexInput = VertexInputLayoutBuilder{}
                                                  .vertexBinding<Vertex>(0)
                                                  .field(KERA_VERTEX_FIELD(Vertex, position, 0, VertexFormat::Float3))
                                                  .field(KERA_VERTEX_FIELD(Vertex, color, 0, VertexFormat::Float3))
                                                  .layout();

        m_pipeline = m_renderer.createGraphicsPipeline({
            .shaderProgram = m_shaderProgram,
            .vertexInput = vertexInput,
            .topology = PrimitiveTopologyKind::TriangleList,
        });
        return static_cast<bool>(m_pipeline.isValid());
    }

    void BasicTriangleSample::update(float deltaTime)
    {
        m_rotationAngle += deltaTime * 45.0f;
        if (m_rotationAngle >= 360.0f)
        {
            m_rotationAngle -= 360.0f;
        }
    }

    void BasicTriangleSample::render(RenderContext& context)
    {
        if (!m_pipeline.isValid() || !m_vertexBuffer.isValid() || !m_indexBuffer.isValid())
        {
            sampleLogWarning("Render called before Basic Triangle resources were initialized");
            return;
        }

        context.renderToBackbuffer(getClearColor(),
                                   [this](FrameHandle frame)
                                   {
                                       m_renderer.bindPipeline(frame, m_pipeline);
                                       m_renderer.bindVertexBuffer(frame, 0, m_vertexBuffer);
                                       m_renderer.bindIndexBuffer(frame, m_indexBuffer, IndexFormat::UInt16);
                                       m_renderer.drawIndexed(frame, m_indexCount);
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

        if (m_indexBuffer.isValid())
        {
            m_renderer.destroyBuffer(m_indexBuffer);
            m_indexBuffer = {};
        }

        if (m_vertexBuffer.isValid())
        {
            m_renderer.destroyBuffer(m_vertexBuffer);
            m_vertexBuffer = {};
        }

        if (m_shaderProgram.isValid())
        {
            m_renderer.destroyShaderProgram(m_shaderProgram);
            m_shaderProgram = {};
        }
    }
}  // namespace kera
