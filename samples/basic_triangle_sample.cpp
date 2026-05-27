#include "basic_triangle_sample.h"

#include "kera/renderer/reflection_contracts.h"
#include "kera/utilities/logger.h"
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
            constexpr const char* MeshVertexBinding = "meshVertex";
        }  // namespace BasicTriangleShader
    }  // namespace

    BasicTriangleSample::BasicTriangleSample(IRenderer& renderer)
        : Sample("Basic Triangle"), m_renderer(renderer), m_indexCount(0), m_rotationAngle(0.0f)
    {
    }

    void BasicTriangleSample::initialize()
    {
        Logger::getInstance().info("Initializing " + std::string(getName()));

        if (!createShaderProgram())
        {
            Logger::getInstance().error("Failed to create Basic Triangle shader program");
            return;
        }

        if (!createGeometry())
        {
            Logger::getInstance().error("Failed to create Basic Triangle geometry");
            return;
        }

        if (!createPipeline())
        {
            Logger::getInstance().error("Failed to create Basic Triangle pipeline");
            return;
        }

        Logger::getInstance().info("Basic Triangle sample initialized successfully");
    }

    bool BasicTriangleSample::createShaderProgram()
    {
        m_shaderProgram = m_renderer.createGraphicsShaderProgram({
            .path = resolveShaderPath(BasicTriangleShader::Path),
            .vertexEntryPoint = BasicTriangleShader::VertexEntryPoint,
            .fragmentEntryPoint = BasicTriangleShader::FragmentEntryPoint,
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
        const PipelineReflectionContract pipelineContract =
            PipelineReflectionBuilder{}
                .debugName("Basic Triangle Pipeline")
                .vertexEntry(BasicTriangleShader::VertexEntryPoint)
                .vertexBinding<Vertex>(BasicTriangleShader::MeshVertexBinding, 0)
                .semantic("POSITION", BasicTriangleShader::MeshVertexBinding, 0, VertexFormat::Float3)
                .semantic("COLOR", BasicTriangleShader::MeshVertexBinding,
                          static_cast<uint32_t>(offsetof(Vertex, color)), VertexFormat::Float3)
                .build();

        m_pipeline = m_renderer.createGraphicsPipeline({
            .shaderProgram = m_shaderProgram,
            .reflectionContract = pipelineContract,
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
            Logger::getInstance().warning("Render called before Basic Triangle resources were initialized");
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
