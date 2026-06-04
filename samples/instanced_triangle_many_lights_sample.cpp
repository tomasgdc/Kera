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
            glm::mat4 modelMatrix;
        };

        struct GeometryUniforms
        {
            glm::mat4 projection;
            glm::mat4 view;
        };

        struct LightData
        {
            glm::vec4 positionRadius;
            glm::vec4 colorIntensity;
        };

        struct LightingUniforms
        {
            glm::vec4 ambientTimeLightCount;
            std::array<LightData, 64> lights;
        };

        namespace ManyLightsShader
        {
            constexpr const char* Path = "shaders/instanced_triangle_many_lights.slang";
            constexpr const char* GeometryVertexEntryPoint = "geometryVertexMain";
            constexpr const char* GeometryFragmentEntryPoint = "geometryFragmentMain";
            constexpr const char* FullscreenVertexEntryPoint = "fullscreenVertexMain";
            constexpr const char* LightingFragmentEntryPoint = "lightingFragmentMain";

            constexpr const char* GeometryParams = "geometryParams";
            constexpr const char* LightingParams = "lightingParams";
            constexpr const char* SceneTexture = "sceneTexture";
            constexpr const char* SceneSampler = "sceneSampler";
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
        const std::string shaderPath = resolveShaderPath(ManyLightsShader::Path);

        m_geometryShaderProgram = m_renderer.createGraphicsShaderProgram({
            .path = sampleStringView(shaderPath),
            .vertexEntryPoint = stringView(ManyLightsShader::GeometryVertexEntryPoint),
            .fragmentEntryPoint = stringView(ManyLightsShader::GeometryFragmentEntryPoint),
            .source = KERA_SHADER_SOURCE_SLANG_FILE,
            .debugName = {},
        });
        if (!m_geometryShaderProgram.isValid())
        {
            return false;
        }

        m_lightingShaderProgram = m_renderer.createGraphicsShaderProgram({
            .path = sampleStringView(shaderPath),
            .vertexEntryPoint = stringView(ManyLightsShader::FullscreenVertexEntryPoint),
            .fragmentEntryPoint = stringView(ManyLightsShader::LightingFragmentEntryPoint),
            .source = KERA_SHADER_SOURCE_SLANG_FILE,
            .debugName = {},
        });
        return m_lightingShaderProgram.isValid();
    }

    bool InstancedTriangleManyLightsSample::createGeometry()
    {
        const std::vector<Vertex> vertices = {
            {{-0.32f, -0.32f, 0.0f}, {1.0f, 0.1f, 0.0f}},
            {{0.32f, -0.32f, 0.0f}, {0.0f, 1.0f, 0.35f}},
            {{0.0f, 0.32f, 0.0f}, {0.15f, 0.3f, 1.0f}},
        };
        const std::vector<uint16_t> indices = {0, 1, 2};
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

        m_instanceCount = 120;
        std::vector<InstanceData> instances(m_instanceCount);
        m_instanceBuffer = m_renderer.createBuffer({
            .size = instances.size() * sizeof(InstanceData),
            .usage = BufferUsageKind::Vertex,
            .memoryAccess = MemoryAccess::CpuWrite,
        });
        if (!m_instanceBuffer.isValid())
        {
            return false;
        }

        const auto& fullscreenVertices = fullscreenTriangleVertices();
        const auto& fullscreenIndices = fullscreenTriangleIndices();
        m_fullscreenIndexCount = static_cast<uint32_t>(fullscreenIndices.size());

        m_fullscreenVertexBuffer = m_renderer.createBuffer({
            .size = fullscreenVertices.size() * sizeof(FullscreenTriangleVertex),
            .usage = BufferUsageKind::Vertex,
            .memoryAccess = MemoryAccess::CpuWrite,
        });
        if (!m_fullscreenVertexBuffer.isValid() ||
            !m_renderer.uploadBuffer(m_fullscreenVertexBuffer, fullscreenVertices.data(),
                                     fullscreenVertices.size() * sizeof(FullscreenTriangleVertex)))
        {
            return false;
        }

        m_fullscreenIndexBuffer = m_renderer.createBuffer({
            .size = fullscreenIndices.size() * sizeof(uint16_t),
            .usage = BufferUsageKind::Index,
            .memoryAccess = MemoryAccess::CpuWrite,
        });
        if (!m_fullscreenIndexBuffer.isValid() ||
            !m_renderer.uploadBuffer(m_fullscreenIndexBuffer, fullscreenIndices.data(),
                                     fullscreenIndices.size() * sizeof(uint16_t)))
        {
            return false;
        }

        m_geometryUniformBuffer = m_renderer.createBuffer({
            .size = sizeof(GeometryUniforms),
            .usage = BufferUsageKind::Uniform,
            .memoryAccess = MemoryAccess::CpuWrite,
        });
        m_lightingUniformBuffer = m_renderer.createBuffer({
            .size = sizeof(LightingUniforms),
            .usage = BufferUsageKind::Uniform,
            .memoryAccess = MemoryAccess::CpuWrite,
        });

        return m_geometryUniformBuffer.isValid() && m_lightingUniformBuffer.isValid();
    }

    bool InstancedTriangleManyLightsSample::recreateRenderResources(Extent2D extent)
    {
        if (extent.width == 0 || extent.height == 0)
        {
            return false;
        }

        destroyRenderResources();
        m_renderExtent = extent;

        m_sceneTexture = m_renderer.createTexture({
            .width = extent.width,
            .height = extent.height,
            .format = TextureFormat::RGBA8,
            .renderTarget = true,
            .sampled = true,
        });
        if (!m_sceneTexture.isValid())
        {
            return false;
        }

        m_sceneDepthTexture = m_renderer.createTexture({
            .width = extent.width,
            .height = extent.height,
            .format = TextureFormat::Depth32,
            .renderTarget = true,
            .sampled = false,
            .depthStencil = true,
        });
        if (!m_sceneDepthTexture.isValid())
        {
            return false;
        }

        m_sceneRenderTarget = m_renderer.createRenderTarget({
            .colorTexture = m_sceneTexture,
            .depthTexture = m_sceneDepthTexture,
        });
        if (!m_sceneRenderTarget.isValid())
        {
            return false;
        }

        if (!m_sceneSampler.isValid())
        {
            m_sceneSampler = m_renderer.createSampler({});
            if (!m_sceneSampler.isValid())
            {
                return false;
            }
        }

        return createPipelinesAndDescriptors();
    }

    bool InstancedTriangleManyLightsSample::createPipelinesAndDescriptors()
    {
        const VertexInputLayout geometryVertexInput =
            VertexInputLayoutBuilder{}
                .vertexBinding<Vertex>(0)
                .vertexBinding<InstanceData>(1, VertexInputRate::Instance)
                .field(KERA_VERTEX_FIELD(Vertex, position, 0, VertexFormat::Float3))
                .field(KERA_VERTEX_FIELD(Vertex, color, 0, VertexFormat::Float3))
                .field(KERA_VERTEX_FIELD(InstanceData, modelMatrix, 1, VertexFormat::Float4))
                .layout();

        m_geometryPipeline = m_renderer.createGraphicsPipeline({
            .shaderProgram = m_geometryShaderProgram,
            .vertexInput = geometryVertexInput,
            .renderTarget = m_sceneRenderTarget,
            .cullMode = CullModeKind::None,
            .depthTest = true,
            .depthWrite = true,
        });
        if (!m_geometryPipeline.isValid())
        {
            return false;
        }

        m_geometryDescriptorSet = m_renderer.createDescriptorSet(m_geometryPipeline);
        if (!m_geometryDescriptorSet.isValid() ||
            !m_renderer.updateDescriptors(m_geometryDescriptorSet)
                 .uniform<GeometryUniforms>(ManyLightsShader::GeometryParams, m_geometryUniformBuffer)
                 .ok())
        {
            return false;
        }

        const VertexInputLayout lightingVertexInput =
            VertexInputLayoutBuilder{}
                .vertexBinding<FullscreenTriangleVertex>(0)
                .field(KERA_VERTEX_FIELD(FullscreenTriangleVertex, position, 0, VertexFormat::Float2))
                .field(KERA_VERTEX_FIELD(FullscreenTriangleVertex, uv, 0, VertexFormat::Float2))
                .layout();

        m_lightingPipeline = m_renderer.createGraphicsPipeline({
            .shaderProgram = m_lightingShaderProgram,
            .vertexInput = lightingVertexInput,
            .cullMode = CullModeKind::None,
        });
        if (!m_lightingPipeline.isValid())
        {
            return false;
        }

        m_lightingDescriptorSet = m_renderer.createDescriptorSet(m_lightingPipeline);
        if (!m_lightingDescriptorSet.isValid())
        {
            return false;
        }

        return m_renderer.updateDescriptors(m_lightingDescriptorSet)
            .uniform<LightingUniforms>(ManyLightsShader::LightingParams, m_lightingUniformBuffer)
            .sampledImage(ManyLightsShader::SceneTexture, m_sceneTexture)
            .sampler(ManyLightsShader::SceneSampler, m_sceneSampler)
            .ok();
    }

    void InstancedTriangleManyLightsSample::resize(Extent2D extent)
    {
        if (extent == m_renderExtent || extent.width == 0 || extent.height == 0)
        {
            return;
        }

        if (!recreateRenderResources(extent))
        {
            sampleLogError("Failed to resize many-lights render resources.");
            m_initialized = false;
        }
    }

    void InstancedTriangleManyLightsSample::update(float deltaTime)
    {
        if (!m_initialized)
        {
            return;
        }

        m_elapsedTime += deltaTime;
        const float angleRadians = m_elapsedTime;
        updateInstances(angleRadians);
        updateGeometryUniforms();
        updateLightingUniforms(angleRadians);
    }

    void InstancedTriangleManyLightsSample::updateInstances(float angleRadians)
    {
        void* mappedData = nullptr;
        if (!m_renderer.mapBuffer(m_instanceBuffer, &mappedData))
        {
            sampleLogError("Failed to map many-lights instance buffer.");
            return;
        }

        InstanceData* data = static_cast<InstanceData*>(mappedData);
        for (uint32_t i = 0; i < m_instanceCount; ++i)
        {
            const float x = (static_cast<float>(i % 15) - 7.0f) * 0.62f;
            const float y = (static_cast<float>(i / 15) - 3.5f) * 0.62f;
            const float phase = static_cast<float>(i) * 0.37f;
            const float bob = std::sin(angleRadians * 1.8f + phase) * 0.08f;
            const float drift = std::cos(angleRadians * 1.1f + phase) * 0.05f;
            const float spin = angleRadians * (0.55f + 0.02f * static_cast<float>(i % 7)) + phase;
            const float scale = 0.58f + 0.08f * std::sin(angleRadians + phase);

            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x + drift, y + bob, 0.0f));
            model = glm::rotate(model, spin, glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, glm::vec3(scale, scale, 1.0f));
            data[i].modelMatrix = model;
        }

        m_renderer.unmapBuffer(m_instanceBuffer);
    }

    void InstancedTriangleManyLightsSample::updateGeometryUniforms()
    {
        void* mappedData = nullptr;
        if (!m_renderer.mapBuffer(m_geometryUniformBuffer, &mappedData))
        {
            sampleLogError("Failed to map many-lights geometry uniforms.");
            return;
        }

        GeometryUniforms* uniforms = static_cast<GeometryUniforms*>(mappedData);
        const float aspect = m_renderExtent.height == 0
                                 ? 16.0f / 9.0f
                                 : static_cast<float>(m_renderExtent.width) / static_cast<float>(m_renderExtent.height);
        uniforms->view =
            glm::lookAt(glm::vec3(0.0f, 0.0f, -8.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        uniforms->projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

        m_renderer.unmapBuffer(m_geometryUniformBuffer);
    }

    void InstancedTriangleManyLightsSample::updateLightingUniforms(float angleRadians)
    {
        void* mappedData = nullptr;
        if (!m_renderer.mapBuffer(m_lightingUniformBuffer, &mappedData))
        {
            sampleLogError("Failed to map many-lights lighting uniforms.");
            return;
        }

        LightingUniforms* uniforms = static_cast<LightingUniforms*>(mappedData);
        uniforms->ambientTimeLightCount = glm::vec4(0.08f, angleRadians, static_cast<float>(kLightCount), 0.0f);

        for (uint32_t i = 0; i < kLightCount; ++i)
        {
            const float ring = static_cast<float>(i % 8);
            const float row = static_cast<float>(i / 8);
            const float phase = static_cast<float>(i) * 0.41f;
            const float radius = 0.13f + 0.035f * std::sin(angleRadians + phase);
            const float centerX = 0.12f + ring * 0.11f;
            const float centerY = 0.15f + row * 0.10f;
            const float x = centerX + 0.055f * std::cos(angleRadians * 0.9f + phase);
            const float y = centerY + 0.045f * std::sin(angleRadians * 1.2f + phase);

            const glm::vec3 color(0.55f + 0.45f * std::sin(phase + 0.0f), 0.55f + 0.45f * std::sin(phase + 2.1f),
                                  0.55f + 0.45f * std::sin(phase + 4.2f));
            uniforms->lights[i].positionRadius = glm::vec4(x, y, radius, 0.0f);
            uniforms->lights[i].colorIntensity = glm::vec4(color, 0.75f + 0.2f * std::sin(angleRadians + phase));
        }

        m_renderer.unmapBuffer(m_lightingUniformBuffer);
    }

    void InstancedTriangleManyLightsSample::render(RenderContext& context)
    {
        if (!m_initialized)
        {
            return;
        }

        if (!m_geometryPipeline.isValid() || !m_lightingPipeline.isValid() || !m_sceneRenderTarget.isValid() ||
            !m_geometryDescriptorSet.isValid() || !m_lightingDescriptorSet.isValid())
        {
            sampleLogWarning("Render called before many-lights resources were initialized");
            return;
        }

        context.renderToTexture(m_sceneRenderTarget, {0.0f, 0.0f, 0.0f, 1.0f},
                                [this](FrameHandle frame)
                                {
                                    m_renderer.bindPipeline(frame, m_geometryPipeline);
                                    m_renderer.bindVertexBuffer(frame, 0, m_vertexBuffer);
                                    m_renderer.bindVertexBuffer(frame, 1, m_instanceBuffer);
                                    m_renderer.bindIndexBuffer(frame, m_indexBuffer, IndexFormat::UInt16);
                                    m_renderer.bindDescriptorSet(frame, m_geometryPipeline, m_geometryDescriptorSet);
                                    m_renderer.drawIndexed(frame, m_indexCount, m_instanceCount);
                                });

        context.renderToBackbuffer(getClearColor(),
                                   [this](FrameHandle frame)
                                   {
                                       m_renderer.bindPipeline(frame, m_lightingPipeline);
                                       m_renderer.bindVertexBuffer(frame, 0, m_fullscreenVertexBuffer);
                                       m_renderer.bindIndexBuffer(frame, m_fullscreenIndexBuffer, IndexFormat::UInt16);
                                       m_renderer.bindDescriptorSet(frame, m_lightingPipeline, m_lightingDescriptorSet);
                                       m_renderer.drawIndexed(frame, m_fullscreenIndexCount);
                                   });
    }

    void InstancedTriangleManyLightsSample::destroyRenderResources()
    {
        if (m_lightingDescriptorSet.isValid())
        {
            m_renderer.destroyDescriptorSet(m_lightingDescriptorSet);
            m_lightingDescriptorSet = {};
        }
        if (m_geometryDescriptorSet.isValid())
        {
            m_renderer.destroyDescriptorSet(m_geometryDescriptorSet);
            m_geometryDescriptorSet = {};
        }
        if (m_lightingPipeline.isValid())
        {
            m_renderer.destroyGraphicsPipeline(m_lightingPipeline);
            m_lightingPipeline = {};
        }
        if (m_geometryPipeline.isValid())
        {
            m_renderer.destroyGraphicsPipeline(m_geometryPipeline);
            m_geometryPipeline = {};
        }
        if (m_sceneRenderTarget.isValid())
        {
            m_renderer.destroyRenderTarget(m_sceneRenderTarget);
            m_sceneRenderTarget = {};
        }
        if (m_sceneTexture.isValid())
        {
            m_renderer.destroyTexture(m_sceneTexture);
            m_sceneTexture = {};
        }
        if (m_sceneDepthTexture.isValid())
        {
            m_renderer.destroyTexture(m_sceneDepthTexture);
            m_sceneDepthTexture = {};
        }
    }

    void InstancedTriangleManyLightsSample::cleanup()
    {
        sampleLogInfo("Cleaning up " + std::string(getName()));
        m_initialized = false;

        destroyRenderResources();

        if (m_sceneSampler.isValid())
        {
            m_renderer.destroySampler(m_sceneSampler);
            m_sceneSampler = {};
        }
        if (m_lightingUniformBuffer.isValid())
        {
            m_renderer.destroyBuffer(m_lightingUniformBuffer);
            m_lightingUniformBuffer = {};
        }
        if (m_geometryUniformBuffer.isValid())
        {
            m_renderer.destroyBuffer(m_geometryUniformBuffer);
            m_geometryUniformBuffer = {};
        }
        if (m_fullscreenIndexBuffer.isValid())
        {
            m_renderer.destroyBuffer(m_fullscreenIndexBuffer);
            m_fullscreenIndexBuffer = {};
        }
        if (m_fullscreenVertexBuffer.isValid())
        {
            m_renderer.destroyBuffer(m_fullscreenVertexBuffer);
            m_fullscreenVertexBuffer = {};
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
        if (m_lightingShaderProgram.isValid())
        {
            m_renderer.destroyShaderProgram(m_lightingShaderProgram);
            m_lightingShaderProgram = {};
        }
        if (m_geometryShaderProgram.isValid())
        {
            m_renderer.destroyShaderProgram(m_geometryShaderProgram);
            m_geometryShaderProgram = {};
        }
    }

}  // namespace kera
