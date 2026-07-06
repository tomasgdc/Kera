// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "damaged_helmet_sample.h"

#include "render_context.h"
#include "sample_utils.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstddef>
#include <string>

namespace kera
{
    namespace
    {
        struct HelmetUniforms
        {
            glm::mat4 model;
            glm::mat4 view;
            glm::mat4 projection;
            glm::vec4 cameraPosition;
            glm::vec4 lightDirectionAmbient;
            glm::vec4 baseColorFactor;
            glm::vec4 emissiveFactorNormalScale;
            glm::vec4 metallicRoughnessOcclusion;
            glm::vec4 alphaModeCutoffReflectionExposure;
            glm::vec4 debugViewGamma;
            glm::vec4 padding2;
        };

        namespace DamagedHelmetShader
        {
            constexpr const char* Path = "shaders/damaged_helmet.slang";
            constexpr const char* MeshVertexEntryPoint = "helmetVertexMain";
            constexpr const char* MeshFragmentEntryPoint = "helmetFragmentMain";
            constexpr const char* FullscreenVertexEntryPoint = "fullscreenVertexMain";
            constexpr const char* FullscreenFragmentEntryPoint = "fullscreenFragmentMain";
            constexpr const char* HelmetParams = "helmetParams";
            constexpr const char* BaseColorTexture = "baseColorTexture";
            constexpr const char* MetalRoughnessTexture = "metalRoughnessTexture";
            constexpr const char* EmissiveTexture = "emissiveTexture";
            constexpr const char* OcclusionTexture = "occlusionTexture";
            constexpr const char* NormalTexture = "normalTexture";
            constexpr const char* SceneTexture = "sceneTexture";
            constexpr const char* MaterialSampler = "materialSampler";
        }  // namespace DamagedHelmetShader

        constexpr uint32_t kUniformRingSlots = 3;
        constexpr uint32_t kMaxDamagedHelmetDebugView = 11;

        float toShaderAlphaMode(KeraGltfAlphaMode mode)
        {
            switch (mode)
            {
                case KERA_GLTF_ALPHA_MASK:
                    return 1.0f;
                case KERA_GLTF_ALPHA_BLEND:
                    return 2.0f;
                case KERA_GLTF_ALPHA_OPAQUE:
                default:
                    return 0.0f;
            }
        }
    }  // namespace

    DamagedHelmetSample::DamagedHelmetSample(Renderer& renderer)
        : Sample("Damaged Helmet Texture Loading"), m_renderer(renderer)
    {
    }

    DamagedHelmetSample::DamagedHelmetSample(Renderer& renderer, uint32_t debugView)
        : Sample("Damaged Helmet Texture Loading")
        , m_renderer(renderer)
        , m_debugView(debugView <= kMaxDamagedHelmetDebugView ? debugView : 0)
    {
    }

    DamagedHelmetSample::DamagedHelmetSample(Renderer& renderer, uint32_t debugView, bool fixedYaw, float yawRadians)
        : Sample("Damaged Helmet Texture Loading")
        , m_renderer(renderer)
        , m_initialYawRadians(fixedYaw ? yawRadians : 2.2f)
        , m_debugView(debugView <= kMaxDamagedHelmetDebugView ? debugView : 0)
        , m_fixedYaw(fixedYaw)
    {
    }

    void DamagedHelmetSample::initialize()
    {
        sampleLogInfo("Initializing " + std::string(getName()));
        m_initialized = false;

        if (!createShaderPrograms())
        {
            sampleLogError("Failed to create DamagedHelmet shader programs.");
            cleanup();
            return;
        }

        if (!loadGltfModelResources())
        {
            sampleLogError("Failed to load DamagedHelmet glTF model resources.");
            cleanup();
            return;
        }

        if (!createFullscreenGeometry())
        {
            sampleLogError("Failed to create DamagedHelmet fullscreen geometry.");
            cleanup();
            return;
        }

        if (!recreateRenderResources(m_renderer.getDrawableExtent()))
        {
            sampleLogError("Failed to create DamagedHelmet render resources.");
            cleanup();
            return;
        }

        m_initialized = true;
        sampleLogInfo("DamagedHelmet sample initialized successfully.");
    }

    bool DamagedHelmetSample::createShaderPrograms()
    {
        const std::string shaderPath = resolveShaderPath(DamagedHelmetShader::Path);
        m_meshShaderProgram = m_renderer.createGraphicsShaderProgram({
            .path = sampleStringView(shaderPath),
            .vertexEntryPoint = stringView(DamagedHelmetShader::MeshVertexEntryPoint),
            .fragmentEntryPoint = stringView(DamagedHelmetShader::MeshFragmentEntryPoint),
            .source = KERA_SHADER_SOURCE_SLANG_FILE,
            .debugName = {},
        });

        if (!m_meshShaderProgram.isValid())
        {
            return false;
        }

        m_displayShaderProgram = m_renderer.createGraphicsShaderProgram({
            .path = sampleStringView(shaderPath),
            .vertexEntryPoint = stringView(DamagedHelmetShader::FullscreenVertexEntryPoint),
            .fragmentEntryPoint = stringView(DamagedHelmetShader::FullscreenFragmentEntryPoint),
            .source = KERA_SHADER_SOURCE_SLANG_FILE,
            .debugName = {},
        });

        return m_displayShaderProgram.isValid();
    }

    bool DamagedHelmetSample::loadGltfModelResources()
    {
        const std::string assetPath = resolveSampleAssetPath("assets/gltf/DamagedHelmet/DamagedHelmet.gltf");
        if (!m_renderer.loadGltfModel(
                {
                    .path = sampleStringView(assetPath),
                    .debugName = stringView("DamagedHelmet"),
                    .requireMaterialTextures = 1,
                },
                m_model))
        {
            sampleLogError("DamagedHelmet glTF load failed.");
            return false;
        }

        m_uniformBuffer = m_renderer.createUniformRingBuffer(sizeof(HelmetUniforms), kUniformRingSlots);
        return m_uniformBuffer.isValid();
    }

    bool DamagedHelmetSample::createFullscreenGeometry()
    {
        const auto& vertices = fullscreenTriangleVertices();
        const auto& indices = fullscreenTriangleIndices();
        m_fullscreenIndexCount = static_cast<uint32_t>(indices.size());

        m_fullscreenVertexBuffer = m_renderer.createBuffer({
            .size = vertices.size() * sizeof(FullscreenTriangleVertex),
            .usage = BufferUsageKind::Vertex,
            .memoryAccess = MemoryAccess::CpuWrite,
            .debugName = stringView("DamagedHelmet Fullscreen Vertex Buffer"),
        });

        m_fullscreenIndexBuffer = m_renderer.createBuffer({
            .size = indices.size() * sizeof(uint16_t),
            .usage = BufferUsageKind::Index,
            .memoryAccess = MemoryAccess::CpuWrite,
            .debugName = stringView("DamagedHelmet Fullscreen Index Buffer"),
        });

        return m_fullscreenVertexBuffer.isValid() && m_fullscreenIndexBuffer.isValid() &&
               m_renderer.uploadBuffer(m_fullscreenVertexBuffer, vertices.data(),
                                       vertices.size() * sizeof(FullscreenTriangleVertex)) &&
               m_renderer.uploadBuffer(m_fullscreenIndexBuffer, indices.data(), indices.size() * sizeof(uint16_t));
    }

    bool DamagedHelmetSample::recreateRenderResources(Extent2D extent)
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
            .debugName = stringView("DamagedHelmet Scene Texture"),
        });

        m_sceneDepthTexture = m_renderer.createTexture({
            .width = extent.width,
            .height = extent.height,
            .format = TextureFormat::Depth32,
            .renderTarget = true,
            .sampled = false,
            .depthStencil = true,
            .debugName = stringView("DamagedHelmet Depth Texture"),
        });

        if (!m_sceneTexture.isValid() || !m_sceneDepthTexture.isValid())
        {
            return false;
        }

        m_sceneRenderTarget = m_renderer.createRenderTarget({
            .colorTexture = m_sceneTexture,
            .depthTexture = m_sceneDepthTexture,
            .debugName = stringView("DamagedHelmet Render Target"),
        });

        return m_sceneRenderTarget.isValid() && createPipelinesAndDescriptors();
    }

    bool DamagedHelmetSample::createPipelinesAndDescriptors()
    {
        const VertexInputLayout meshVertexInput =
            VertexInputLayoutBuilder{}
                .vertexBinding<GltfVertex>(0)
                .field(KERA_VERTEX_FIELD(GltfVertex, position, 0, VertexFormat::Float3))
                .field(KERA_VERTEX_FIELD(GltfVertex, normal, 0, VertexFormat::Float3))
                .field(KERA_VERTEX_FIELD(GltfVertex, uv, 0, VertexFormat::Float2))
                .field(KERA_VERTEX_FIELD(GltfVertex, tangent, 0, VertexFormat::Float4))
                .layout();

        m_meshPipeline = m_renderer.createGraphicsPipeline({
            .shaderProgram = m_meshShaderProgram,
            .vertexInput = meshVertexInput,
            .renderTarget = m_sceneRenderTarget,
            .cullMode = m_model.materialFactors.doubleSided ? CullModeKind::None : CullModeKind::Back,
            .frontFace = FrontFaceKind::Clockwise,
            .blendMode = m_model.materialFactors.alphaMode == KERA_GLTF_ALPHA_BLEND ? BlendModeKind::Alpha
                                                                                    : BlendModeKind::Opaque,
            .depthTest = true,
            .depthWrite = m_model.materialFactors.alphaMode != KERA_GLTF_ALPHA_BLEND,
            .debugName = stringView("DamagedHelmet Mesh Pipeline"),
        });

        if (!m_meshPipeline.isValid())
        {
            return false;
        }

        KeraUniformRingBufferInfo ringInfo = m_renderer.getUniformRingBufferInfo(m_uniformBuffer);

        m_meshDescriptorSets.clear();
        m_meshDescriptorSets.reserve(ringInfo.slotCount);

        for (uint32_t slot = 0; slot < ringInfo.slotCount; ++slot)
        {
            DescriptorSetHandle descriptorSet = m_renderer.createDescriptorSet(m_meshPipeline);
            if (!descriptorSet.isValid())
            {
                return false;
            }

            const std::size_t uniformOffset = m_renderer.getUniformRingBufferSlotOffset(m_uniformBuffer, slot);
            if (!m_renderer.updateDescriptors(descriptorSet)
                     .uniform<HelmetUniforms>(DamagedHelmetShader::HelmetParams, m_uniformBuffer, uniformOffset)
                     .sampledImage(DamagedHelmetShader::BaseColorTexture, m_model.materialTextures.baseColor)
                     .sampledImage(DamagedHelmetShader::MetalRoughnessTexture, m_model.materialTextures.metalRoughness)
                     .sampledImage(DamagedHelmetShader::EmissiveTexture, m_model.materialTextures.emissive)
                     .sampledImage(DamagedHelmetShader::OcclusionTexture, m_model.materialTextures.occlusion)
                     .sampledImage(DamagedHelmetShader::NormalTexture, m_model.materialTextures.normal)
                     .sampler(DamagedHelmetShader::MaterialSampler, m_model.materialSampler)
                     .ok())
            {
                return false;
            }
            m_meshDescriptorSets.push_back(descriptorSet);
        }

        const VertexInputLayout displayVertexInput =
            VertexInputLayoutBuilder{}
                .vertexBinding<FullscreenTriangleVertex>(0)
                .field(KERA_VERTEX_FIELD(FullscreenTriangleVertex, position, 0, VertexFormat::Float2))
                .field(KERA_VERTEX_FIELD(FullscreenTriangleVertex, uv, 0, VertexFormat::Float2))
                .layout();

        m_displayPipeline = m_renderer.createGraphicsPipeline({
            .shaderProgram = m_displayShaderProgram,
            .vertexInput = displayVertexInput,
            .cullMode = CullModeKind::None,
            .debugName = stringView("DamagedHelmet Display Pipeline"),
        });

        if (!m_displayPipeline.isValid())
        {
            return false;
        }

        m_displayDescriptorSet = m_renderer.createDescriptorSet(m_displayPipeline);
        return m_displayDescriptorSet.isValid() &&
               m_renderer.updateDescriptors(m_displayDescriptorSet)
                   .sampledImage(DamagedHelmetShader::SceneTexture, m_sceneTexture)
                   .sampler(DamagedHelmetShader::MaterialSampler, m_model.materialSampler)
                   .ok();
    }

    void DamagedHelmetSample::update(float deltaTime)
    {
        if (m_initialized)
        {
            m_elapsedTime += deltaTime;
        }
    }

    void DamagedHelmetSample::resize(Extent2D extent)
    {
        if (extent == m_renderExtent || extent.width == 0 || extent.height == 0)
        {
            return;
        }

        if (!recreateRenderResources(extent))
        {
            sampleLogError("Failed to resize DamagedHelmet render resources.");
            m_initialized = false;
        }
    }

    void DamagedHelmetSample::render(RenderContext& context)
    {
        if (!m_initialized)
        {
            return;
        }

        if (!m_meshPipeline.isValid() || !m_displayPipeline.isValid() || !m_sceneRenderTarget.isValid() ||
            m_meshDescriptorSets.empty() || !m_displayDescriptorSet.isValid())
        {
            sampleLogWarning("Render called before DamagedHelmet resources were initialized.");
            return;
        }

        context.renderToTexture(
            m_sceneRenderTarget, getClearColor(),
            [this](FrameHandle frame)
            {
                HelmetUniforms uniforms{};
                const float aspect = m_renderExtent.height == 0 ? 16.0f / 9.0f
                                                                : static_cast<float>(m_renderExtent.width) /
                                                                      static_cast<float>(m_renderExtent.height);
                const glm::vec3 cameraPosition(0.36f, 0.08f, -3.0f);
                const float yawRadians = m_fixedYaw ? m_initialYawRadians : m_initialYawRadians + m_elapsedTime * 0.25f;
                uniforms.model = glm::rotate(glm::mat4(1.0f), yawRadians, glm::vec3(0.0f, 1.0f, 0.0f)) *
                                 glm::make_mat4(m_model.transform);
                uniforms.view = glm::lookAt(cameraPosition, glm::vec3(0.0f, -0.05f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                uniforms.projection = glm::perspective(glm::radians(42.0f), aspect, 0.1f, 100.0f);
                uniforms.cameraPosition = glm::vec4(cameraPosition, 1.0f);
                uniforms.lightDirectionAmbient = glm::vec4(glm::normalize(glm::vec3(-0.42f, -0.72f, -0.48f)), 0.08f);
                uniforms.baseColorFactor =
                    glm::vec4(m_model.materialFactors.baseColor[0], m_model.materialFactors.baseColor[1],
                              m_model.materialFactors.baseColor[2], m_model.materialFactors.baseColor[3]);
                uniforms.emissiveFactorNormalScale =
                    glm::vec4(m_model.materialFactors.emissive[0], m_model.materialFactors.emissive[1],
                              m_model.materialFactors.emissive[2], m_model.materialFactors.normalScale);
                uniforms.metallicRoughnessOcclusion =
                    glm::vec4(m_model.materialFactors.metallic, m_model.materialFactors.roughness,
                              m_model.materialFactors.occlusionStrength, 0.0f);
                uniforms.alphaModeCutoffReflectionExposure =
                    glm::vec4(toShaderAlphaMode(m_model.materialFactors.alphaMode), m_model.materialFactors.alphaCutoff,
                              1.22f, 0.86f);
                uniforms.debugViewGamma = glm::vec4(static_cast<float>(m_debugView), 2.2f, 1.0f / 2.2f, 0.0f);
                uniforms.padding2 = glm::vec4(m_model.materialFactors.doubleSided ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);

                if (!m_renderer.uploadUniformRingBuffer(m_uniformBuffer, frame, &uniforms, sizeof(uniforms)))
                {
                    sampleLogError("Failed to upload DamagedHelmet uniforms.");
                    return;
                }

                uint32_t uniformBufferSlot = m_renderer.getUniformRingBufferSlot(m_uniformBuffer, frame);

                m_renderer.bindPipeline(frame, m_meshPipeline);
                m_renderer.bindVertexBuffer(frame, 0, m_model.vertexBuffer);
                m_renderer.bindIndexBuffer(frame, m_model.indexBuffer, m_model.indexFormat);
                m_renderer.bindDescriptorSet(frame, m_meshPipeline, m_meshDescriptorSets[uniformBufferSlot]);
                m_renderer.drawIndexed(frame, m_model.indexCount);
            });

        context.renderToBackbuffer(getClearColor(),
                                   [this](FrameHandle frame)
                                   {
                                       m_renderer.bindPipeline(frame, m_displayPipeline);
                                       m_renderer.bindVertexBuffer(frame, 0, m_fullscreenVertexBuffer);
                                       m_renderer.bindIndexBuffer(frame, m_fullscreenIndexBuffer, IndexFormat::UInt16);
                                       m_renderer.bindDescriptorSet(frame, m_displayPipeline, m_displayDescriptorSet);
                                       m_renderer.drawIndexed(frame, m_fullscreenIndexCount);
                                   });
    }

    ClearColorValue DamagedHelmetSample::getClearColor() const
    {
        return {0.9f, 0.9f, 0.88f, 1.0f};
    }

    void DamagedHelmetSample::destroyRenderResources()
    {
        if (m_displayDescriptorSet.isValid())
        {
            m_renderer.destroyDescriptorSet(m_displayDescriptorSet);
            m_displayDescriptorSet = {};
        }
        for (DescriptorSetHandle descriptorSet : m_meshDescriptorSets)
        {
            if (descriptorSet.isValid())
            {
                m_renderer.destroyDescriptorSet(descriptorSet);
            }
        }

        m_meshDescriptorSets.clear();

        if (m_displayPipeline.isValid())
        {
            m_renderer.destroyGraphicsPipeline(m_displayPipeline);
            m_displayPipeline = {};
        }

        if (m_meshPipeline.isValid())
        {
            m_renderer.destroyGraphicsPipeline(m_meshPipeline);
            m_meshPipeline = {};
        }

        if (m_sceneRenderTarget.isValid())
        {
            m_renderer.destroyRenderTarget(m_sceneRenderTarget);
            m_sceneRenderTarget = {};
        }

        if (m_sceneDepthTexture.isValid())
        {
            m_renderer.destroyTexture(m_sceneDepthTexture);
            m_sceneDepthTexture = {};
        }

        if (m_sceneTexture.isValid())
        {
            m_renderer.destroyTexture(m_sceneTexture);
            m_sceneTexture = {};
        }
    }

    void DamagedHelmetSample::destroyLoadedResources()
    {
        if (m_uniformBuffer.isValid())
        {
            m_renderer.destroyBuffer(m_uniformBuffer);
            m_uniformBuffer = {};
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
        m_renderer.destroyGltfModel(m_model);
    }

    void DamagedHelmetSample::cleanup()
    {
        sampleLogInfo("Cleaning up " + std::string(getName()));
        m_initialized = false;

        destroyRenderResources();
        destroyLoadedResources();

        if (m_displayShaderProgram.isValid())
        {
            m_renderer.destroyShaderProgram(m_displayShaderProgram);
            m_displayShaderProgram = {};
        }

        if (m_meshShaderProgram.isValid())
        {
            m_renderer.destroyShaderProgram(m_meshShaderProgram);
            m_meshShaderProgram = {};
        }
    }

}  // namespace kera
