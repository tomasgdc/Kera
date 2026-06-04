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
            glm::mat4 modelMatrix;
        };

        namespace InstancedTriangleShader
        {
            constexpr const char* Path = "shaders/instanced_triangle.slang";
            constexpr const char* VertexEntryPoint = "vertexMain";
            constexpr const char* FragmentEntryPoint = "fragmentMain";
            constexpr const char* GlobalParams = "globalParams";
        }  // namespace InstancedTriangleShader

        constexpr uint32_t kUniformRingSlots = 3;
    }  // namespace

    InstancedTriangleSample::InstancedTriangleSample(Renderer& renderer)
        : Sample("Instanced Triangle"), m_renderer(renderer), m_indexCount(0), m_instanceCount(0), m_rotationAngle(0.0f)
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
        const std::string shaderPath = resolveShaderPath(InstancedTriangleShader::Path);
        m_shaderProgram = m_renderer.createGraphicsShaderProgram({
            .path = sampleStringView(shaderPath),
            .vertexEntryPoint = stringView(InstancedTriangleShader::VertexEntryPoint),
            .fragmentEntryPoint = stringView(InstancedTriangleShader::FragmentEntryPoint),
            .source = KERA_SHADER_SOURCE_SLANG_FILE,
            .debugName = {},
        });
        return static_cast<bool>(m_shaderProgram.isValid());
    }

    bool InstancedTriangleSample::createGeometry()
    {
        std::vector<Vertex> vertices = {
            {{-0.35f, -0.35f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{0.35f, -0.35f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{0.0f, 0.35f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        };

        std::vector<uint16_t> indices = {0, 1, 2};
        m_indexCount = static_cast<uint32_t>(indices.size());

        m_vertexBuffer = m_renderer.createBuffer({
            .size = vertices.size() * sizeof(Vertex),
            .usage = BufferUsageKind::Vertex,
            .memoryAccess = MemoryAccess::CpuWrite,
        });

        if (!m_vertexBuffer.isValid() ||
            !m_renderer.uploadBuffer(m_vertexBuffer, vertices.data(), vertices.size() * sizeof(Vertex)))
        {
            return false;
        }

        m_indexBuffer = m_renderer.createBuffer({
            .size = indices.size() * sizeof(uint16_t),
            .usage = BufferUsageKind::Index,
            .memoryAccess = MemoryAccess::CpuWrite,
        });

        if (!m_indexBuffer.isValid() ||
            !m_renderer.uploadBuffer(m_indexBuffer, indices.data(), indices.size() * sizeof(uint16_t)))
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
            instanceData[i].modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
        }

        m_instanceBuffer = m_renderer.createBuffer({
            .size = m_instanceCount * sizeof(BasicInstanceData),
            .usage = BufferUsageKind::Vertex,
            .memoryAccess = MemoryAccess::CpuWrite,
        });

        if (!m_instanceBuffer.isValid() || !m_renderer.uploadBuffer(m_instanceBuffer, instanceData.data(),
                                                                    instanceData.size() * sizeof(BasicInstanceData)))
        {
            return false;
        }

        m_uniformBuffer = m_renderer.createUniformRingBuffer(sizeof(Uniforms), kUniformRingSlots);

        return m_uniformBuffer.isValid();
    }

    bool InstancedTriangleSample::createPipeline()
    {
        const VertexInputLayout vertexInput =
            VertexInputLayoutBuilder{}
                .vertexBinding<Vertex>(0)
                .vertexBinding<BasicInstanceData>(1, VertexInputRate::Instance)
                .field(KERA_VERTEX_FIELD(Vertex, position, 0, VertexFormat::Float3))
                .field(KERA_VERTEX_FIELD(Vertex, color, 0, VertexFormat::Float3))
                .field(KERA_VERTEX_FIELD(BasicInstanceData, modelMatrix, 1, VertexFormat::Float4))
                .layout();

        m_pipeline = m_renderer.createGraphicsPipeline({
            .shaderProgram = m_shaderProgram,
            .vertexInput = vertexInput,
            .topology = PrimitiveTopologyKind::TriangleList,
            .cullMode = CullModeKind::None,
        });
        if (!m_pipeline.isValid())
        {
            return false;
        }

        m_uniformDescriptorSets.clear();
        m_uniformDescriptorSets.reserve(kUniformRingSlots);
        for (uint32_t index = 0; index < kUniformRingSlots; ++index)
        {
            DescriptorSetHandle descriptorSet = m_renderer.createDescriptorSet(m_pipeline);
            if (!descriptorSet.isValid())
            {
                return false;
            }

            const std::size_t uniformOffset = sizeof(Uniforms) * index;
            if (!m_renderer.updateDescriptors(descriptorSet)
                     .uniform<Uniforms>(InstancedTriangleShader::GlobalParams, m_uniformBuffer, uniformOffset)
                     .ok())
            {
                return false;
            }
            m_uniformDescriptorSets.push_back(descriptorSet);
        }

        return true;
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
                const float scale = 0.78f + 0.08f * std::sin(angleRadians * 1.7f + phase);

                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(baseX + drift, baseY + bob, 0.0f));
                model = glm::rotate(model, spin, glm::vec3(0.0f, 0.0f, 1.0f));
                model = glm::scale(model, glm::vec3(scale, scale, 1.0f));
                data[i].modelMatrix = model;
            }

            m_renderer.unmapBuffer(m_instanceBuffer);
        }
        else
        {
            sampleLogError("Failed to map instance buffer for update.");
        }
    }

    void InstancedTriangleSample::render(RenderContext& context)
    {
        if (!m_pipeline.isValid() || !m_vertexBuffer.isValid() || !m_indexBuffer.isValid() ||
            !m_instanceBuffer.isValid() || !m_uniformBuffer.isValid() || m_uniformDescriptorSets.empty())
        {
            sampleLogWarning("Render called before Instanced Triangle resources were initialized");
            return;
        }

        context.renderToBackbuffer(
            getClearColor(),
            [this](FrameHandle frame)
            {
                const float angleRadians = glm::radians(m_rotationAngle);
                const float timeFactor = std::sin(angleRadians * 0.5f) * 0.05f;
                Uniforms uniforms{};
                uniforms.view =
                    glm::lookAt(glm::vec3(0.0f, 0.0f, -7.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                uniforms.projection = glm::perspective(glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 100.0f) *
                                      glm::scale(glm::mat4(1.0f), glm::vec3(1.0f + timeFactor, 1.0f, 1.0f));
                if (!m_renderer.uploadUniformRingBuffer(m_uniformBuffer, frame, &uniforms, sizeof(Uniforms)))
                {
                    sampleLogError("Failed to upload instanced triangle uniforms.");
                    return;
                }

                const std::size_t uniformOffset = m_renderer.getUniformRingBufferOffset(m_uniformBuffer, frame);
                const std::size_t descriptorIndex = (uniformOffset / sizeof(Uniforms)) % m_uniformDescriptorSets.size();
                DescriptorSetHandle uniformDescriptorSet = m_uniformDescriptorSets[descriptorIndex];
                if (!m_renderer.updateDescriptors(uniformDescriptorSet)
                         .uniform<Uniforms>(InstancedTriangleShader::GlobalParams, m_uniformBuffer, uniformOffset)
                         .ok())
                {
                    sampleLogError("Failed to update instanced triangle uniform descriptor.");
                    return;
                }

                m_renderer.bindPipeline(frame, m_pipeline);
                m_renderer.bindVertexBuffer(frame, 0, m_vertexBuffer);
                m_renderer.bindVertexBuffer(frame, 1, m_instanceBuffer);
                m_renderer.bindIndexBuffer(frame, m_indexBuffer, IndexFormat::UInt16);
                m_renderer.bindDescriptorSet(frame, m_pipeline, uniformDescriptorSet);
                m_renderer.drawIndexed(frame, m_indexCount, m_instanceCount);
            });
    }

    void InstancedTriangleSample::cleanup()
    {
        sampleLogInfo("Cleaning up " + std::string(getName()));
        for (DescriptorSetHandle descriptorSet : m_uniformDescriptorSets)
        {
            if (descriptorSet.isValid())
            {
                m_renderer.destroyDescriptorSet(descriptorSet);
            }
        }
        m_uniformDescriptorSets.clear();

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
