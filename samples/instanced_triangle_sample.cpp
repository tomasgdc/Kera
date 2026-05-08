#include "instanced_triangle_sample.h"

#include <cmath>
#include <cstddef>
#include <glm/glm.hpp>
#include <vector>
#include <glm/gtc/matrix_transform.hpp> 

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

        struct Uniforms 
        {
            glm::mat4 projection;
            glm::mat4 view;
        };

        struct BasicInstanceData 
        {
            glm::mat4 modelMatrix;
        };
    }  // namespace

    InstancedTriangleSample::InstancedTriangleSample(IRenderer& renderer)
        : Sample("Instanced Triangle with Slang"),
        m_renderer(renderer),
        m_indexCount(0),
        m_instanceCount(0),
        m_rotationAngle(0.0f)
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
            {{-0.35f, -0.35f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{0.35f, -0.35f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{0.0f, 0.35f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        };

        std::vector<uint16_t> indices = { 0, 1, 2 };
        m_indexCount = static_cast<uint32_t>(indices.size());

        m_vertexBuffer = m_renderer.createBuffer(
            {
                .size = vertices.size() * sizeof(Vertex),
                .usage = BufferUsageKind::Vertex,
                .memoryAccess = MemoryAccess::CpuWrite,
            });

        if (!m_vertexBuffer.isValid() ||
            !m_renderer.uploadBuffer(m_vertexBuffer, vertices.data(),
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

        if (!m_indexBuffer.isValid() ||
            !m_renderer.uploadBuffer(m_indexBuffer, indices.data(),
                indices.size() * sizeof(uint16_t)))
        {
            return false;
        }

        m_instanceCount = 50;
        std::vector<BasicInstanceData> instanceData(m_instanceCount);
        for (uint32_t i = 0; i < m_instanceCount; ++i)
        {
            const float x = (static_cast<float>(i % 10) - 4.5f) * 0.9f;
            const float y = (static_cast<float>(i / 10) - 2.0f) * 0.8f;
            const float z = 0.0f;
            instanceData[i].modelMatrix =
                glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
        }

        m_instanceBuffer = m_renderer.createBuffer(
            {
                .size = m_instanceCount * sizeof(BasicInstanceData),
                .usage = BufferUsageKind::Vertex,
                .memoryAccess = MemoryAccess::CpuWrite,
            });

        if (!m_instanceBuffer.isValid() ||
            !m_renderer.uploadBuffer(m_instanceBuffer, instanceData.data(),
                instanceData.size() * sizeof(BasicInstanceData)))
        {
            return false;
        }

        m_uniformBuffer = m_renderer.createBuffer(
            {
                .size = sizeof(Uniforms),
                .usage = BufferUsageKind::Uniform,
                .memoryAccess = MemoryAccess::CpuWrite,
            });

        return m_uniformBuffer.isValid();
    }

    bool InstancedTriangleSample::createPipeline() 
    {
        GraphicsPipelineDesc pipelineDesc{};
        pipelineDesc.topology = PrimitiveTopologyKind::TriangleList;
        pipelineDesc.cullMode = CullModeKind::None;

        // Binding 0 is the shared triangle mesh.
        pipelineDesc.vertexLayout.bindings.push_back(
            {
                .binding = 0,
                .stride = static_cast<uint32_t>(sizeof(Vertex)),
            });

        // Binding 1 advances once per instance and carries the model matrix.
        pipelineDesc.vertexLayout.bindings.push_back(
            {
                .binding = 1,
                .stride = static_cast<uint32_t>(sizeof(BasicInstanceData)),
                .inputRate = VertexInputRate::Instance,
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

        // A mat4 instance attribute is exposed as four vec4 vertex inputs.
        pipelineDesc.vertexLayout.attributes.push_back(
            {
                .location = 2,
                .binding = 1,
                .offset = 0,
                .format = VertexFormat::Float4,
            });

        pipelineDesc.vertexLayout.attributes.push_back(
            {
                .location = 3,
                .binding = 1,
                .offset = sizeof(glm::vec4),
                .format = VertexFormat::Float4,
            });

        pipelineDesc.vertexLayout.attributes.push_back(
            {
                .location = 4,
                .binding = 1,
                .offset = sizeof(glm::vec4) * 2,
                .format = VertexFormat::Float4,
            });

        pipelineDesc.vertexLayout.attributes.push_back(
            {
                .location = 5,
                .binding = 1,
                .offset = sizeof(glm::vec4) * 3,
                .format = VertexFormat::Float4,
            });

        pipelineDesc.descriptorSets.push_back(
            {
                .set = 0,
                .bindings =
                    {
                        {
                            .binding = 0,
                            .type = DescriptorType::UniformBuffer,
                            .stage = ShaderStage::Vertex,
                        },
                    },
            });

        m_pipeline = m_renderer.createGraphicsPipeline(pipelineDesc, m_shaderProgram);
        if (!m_pipeline.isValid())
        {
            return false;
        }

        m_uniformDescriptorSet = m_renderer.createDescriptorSet(m_pipeline, 0);
        if (!m_uniformDescriptorSet.isValid())
        {
            return false;
        }

        return m_renderer.updateDescriptorSet(m_uniformDescriptorSet, 0, m_uniformBuffer);
    }

    void InstancedTriangleSample::update(float deltaTime) 
    {
        m_rotationAngle += deltaTime * 45.0f;
        if (m_rotationAngle >= 360.0f) 
        {
            m_rotationAngle -= 360.0f;
        }

        const float angleRadians = glm::radians(m_rotationAngle);
        void* mappedData = nullptr;
        if (m_renderer.mapBuffer(m_instanceBuffer, &mappedData))
        {
            BasicInstanceData* data = static_cast<BasicInstanceData*>(mappedData);

            for (uint32_t i = 0; i < m_instanceCount; ++i)
            {
                const float baseX = (static_cast<float>(i % 10) - 4.5f) * 0.9f;
                const float baseY = (static_cast<float>(i / 10) - 2.0f) * 0.8f;
                const float phase = static_cast<float>(i) * 0.31f;
                const float bob = std::sin(angleRadians * 2.0f + phase) * 0.16f;
                const float drift = std::cos(angleRadians * 1.3f + phase) * 0.08f;
                const float spin = angleRadians + phase;
                const float scale =
                    0.78f + 0.08f * std::sin(angleRadians * 1.7f + phase);

                glm::mat4 model = glm::translate(glm::mat4(1.0f),
                    glm::vec3(baseX + drift, baseY + bob, 0.0f));
                model = glm::rotate(model, spin, glm::vec3(0.0f, 0.0f, 1.0f));
                model = glm::scale(model, glm::vec3(scale, scale, 1.0f));
                data[i].modelMatrix = model;
            }

            m_renderer.unmapBuffer(m_instanceBuffer);
        }
        else
        {
            Logger::getInstance().error("Failed to map instance buffer for update.");
        }

        if (m_renderer.mapBuffer(m_uniformBuffer, &mappedData)) 
        {
            kera::Uniforms* uniforms = static_cast<kera::Uniforms*>(mappedData);

            glm::mat4 newView = glm::lookAt(
                glm::vec3(0.0f, 0.0f, -7.0f),
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, 1.0f, 0.0f)
            );
            uniforms->view = newView;

            float timeFactor = std::sin(angleRadians * 0.5f) * 0.05f;
            uniforms->projection = glm::perspective(glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 100.0f) * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f + timeFactor, 1.0f, 1.0f));

            m_renderer.unmapBuffer(m_uniformBuffer);
        }
        else 
        {
            Logger::getInstance().error("Failed to map uniform buffer for update. Uniforms not updated.");
        }
    }

    void InstancedTriangleSample::render(RenderContext& context)
    {
        if (!m_pipeline.isValid() || !m_vertexBuffer.isValid() ||
            !m_indexBuffer.isValid() || !m_instanceBuffer.isValid() ||
            !m_uniformBuffer.isValid() ||
            !m_uniformDescriptorSet.isValid())
        {
            Logger::getInstance().warning(
                "Render called before sample resources were initialized");
            return;
        }

        context.renderToBackbuffer(getClearColor(), [this](FrameHandle frame) {
            m_renderer.bindPipeline(frame, m_pipeline);
            m_renderer.bindVertexBuffer(frame, 0, m_vertexBuffer);
            m_renderer.bindVertexBuffer(frame, 1, m_instanceBuffer);
            m_renderer.bindIndexBuffer(frame, m_indexBuffer, IndexFormat::UInt16);
            m_renderer.bindDescriptorSet(frame, m_pipeline, 0, m_uniformDescriptorSet);
            m_renderer.drawIndexed(frame, m_indexCount, m_instanceCount);
        });
    }

    void InstancedTriangleSample::cleanup()
    {
        Logger::getInstance().info("Cleaning up " + std::string(getName()));
        if (m_uniformDescriptorSet.isValid())
        {
            m_renderer.destroyDescriptorSet(m_uniformDescriptorSet);
            m_uniformDescriptorSet = {};
        }

        if (m_pipeline.isValid()) 
        {
            m_renderer.destroyGraphicsPipeline(m_pipeline);
            m_pipeline = {};
        }

        if (m_uniformBuffer.isValid())
        {
            m_renderer.destroyBuffer(m_uniformBuffer);
            m_uniformBuffer = {};
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
