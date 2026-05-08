#include "instanced_triangle.h"

#include <cstddef>
#include <glm/glm.hpp>
#include <vector>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp> 

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

        struct Uniforms 
        {
            glm::mat4 projection;
            glm::mat4 view;
        };

        struct BasicInstanceData 
        {
            glm::mat4 modelMatrix;
        };

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
    }  // namespace

    InstancedTriangleSample::InstancedTriangleSample(IRenderer& renderer)
        : Sample("Instanced Triangle with Slang"),
        m_renderer(renderer),
        m_indexCount(0),
        m_rotationAngle(0.0f),
        m_instanceCount(0)
    {
    }

    void InstancedTriangleSample::initialize() 
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

    bool InstancedTriangleSample::createShaderProgram() 
    {
        ShaderProgramDesc programDesc
        {
            .stages =
                {
                    {
                        .path = resolveShaderPath("shaders/instanced_triangle.slang"),
                        .entryPoint = "vertexMain",
                        .stage = ShaderStage::Vertex,
                        .source = ShaderSourceKind::SlangFile,
                    },
                    {
                        .path = resolveShaderPath("shaders/instanced_triangle.slang"),
                        .entryPoint = "fragmentMain",
                        .stage = ShaderStage::Fragment,
                        .source = ShaderSourceKind::SlangFile,
                    },
                },
        };

        m_shaderProgram = m_renderer.createShaderProgram(programDesc);
        return static_cast<bool>(m_shaderProgram.isValid());
    }

    bool InstancedTriangleSample::createGeometry() 
    {
        std::vector<Vertex> vertices = 
        {
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{0.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        };

        std::vector<uint16_t> indices = { 0, 1, 2 };
        m_indexCount = static_cast<uint32_t>(indices.size());

        // Mesh data (static)
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

        if (!m_renderer.uploadBuffer(m_indexBuffer, indices.data(),
            indices.size() * sizeof(uint16_t)))
        {
            return false;
        }

        // 2. Instance Data (Dynamic)
        m_instanceCount = 50; // Define how many instances we want to draw
        std::vector<BasicInstanceData> instanceData(m_instanceCount);

        // Populate instance data
        for (size_t i = 0; i < m_instanceCount; ++i) 
        {
            // Create a unique translation matrix for each instance
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(
                (float)(i % 10) * 1.0f - 5.0f,
                0.0f,
                (float)(i / 10) * 1.0f
            ));

            instanceData[i].modelMatrix = model;
        }

        m_instanceBuffer = m_renderer.createBuffer(
            {
                .size = m_instanceCount * sizeof(BasicInstanceData),
                .usage = BufferUsageKind::Vertex,
                .memoryAccess = MemoryAccess::CpuWrite, //Host visible
            });

        if (!m_instanceBuffer.isValid())
        {
            return false;
        }

        if (!m_renderer.uploadBuffer(m_instanceBuffer, instanceData.data(),
            instanceData.size() * sizeof(BasicInstanceData)))
        {
            return false;
        }

        // We need a buffer to hold the projection and view matrices.
        m_uniformBuffer = m_renderer.createBuffer(
            {
                .size = sizeof(Uniforms), // Size of one Uniforms struct
                .usage = BufferUsageKind::Uniform,
                .memoryAccess = MemoryAccess::CpuWrite, // Must be host visible
            });

        if (!m_uniformBuffer.isValid()) 
        {
            return false;
        }

        return true;
    }

    bool InstancedTriangleSample::createPipeline() 
    {
        GraphicsPipelineDesc pipelineDesc{};
        pipelineDesc.topology = PrimitiveTopologyKind::TriangleList;

        // ====================================================================
        // 1. VERTEX BINDING DESCRIPTIONS (Defines the buffers)
        // These define *which* buffer (m_vertexBuffer or m_instanceBuffer)
        // corresponds to which binding number (0 or 1).
        // ====================================================================

        // Binding 0: Mesh Vertex Buffer
        pipelineDesc.vertexLayout.bindings.push_back(
            {
                .binding = 0,
                .stride = static_cast<uint32_t>(sizeof(Vertex)),
            });

        // Binding 1: Instance Data Buffer
        pipelineDesc.vertexLayout.bindings.push_back(
            {
                .binding = 1,
                .stride = static_cast<uint32_t>(sizeof(BasicInstanceData)),
            });


        // ====================================================================
        // 2. VERTEX ATTRIBUTE DESCRIPTIONS (Defines the fields within the buffers)
        // These define the *location* (0, 1, 2...) within the bound buffer.
        // ====================================================================

        // Attribute 0: Position (Mesh Vertex Data)
        pipelineDesc.vertexLayout.attributes.push_back(
            {
                .location = 0, // Unique Location
                .binding = 0,
                .offset = 0,
                .format = VertexFormat::Float3,
            });

        // Attribute 1: Color (Mesh Vertex Data)
        pipelineDesc.vertexLayout.attributes.push_back(
            {
                .location = 1, // Unique Location
                .binding = 0,
                .offset = static_cast<uint32_t>(offsetof(Vertex, color)),
                .format = VertexFormat::Float3,
            });

        // Attribute 2: Model Matrix Component 1 (Instance Data)
        pipelineDesc.vertexLayout.attributes.push_back(
            {
                .location = 2, // Unique Location
                .binding = 1,
                .offset = 0,
                .format = VertexFormat::Float4,
            });

        // Attribute 3: Model Matrix Component 2 (Instance Data)
        pipelineDesc.vertexLayout.attributes.push_back(
            {
                .location = 3, // Unique Location
                .binding = 1,
                .offset = sizeof(glm::vec4), // *** Double-check this offset! ***
                .format = VertexFormat::Float4,
            });

        // Attribute 4: Model Matrix Component 3 (Instance Data)
        pipelineDesc.vertexLayout.attributes.push_back(
            {
                .location = 4, // Unique Location
                .binding = 1,
                .offset = sizeof(glm::vec4) * 2, // *** Double-check this offset! ***
                .format = VertexFormat::Float4,
            });

        // Attribute 5: Model Matrix Component 4 (Instance Data)
        pipelineDesc.vertexLayout.attributes.push_back(
            {
                .location = 5, // Unique Location
                .binding = 1,
                .offset = sizeof(glm::vec4) * 3, // *** Double-check this offset! ***
                .format = VertexFormat::Float4,
            });

        // Attribute 6: Uniform Data (Projection/View) (from Uniforms at Binding 0)
        // NOTE: I've changed this to Location 6 to avoid conflict with the matrix components (2-5).
        pipelineDesc.vertexLayout.attributes.push_back(
            {
                .location = 6, // Changed location to 6 to be unique
                .binding = 0,
                .offset = 0,
                .format = VertexFormat::Float4,
            });

        m_pipeline = m_renderer.createGraphicsPipeline(pipelineDesc, m_shaderProgram);
        return static_cast<bool>(m_pipeline.isValid());
    }

    void InstancedTriangleSample::update(float deltaTime) 
    {
        m_rotationAngle += deltaTime * 45.0f;
        if (m_rotationAngle >= 360.0f) 
        {
            m_rotationAngle -= 360.0f;
        }

        void* mappedData = nullptr;
        if (m_renderer.mapBuffer(m_instanceBuffer , &mappedData))
        {
            BasicInstanceData* data = static_cast<BasicInstanceData*>(mappedData);

            // Iterate over all instances and update their model matrices
            for (uint32_t i = 0; i < m_instanceCount; ++i) 
            {
                // Example: Create a new translation matrix based on index
                glm::mat4 newModel = glm::translate(glm::mat4(1.0f), glm::vec3(
                    (float)(i % 10) * 1.0f - 5.0f,
                    0.0f,
                    (float)(i / 10) * 1.0f
                ));
                data[i].modelMatrix = newModel;
            }

			m_renderer.unmapBuffer(m_instanceBuffer);
        }
        else 
        {
            Logger::getInstance().error("Failed to map instance buffer for update.");
        }

        // Map the Uniform Buffer Memory
        if (m_renderer.mapBuffer(m_uniformBuffer, &mappedData)) 
        {
            kera::Uniforms* uniforms = static_cast<kera::Uniforms*>(mappedData);

            // Example: Slowly change the view matrix (e.g., simulating camera drift)
            glm::mat4 newView = glm::lookAt(
                glm::vec3(0.0f, 0.0f, -5.0f), // New camera position
                glm::vec3(0.0f, 0.0f, 0.0f),  // Target
                glm::vec3(0.0f, 1.0f, 0.0f)   // Up vector
            );
            uniforms->view = newView;

            float timeFactor = (float)std::sin(deltaTime * 0.5f) * 0.1f;
            uniforms->projection = glm::perspective(glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 100.0f) * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f + timeFactor, 1.0f, 1.0f));

            m_renderer.unmapBuffer(m_uniformBuffer);
        }
        else 
        {
            Logger::getInstance().error("Failed to map uniform buffer for update. Uniforms not updated.");
        }
    }

    void InstancedTriangleSample::render() 
    {
        if (!m_pipeline.isValid() || !m_instanceBuffer.isValid() || !m_vertexBuffer.isValid() || 
            !m_indexBuffer.isValid() || !m_uniformBuffer.isValid())
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
        m_renderer.bindVertexBuffer(frame, 1, m_instanceBuffer);
        m_renderer.bindVertexBuffer(frame, 2, m_uniformBuffer);
        m_renderer.bindIndexBuffer(frame, m_indexBuffer, IndexFormat::UInt16);
        m_renderer.drawIndexed(frame, m_indexCount, m_instanceCount);
        m_renderer.endRenderPass(frame);

        if (!m_renderer.endFrame(frame)) 
        {
            Logger::getInstance().error("Failed to end frame");
        }
    }

    void InstancedTriangleSample::cleanup()
    {
        Logger::getInstance().info("Cleaning up " + std::string(getName()));
        if (m_pipeline.isValid()) 
        {
            m_renderer.destroyGraphicsPipeline(m_pipeline);
            m_pipeline = {};
        }

        if (m_instanceBuffer.isValid())
        {
            m_renderer.destroyBuffer(m_instanceBuffer);
            m_instanceBuffer = {};
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
