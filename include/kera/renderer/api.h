// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/renderer/abi.h"

#include <cstddef>

#ifdef __cplusplus
namespace kera
{
    inline constexpr KeraStringView stringView(const char* text, size_t size) noexcept
    {
        return {text, size};
    }

    inline KeraStringView stringView(const char* text) noexcept
    {
        size_t size = 0;
        if (text)
        {
            while (text[size] != '\0')
            {
                ++size;
            }
        }
        return {text, size};
    }

    using Extent2D = KeraExtent2D;
    using ClearColorValue = KeraClearColorValue;
    using RendererStats = KeraRendererStats;
    using RendererResourceStats = KeraRendererResourceStats;
    using ShaderProgramHandle = KeraShaderProgramHandle;
    using BufferHandle = KeraBufferHandle;
    using TextureHandle = KeraTextureHandle;
    using SamplerHandle = KeraSamplerHandle;
    using RenderTargetHandle = KeraRenderTargetHandle;
    using GraphicsPipelineHandle = KeraGraphicsPipelineHandle;
    using DescriptorSetHandle = KeraDescriptorSetHandle;
    using FrameHandle = KeraFrameHandle;
    using GltfLoadedModel = KeraGltfLoadedModel;
    using GltfVertex = KeraGltfVertex;

    namespace GraphicsBackend
    {
        inline constexpr KeraGraphicsBackend Vulkan = KERA_GRAPHICS_BACKEND_VULKAN;
    }

    namespace BufferUsageKind
    {
        inline constexpr KeraBufferUsageKind Vertex = KERA_BUFFER_USAGE_VERTEX;
        inline constexpr KeraBufferUsageKind Index = KERA_BUFFER_USAGE_INDEX;
        inline constexpr KeraBufferUsageKind Uniform = KERA_BUFFER_USAGE_UNIFORM;
        inline constexpr KeraBufferUsageKind Storage = KERA_BUFFER_USAGE_STORAGE;
    }

    namespace MemoryAccess
    {
        inline constexpr KeraMemoryAccess GpuOnly = KERA_MEMORY_ACCESS_GPU_ONLY;
        inline constexpr KeraMemoryAccess CpuWrite = KERA_MEMORY_ACCESS_CPU_WRITE;
    }

    namespace IndexFormat
    {
        inline constexpr KeraIndexFormat UInt16 = KERA_INDEX_FORMAT_UINT16;
        inline constexpr KeraIndexFormat UInt32 = KERA_INDEX_FORMAT_UINT32;
    }

    namespace VertexFormat
    {
        inline constexpr KeraVertexFormat Float2 = KERA_VERTEX_FORMAT_FLOAT2;
        inline constexpr KeraVertexFormat Float3 = KERA_VERTEX_FORMAT_FLOAT3;
        inline constexpr KeraVertexFormat Float4 = KERA_VERTEX_FORMAT_FLOAT4;
    }

    namespace VertexInputRate
    {
        inline constexpr KeraVertexInputRate Vertex = KERA_VERTEX_INPUT_RATE_VERTEX;
        inline constexpr KeraVertexInputRate Instance = KERA_VERTEX_INPUT_RATE_INSTANCE;
    }

    namespace PrimitiveTopologyKind
    {
        inline constexpr KeraPrimitiveTopologyKind TriangleList = KERA_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }

    namespace CullModeKind
    {
        inline constexpr KeraCullModeKind None = KERA_CULL_MODE_NONE;
        inline constexpr KeraCullModeKind Front = KERA_CULL_MODE_FRONT;
        inline constexpr KeraCullModeKind Back = KERA_CULL_MODE_BACK;
    }

    namespace FrontFaceKind
    {
        inline constexpr KeraFrontFaceKind Clockwise = KERA_FRONT_FACE_CLOCKWISE;
        inline constexpr KeraFrontFaceKind CounterClockwise = KERA_FRONT_FACE_COUNTER_CLOCKWISE;
    }

    namespace BlendModeKind
    {
        inline constexpr KeraBlendModeKind Opaque = KERA_BLEND_MODE_OPAQUE;
        inline constexpr KeraBlendModeKind Alpha = KERA_BLEND_MODE_ALPHA;
    }

    namespace DescriptorType
    {
        inline constexpr KeraDescriptorType UniformBuffer = KERA_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        inline constexpr KeraDescriptorType SampledImage = KERA_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        inline constexpr KeraDescriptorType Sampler = KERA_DESCRIPTOR_TYPE_SAMPLER;
    }

    namespace TextureFormat
    {
        inline constexpr KeraTextureFormat RGBA8 = KERA_TEXTURE_FORMAT_RGBA8;
        inline constexpr KeraTextureFormat RGBA8Srgb = KERA_TEXTURE_FORMAT_RGBA8_SRGB;
        inline constexpr KeraTextureFormat B10G11R11Ufloat = KERA_TEXTURE_FORMAT_B10G11R11_UFLOAT;
        inline constexpr KeraTextureFormat Depth32 = KERA_TEXTURE_FORMAT_DEPTH32;
    }

    namespace SamplerFilter
    {
        inline constexpr KeraSamplerFilter Nearest = KERA_SAMPLER_FILTER_NEAREST;
        inline constexpr KeraSamplerFilter Linear = KERA_SAMPLER_FILTER_LINEAR;
    }

    namespace SamplerMipFilter
    {
        inline constexpr KeraSamplerMipFilter Nearest = KERA_SAMPLER_MIP_FILTER_NEAREST;
        inline constexpr KeraSamplerMipFilter Linear = KERA_SAMPLER_MIP_FILTER_LINEAR;
    }

    namespace SamplerAddressMode
    {
        inline constexpr KeraSamplerAddressMode ClampToEdge = KERA_SAMPLER_ADDRESS_CLAMP_TO_EDGE;
        inline constexpr KeraSamplerAddressMode Repeat = KERA_SAMPLER_ADDRESS_REPEAT;
        inline constexpr KeraSamplerAddressMode MirroredRepeat = KERA_SAMPLER_ADDRESS_MIRRORED_REPEAT;
    }
    namespace GltfAlphaMode
    {
        inline constexpr KeraGltfAlphaMode Opaque = KERA_GLTF_ALPHA_OPAQUE;
        inline constexpr KeraGltfAlphaMode Mask = KERA_GLTF_ALPHA_MASK;
        inline constexpr KeraGltfAlphaMode Blend = KERA_GLTF_ALPHA_BLEND;
    }

    using BufferDesc = KeraBufferDesc;
    using TextureDesc = KeraTextureDesc;
    using SamplerDesc = KeraSamplerDesc;
    using RenderTargetDesc = KeraRenderTargetDesc;
    using GraphicsShaderProgramDesc = KeraGraphicsShaderProgramDesc;
    using VertexInputBindingDesc = KeraVertexInputBindingDesc;
    using VertexInputFieldDesc = KeraVertexInputFieldDesc;
    using GltfLoadDesc = KeraGltfLoadDesc;
    using IblEnvironmentLoadDesc = KeraIblEnvironmentLoadDesc;
    using IblEnvironment = KeraIblEnvironment;
    using IblSphericalHarmonics = KeraIblSphericalHarmonics;

#define KERA_VERTEX_FIELD(VertexType, member, bindingIndex, vertexFormat) \
    #member, bindingIndex, static_cast<uint32_t>(offsetof(VertexType, member)), vertexFormat

    struct VertexInputLayout
    {
        KeraVertexInputBindingDesc bindings[16]{};
        size_t bindingCount = 0;
        KeraVertexInputFieldDesc fields[32]{};
        size_t fieldCount = 0;

        KeraVertexInputLayout view() const noexcept
        {
            return {
                bindings,
                bindingCount,
                fields,
                fieldCount,
            };
        }
    };

    class VertexInputLayoutBuilder
    {
    public:
        VertexInputLayoutBuilder& vertexBinding(uint32_t binding, uint32_t stride,
                                                KeraVertexInputRate inputRate = KERA_VERTEX_INPUT_RATE_VERTEX) noexcept
        {
            if (m_layout.bindingCount < 16)
            {
                m_layout.bindings[m_layout.bindingCount++] = {
                    binding,
                    stride,
                    inputRate,
                };
            }
            return *this;
        }

        template <typename VertexT>
        VertexInputLayoutBuilder& vertexBinding(uint32_t binding,
                                                KeraVertexInputRate inputRate = KERA_VERTEX_INPUT_RATE_VERTEX) noexcept
        {
            return vertexBinding(binding, static_cast<uint32_t>(sizeof(VertexT)), inputRate);
        }

        VertexInputLayoutBuilder& field(const char* fieldName, uint32_t binding, uint32_t offset,
                                        KeraVertexFormat format) noexcept
        {
            if (m_layout.fieldCount < 32)
            {
                m_layout.fields[m_layout.fieldCount++] = {
                    {}, stringView(fieldName), binding, offset, format,
                };
            }
            return *this;
        }

        VertexInputLayoutBuilder& fieldIn(const char* parameterName, const char* fieldName, uint32_t binding,
                                          uint32_t offset, KeraVertexFormat format) noexcept
        {
            if (m_layout.fieldCount < 32)
            {
                m_layout.fields[m_layout.fieldCount++] = {
                    stringView(parameterName), stringView(fieldName), binding, offset, format,
                };
            }
            return *this;
        }

        VertexInputLayout layout() const noexcept
        {
            return m_layout;
        }

    private:
        VertexInputLayout m_layout{};
    };

    struct GraphicsPipelineCreateDesc
    {
        KeraShaderProgramHandle shaderProgram{};
        VertexInputLayout vertexInput{};
        KeraRenderTargetHandle renderTarget{};
        KeraPrimitiveTopologyKind topology = KERA_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        KeraCullModeKind cullMode = KERA_CULL_MODE_BACK;
        KeraFrontFaceKind frontFace = KERA_FRONT_FACE_CLOCKWISE;
        KeraBlendModeKind blendMode = KERA_BLEND_MODE_OPAQUE;
        bool depthTest = false;
        bool depthWrite = false;
        KeraStringView debugName{};
    };

    class DescriptorSetWriter;

    class Renderer
    {
    public:
        Renderer() noexcept = default;
        Renderer(KeraRenderer* renderer, const KeraRendererApiV1* api) noexcept : m_renderer(renderer), m_api(api) {}

        static Renderer create(const KeraRendererCreateDesc& desc) noexcept
        {
            const KeraRendererApiV1* api = keraGetRendererApiV1();
            return api ? Renderer(api->createRenderer(&desc), api) : Renderer{};
        }

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&& other) noexcept : m_renderer(other.m_renderer), m_api(other.m_api)
        {
            other.m_renderer = nullptr;
            other.m_api = nullptr;
        }

        Renderer& operator=(Renderer&& other) noexcept
        {
            if (this != &other)
            {
                destroy();
                m_renderer = other.m_renderer;
                m_api = other.m_api;
                other.m_renderer = nullptr;
                other.m_api = nullptr;
            }
            return *this;
        }

        ~Renderer()
        {
            destroy();
        }

        bool isValid() const noexcept
        {
            return m_renderer && m_api;
        }

        KeraRenderer* native() const noexcept
        {
            return m_renderer;
        }

        const KeraRendererApiV1* api() const noexcept
        {
            return m_api;
        }

        void destroy() noexcept
        {
            if (m_api && m_renderer)
            {
                m_api->destroy(m_renderer);
            }
            m_renderer = nullptr;
        }

        void shutdown() noexcept
        {
            if (isValid()) m_api->shutdown(m_renderer);
        }

        KeraExtent2D getDrawableExtent() const noexcept
        {
            return isValid() ? m_api->getDrawableExtent(m_renderer) : KeraExtent2D{};
        }

        KeraRendererStats getStats() const noexcept
        {
            return isValid() ? m_api->getStats(m_renderer) : KeraRendererStats{};
        }

        bool resize(KeraExtent2D extent) noexcept
        {
            return isValid() && m_api->resize(m_renderer, extent) != 0;
        }

        bool initializeUi() noexcept
        {
            return isValid() && m_api->initializeUi(m_renderer) != 0;
        }

        void shutdownUi() noexcept
        {
            if (isValid()) m_api->shutdownUi(m_renderer);
        }

        void handleUiEvent(const void* event) noexcept
        {
            if (isValid()) m_api->handleUiEvent(m_renderer, event);
        }

        void beginUi() noexcept
        {
            if (isValid()) m_api->beginUi(m_renderer);
        }

        void endUi() noexcept
        {
            if (isValid()) m_api->endUi(m_renderer);
        }

        void renderUi(KeraFrameHandle frame) noexcept
        {
            if (isValid()) m_api->renderUi(m_renderer, frame);
        }

        bool isUiAvailable() const noexcept
        {
            return isValid() && m_api->isUiAvailable(m_renderer) != 0;
        }

        KeraShaderProgramHandle createGraphicsShaderProgram(const KeraGraphicsShaderProgramDesc& desc) noexcept
        {
            return isValid() ? m_api->createGraphicsShaderProgram(m_renderer, &desc) : KeraShaderProgramHandle{};
        }

        bool destroyShaderProgram(KeraShaderProgramHandle program) noexcept
        {
            return isValid() && m_api->destroyShaderProgram(m_renderer, program) != 0;
        }

        KeraBufferHandle createBuffer(const KeraBufferDesc& desc) noexcept
        {
            return isValid() ? m_api->createBuffer(m_renderer, &desc) : KeraBufferHandle{};
        }

        KeraBufferHandle createUniformRingBuffer(size_t elementSize, uint32_t slotCount) noexcept
        {
            return isValid() ? m_api->createUniformRingBuffer(m_renderer, elementSize, slotCount) : KeraBufferHandle{};
        }

        bool destroyBuffer(KeraBufferHandle buffer) noexcept
        {
            return isValid() && m_api->destroyBuffer(m_renderer, buffer) != 0;
        }

        bool mapBuffer(KeraBufferHandle buffer, void** data) noexcept
        {
            return isValid() && m_api->mapBuffer(m_renderer, buffer, data) != 0;
        }

        void unmapBuffer(KeraBufferHandle buffer) noexcept
        {
            if (isValid()) m_api->unmapBuffer(m_renderer, buffer);
        }

        bool uploadBuffer(KeraBufferHandle buffer, const void* data, size_t size, size_t offset = 0) noexcept
        {
            return isValid() && m_api->uploadBuffer(m_renderer, buffer, data, size, offset) != 0;
        }

        bool uploadUniformRingBuffer(KeraBufferHandle buffer, KeraFrameHandle frame, const void* data,
                                     size_t size) noexcept
        {
            return isValid() && m_api->uploadUniformRingBuffer(m_renderer, buffer, frame, data, size) != 0;
        }

        size_t getUniformRingBufferOffset(KeraBufferHandle buffer, KeraFrameHandle frame) const noexcept
        {
            return isValid() ? m_api->getUniformRingBufferOffset(m_renderer, buffer, frame) : 0;
        }

        KeraTextureHandle createTexture(const KeraTextureDesc& desc) noexcept
        {
            return isValid() ? m_api->createTexture(m_renderer, &desc) : KeraTextureHandle{};
        }

        bool uploadTexture(KeraTextureHandle texture, const void* data, size_t size) noexcept
        {
            return isValid() && m_api->uploadTexture(m_renderer, texture, data, size) != 0;
        }

        bool destroyTexture(KeraTextureHandle texture) noexcept
        {
            return isValid() && m_api->destroyTexture(m_renderer, texture) != 0;
        }

        KeraSamplerHandle createSampler(const KeraSamplerDesc& desc = {}) noexcept
        {
            return isValid() ? m_api->createSampler(m_renderer, &desc) : KeraSamplerHandle{};
        }

        bool destroySampler(KeraSamplerHandle sampler) noexcept
        {
            return isValid() && m_api->destroySampler(m_renderer, sampler) != 0;
        }

        KeraRenderTargetHandle createRenderTarget(const KeraRenderTargetDesc& desc) noexcept
        {
            return isValid() ? m_api->createRenderTarget(m_renderer, &desc) : KeraRenderTargetHandle{};
        }

        bool destroyRenderTarget(KeraRenderTargetHandle target) noexcept
        {
            return isValid() && m_api->destroyRenderTarget(m_renderer, target) != 0;
        }

        KeraGraphicsPipelineHandle createGraphicsPipeline(const GraphicsPipelineCreateDesc& desc) noexcept
        {
            if (!isValid()) return {};
            const KeraGraphicsPipelineCreateDesc abiDesc{
                desc.shaderProgram,
                desc.vertexInput.view(),
                desc.renderTarget,
                desc.topology,
                desc.cullMode,
                desc.frontFace,
                desc.blendMode,
                static_cast<uint8_t>(desc.depthTest ? 1 : 0),
                static_cast<uint8_t>(desc.depthWrite ? 1 : 0),
                desc.debugName,
            };
            return m_api->createGraphicsPipeline(m_renderer, &abiDesc);
        }

        KeraRendererValidationReport validateVertexInputLayout(KeraShaderProgramHandle shaderProgram,
                                                               const VertexInputLayout& vertexInput) const noexcept
        {
            return isValid() ? m_api->validateVertexInputLayout(m_renderer, shaderProgram, vertexInput.view())
                             : KeraRendererValidationReport{};
        }

        bool destroyGraphicsPipeline(KeraGraphicsPipelineHandle pipeline) noexcept
        {
            return isValid() && m_api->destroyGraphicsPipeline(m_renderer, pipeline) != 0;
        }

        KeraDescriptorSetHandle createDescriptorSet(KeraGraphicsPipelineHandle pipeline, uint32_t set = 0) noexcept
        {
            return isValid() ? m_api->createDescriptorSet(m_renderer, pipeline, set) : KeraDescriptorSetHandle{};
        }

        bool destroyDescriptorSet(KeraDescriptorSetHandle set) noexcept
        {
            return isValid() && m_api->destroyDescriptorSet(m_renderer, set) != 0;
        }

        DescriptorSetWriter updateDescriptors(KeraDescriptorSetHandle set) noexcept;
        KeraFrameHandle beginFrame() noexcept
        {
            return isValid() ? m_api->beginFrame(m_renderer) : KeraFrameHandle{};
        }

        void beginRenderPass(KeraFrameHandle frame, const KeraRenderPassDesc& desc) noexcept
        {
            if (isValid()) m_api->beginRenderPass(m_renderer, frame, &desc);
        }

        void beginRenderPass(KeraFrameHandle frame, KeraRenderTargetHandle target,
                             const KeraRenderPassDesc& desc) noexcept
        {
            if (isValid()) m_api->beginRenderPassTarget(m_renderer, frame, target, &desc);
        }

        void endRenderPass(KeraFrameHandle frame) noexcept
        {
            if (isValid()) m_api->endRenderPass(m_renderer, frame);
        }

        void bindPipeline(KeraFrameHandle frame, KeraGraphicsPipelineHandle pipeline) noexcept
        {
            if (isValid()) m_api->bindPipeline(m_renderer, frame, pipeline);
        }

        void bindVertexBuffer(KeraFrameHandle frame, uint32_t slot, KeraBufferHandle buffer, size_t offset = 0) noexcept
        {
            if (isValid()) m_api->bindVertexBuffer(m_renderer, frame, slot, buffer, offset);
        }

        void bindIndexBuffer(KeraFrameHandle frame, KeraBufferHandle buffer, KeraIndexFormat format,
                             size_t offset = 0) noexcept
        {
            if (isValid()) m_api->bindIndexBuffer(m_renderer, frame, buffer, format, offset);
        }

        void bindDescriptorSet(KeraFrameHandle frame, KeraGraphicsPipelineHandle pipeline,
                               KeraDescriptorSetHandle set) noexcept
        {
            if (isValid()) m_api->bindDescriptorSet(m_renderer, frame, pipeline, 0, set);
        }

        void drawIndexed(KeraFrameHandle frame, uint32_t indexCount, uint32_t instanceCount = 1) noexcept
        {
            if (isValid()) m_api->drawIndexed(m_renderer, frame, indexCount, instanceCount);
        }

        bool endFrame(KeraFrameHandle frame) noexcept
        {
            return isValid() && m_api->endFrame(m_renderer, frame) != 0;
        }

        bool loadGltfModel(const KeraGltfLoadDesc& desc, KeraGltfLoadedModel& outModel) noexcept
        {
            return isValid() && m_api->loadGltfModel(m_renderer, &desc, &outModel) != 0;
        }

        void destroyGltfModel(KeraGltfLoadedModel& model) noexcept
        {
            if (isValid()) m_api->destroyGltfModel(m_renderer, &model);
        }

        bool loadIblEnvironment(const IblEnvironmentLoadDesc& desc, KeraIblEnvironment& outEnvironment) noexcept
        {
            return isValid() && m_api->loadIblEnvironment(m_renderer, &desc, &outEnvironment) != 0;
        }

        void destroyIblEnvironment(KeraIblEnvironment& env) noexcept
        {
            if (isValid()) m_api->destroyIblEnvironment(m_renderer, &env);
        }

    private:
        KeraRenderer* m_renderer = nullptr;
        const KeraRendererApiV1* m_api = nullptr;
    };

    class DescriptorSetWriter
    {
    public:
        DescriptorSetWriter(Renderer& renderer, KeraDescriptorSetHandle set) noexcept
            : m_renderer(&renderer), m_set(set)
        {
        }

        DescriptorSetWriter& uniform(const char* name, KeraBufferHandle buffer, size_t offset = 0,
                                     size_t range = 0) noexcept
        {
            m_success = m_success && m_renderer->api()->updateDescriptorBuffer(
                                         m_renderer->native(), m_set, stringView(name), buffer, offset, range) != 0;
            return *this;
        }

        template <typename UniformT>
        DescriptorSetWriter& uniform(const char* name, KeraBufferHandle buffer, size_t offset = 0) noexcept
        {
            return uniform(name, buffer, offset, sizeof(UniformT));
        }

        DescriptorSetWriter& sampledImage(const char* name, KeraTextureHandle texture) noexcept
        {
            m_success = m_success && m_renderer->api()->updateDescriptorTexture(m_renderer->native(), m_set,
                                                                                stringView(name), texture) != 0;
            return *this;
        }

        DescriptorSetWriter& sampler(const char* name, KeraSamplerHandle sampler) noexcept
        {
            m_success = m_success && m_renderer->api()->updateDescriptorSampler(m_renderer->native(), m_set,
                                                                                stringView(name), sampler) != 0;
            return *this;
        }

        bool ok() const noexcept
        {
            return m_success;
        }

    private:
        Renderer* m_renderer;
        KeraDescriptorSetHandle m_set;
        bool m_success = true;
    };

    inline DescriptorSetWriter Renderer::updateDescriptors(KeraDescriptorSetHandle set) noexcept
    {
        return DescriptorSetWriter(*this, set);
    }
}  // namespace kera
#endif
