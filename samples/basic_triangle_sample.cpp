#include "basic_triangle_sample.h"

#include <cstddef>
#include <glm/glm.hpp>
#include <vector>
#include <filesystem>

#include "kera/utilities/file_utils.h"
#include "kera/utilities/logger.h"

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

    std::string resolveShaderPath(const std::string& shaderPath) 
    {
        // If the path exists as-is, return it
        if (kera::FileUtils::fileExists(shaderPath)) 
        {
            return shaderPath;
        }

        // Try relative to current working directory
        const std::filesystem::path cwdPath = std::filesystem::current_path() / shaderPath;
        if (std::filesystem::exists(cwdPath) && std::filesystem::is_regular_file(cwdPath)) 
        {
            return cwdPath.string();
        }

        // Try relative to parent directory (for build artifacts)
        // Assuming executable runs from build/windows-debug or similar
        const std::filesystem::path parentPath = std::filesystem::current_path().parent_path() / shaderPath;
        if (std::filesystem::exists(parentPath) && std::filesystem::is_regular_file(parentPath)) 
        {
            return parentPath.string();
        }

        // Try assuming we're in the build directory and shaders are in build/samples/shaders
        const std::filesystem::path buildPath = std::filesystem::current_path() / "samples" / shaderPath;
        if (std::filesystem::exists(buildPath) && std::filesystem::is_regular_file(buildPath)) 
        {
            return buildPath.string();
        }

        // Return original path if not found (will fail later with better error message)
        return shaderPath;
    }

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
                        .path = resolveShaderPath("shaders/triangle.vert.slang"),
                        .entryPoint = "main",
                        .stage = ShaderStage::Vertex,
                        .source = ShaderSourceKind::SlangFile,
                    },
                    {
                        .path = resolveShaderPath("shaders/triangle.frag.slang"),
                        .entryPoint = "main",
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

    void BasicTriangleSample::render() 
    {
        if (!m_pipeline.isValid() || !m_vertexBuffer.isValid() || !m_indexBuffer.isValid()) 
        {
            Logger::getInstance().warning(
                "Render called before sample resources were initialized");
            return;
        }

        FrameHandle frame = m_renderer.beginFrame();
        if (!frame.isValid()) 
        {
            return;
        }

        m_renderer.beginRenderPass(frame,
            {
                .clearColor = {0.0f, 0.0f, 0.1f, 1.0f},
            });

        m_renderer.bindPipeline(frame, m_pipeline);
        m_renderer.bindVertexBuffer(frame, 0, m_vertexBuffer);
        m_renderer.bindIndexBuffer(frame, m_indexBuffer, IndexFormat::UInt16);
        m_renderer.drawIndexed(frame, m_indexCount);
        m_renderer.endRenderPass(frame);

        if (!m_renderer.endFrame(frame)) 
        {
            Logger::getInstance().error("Failed to end frame");
        }
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
