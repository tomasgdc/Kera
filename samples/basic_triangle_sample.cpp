#include "basic_triangle_sample.h"

#include <cstddef>
#include <glm/glm.hpp>
#include <vector>

#include "kera/utilities/logger.h"
#include "render_context.h"
#include "sample_utils.h"

namespace kera 
{
    namespace
    {
        struct Vertex
        {
            glm::vec3 position;
            glm::vec3 color;
        };
    }  // namespace

    BasicTriangleSample::BasicTriangleSample(IRenderer& renderer)
        : Sample("Basic Triangle with Slang"),
        m_renderer(renderer),
        m_indexCount(0),
        m_rotationAngle(0.0f) 
    {
    }

    void BasicTriangleSample::initialize() 
    {
        Logger::getInstance().info("Initializing " + std::string(getName()));

        if (!createShaderProgram()) 
        {
            Logger::getInstance().error("Failed to create shader program");
            return;
        }

        if (!createGeometry()) 
        {
            Logger::getInstance().error("Failed to create geometry");
            return;
        }

        if (!createPipeline()) 
        {
            Logger::getInstance().error("Failed to create pipeline");
            return;
        }

        Logger::getInstance().info("Triangle sample initialized successfully");
    }

    bool BasicTriangleSample::createShaderProgram() 
    {
        ShaderProgramDesc programDesc
        {
            .stages =
                {
                    {
                        .path = resolveShaderPath("shaders/triangle.slang"),
                        .entryPoint = "vertexMain",
                        .stage = ShaderStage::Vertex,
                        .source = ShaderSourceKind::SlangFile,
                    },
                    {
                        .path = resolveShaderPath("shaders/triangle.slang"),
                        .entryPoint = "fragmentMain",
                        .stage = ShaderStage::Fragment,
                        .source = ShaderSourceKind::SlangFile,
                    },
                },
        };

        m_shaderProgram = m_renderer.createShaderProgram(programDesc);
        return static_cast<bool>(m_shaderProgram.isValid());
    }

    bool BasicTriangleSample::createGeometry() 
    {
        std::vector<Vertex> vertices = 
        {
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{0.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        };

        std::vector<uint16_t> indices = { 0, 1, 2 };
        m_indexCount = static_cast<uint32_t>(indices.size());

        m_vertexBuffer = m_renderer.createBuffer(
            {
                .size = vertices.size() * sizeof(Vertex),
                .usage = BufferUsageKind::Vertex,
                .memoryAccess = MemoryAccess::CpuWrite,
            });

        if (!m_vertexBuffer.isValid()) 
        {
            return false;
        }

        if (!m_renderer.uploadBuffer(m_vertexBuffer, vertices.data(),
            vertices.size() * sizeof(Vertex))) 
        {
            return false;
        }

        m_indexBuffer = m_renderer.createBuffer(
            {
                .size = indices.size() * sizeof(uint16_t),
                .usage = BufferUsageKind::Index,
                .memoryAccess = MemoryAccess::CpuWrite,
            });

        if (!m_indexBuffer.isValid()) 
        {
            return false;
        }

        return m_renderer.uploadBuffer(m_indexBuffer, indices.data(),
            indices.size() * sizeof(uint16_t));
    }

    bool BasicTriangleSample::createPipeline() 
    {
        GraphicsPipelineDesc pipelineDesc{};
        pipelineDesc.topology = PrimitiveTopologyKind::TriangleList;
        pipelineDesc.vertexLayout.bindings.push_back(
            {
                .binding = 0,
                .stride = static_cast<uint32_t>(sizeof(Vertex)),
            });

        pipelineDesc.vertexLayout.attributes.push_back(
            {
                .location = 0,
                .binding = 0,
                .offset = 0,
                .format = VertexFormat::Float3,
            });
        pipelineDesc.vertexLayout.attributes.push_back(
            {
                .location = 1,
                .binding = 0,
                .offset = static_cast<uint32_t>(offsetof(Vertex, color)),
                .format = VertexFormat::Float3,
            });

        m_pipeline = m_renderer.createGraphicsPipeline(pipelineDesc, m_shaderProgram);
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
            Logger::getInstance().warning(
                "Render called before sample resources were initialized");
            return;
        }

        context.renderToBackbuffer(getClearColor(), [this](FrameHandle frame) {
            m_renderer.bindPipeline(frame, m_pipeline);
            m_renderer.bindVertexBuffer(frame, 0, m_vertexBuffer);
            m_renderer.bindIndexBuffer(frame, m_indexBuffer, IndexFormat::UInt16);
            m_renderer.drawIndexed(frame, m_indexCount);
        });
    }

    void BasicTriangleSample::cleanup()
    {
        Logger::getInstance().info("Cleaning up " + std::string(getName()));
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
