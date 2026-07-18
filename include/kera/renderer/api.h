// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/renderer/abi.h"

#include <cstddef>

#ifdef __cplusplus
namespace kera
{
    enum class EGraphicsBackend
    {
        VULKAN
    };

    enum class EShaderSourceKind
    {
        SLANG_FILE,
        SPIRV_FILE,
        SPIRV_BINARY
    };

    enum class EBufferUsageKind
    {
        VERTEX,
        INDEX,
        UNIFORM,
        STORAGE
    };

    enum class EMemoryAccess
    {
        GPU_ONLY,
        CPU_WRITE
    };

    enum class EIndexFormat
    {
        U_INT16,
        U_INT32
    };

    enum class EVertexFormat
    {
        FLOAT2,
        FLOAT3,
        FLOAT4
    };

    enum class EVertexInputRate
    {
        VERTEX,
        INSTANCE
    };

    enum class EPrimitiveTopologyKind
    {
        TRIANGLE_LIST
    };

    enum class ECullModeKind
    {
        NONE,
        FRONT,
        BACK
    };

    enum class EFrontFaceKind
    {
        CLOCKWISE,
        COUNTER_CLOCKWISE
    };

    enum class EBlendModeKind
    {
        OPAQUE,
        ALPHA
    };

    enum class ETextureFormat
    {
        RGBA8,
        RGB_A8_SRGB,
        B10_G11_R11_UFLOAT,
        DEPTH32
    };

    enum class EGltfAlphaMode
    {
        ALPHA_OPAQUE,
        ALPHA_MASK,
        ALPHA_BLEND
    };

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

    using SamplerDesc = KeraSamplerDesc;
    using RenderTargetDesc = KeraRenderTargetDesc;
    using VertexInputBindingDesc = KeraVertexInputBindingDesc;
    using VertexInputFieldDesc = KeraVertexInputFieldDesc;
    using GltfLoadDesc = KeraGltfLoadDesc;
    using IblEnvironmentLoadDesc = KeraIblEnvironmentLoadDesc;
    using IblEnvironment = KeraIblEnvironment;
    using IblSphericalHarmonics = KeraIblSphericalHarmonics;

    struct BufferDesc
    {
        size_t size = 0;
        EBufferUsageKind usage = EBufferUsageKind::VERTEX;
        EMemoryAccess memory_access = EMemoryAccess::GPU_ONLY;
        KeraStringView debug_name = {};

        KeraBufferDesc view() const noexcept
        {
            return {
                size,
                static_cast<KeraBufferUsageKind>(usage),
                static_cast<KeraMemoryAccess>(memory_access),
                debug_name,
            };
        }
    };

    struct TextureDesc
    {
        uint32_t width = 0;
        uint32_t height = 0;
        ETextureFormat format = ETextureFormat::RGBA8;
        bool generate_mipmaps = false;
        bool render_target = false;
        bool sampled = true;
        bool depth_stencil = false;
        KeraStringView debug_name = {};

        KeraTextureDesc view() const noexcept
        {
            return {
                width,
                height,
                static_cast<KeraTextureFormat>(format),
                1,
                static_cast<uint8_t>(generate_mipmaps ? 1 : 0),
                static_cast<uint8_t>(render_target ? 1 : 0),
                static_cast<uint8_t>(sampled ? 1 : 0),
                static_cast<uint8_t>(depth_stencil ? 1 : 0),
                debug_name,
            };
        }
    };

    struct GraphicsShaderProgramDesc
    {
        KeraStringView path = {};
        KeraStringView vertex_entry_point = stringView("vertexMain");
        KeraStringView fragment_entry_point = stringView("fragmentMain");
        EShaderSourceKind source = EShaderSourceKind::SLANG_FILE;
        KeraStringView debug_name = {};

        KeraGraphicsShaderProgramDesc view() const noexcept
        {
            return {
                path,
                vertex_entry_point,
                fragment_entry_point,
                static_cast<KeraShaderSourceKind>(source),
                debug_name,
            };
        }
    };

#define KERA_VERTEX_FIELD(VertexType, member, bindingIndex, vertexFormat) \
    #member, bindingIndex, static_cast<uint32_t>(offsetof(VertexType, member)), \
    static_cast<KeraVertexFormat>(vertexFormat)

    struct VertexInputLayout
    {
        KeraVertexInputBindingDesc bindings[16]{};
        size_t binding_count = 0;
        KeraVertexInputFieldDesc fields[32]{};
        size_t field_count = 0;

        KeraVertexInputLayout view() const noexcept
        {
            return {
                bindings,
                binding_count,
                fields,
                field_count,
            };
        }
    };

    class VertexInputLayoutBuilder
    {
    public:
        VertexInputLayoutBuilder& vertexBinding(uint32_t binding, uint32_t stride,
                                                KeraVertexInputRate input_rate = KERA_VERTEX_INPUT_RATE_VERTEX) noexcept
        {
            if (m_layout.binding_count < 16)
            {
                m_layout.bindings[m_layout.binding_count++] = {
                    binding,
                    stride,
                    input_rate,
                };
            }
            return *this;
        }

        template <typename VertexT>
        VertexInputLayoutBuilder& vertexBinding(uint32_t binding,
                                                KeraVertexInputRate input_rate = KERA_VERTEX_INPUT_RATE_VERTEX) noexcept
        {
            return vertexBinding(binding, static_cast<uint32_t>(sizeof(VertexT)), input_rate);
        }

        VertexInputLayoutBuilder& field(const char* field_name, uint32_t binding, uint32_t offset,
                                        KeraVertexFormat format) noexcept
        {
            if (m_layout.field_count < 32)
            {
                m_layout.fields[m_layout.field_count++] = {
                    {}, stringView(field_name), binding, offset, format,
                };
            }
            return *this;
        }

        VertexInputLayoutBuilder& fieldIn(const char* parameter_name, const char* field_name, uint32_t binding,
                                          uint32_t offset, KeraVertexFormat format) noexcept
        {
            if (m_layout.field_count < 32)
            {
                m_layout.fields[m_layout.field_count++] = {
                    stringView(parameter_name), stringView(field_name), binding, offset, format,
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
        KeraShaderProgramHandle shader_program{};
        VertexInputLayout vertex_input{};
        KeraRenderTargetHandle render_target{};
        EPrimitiveTopologyKind topology = EPrimitiveTopologyKind::TRIANGLE_LIST;
        ECullModeKind cull_mode = ECullModeKind::BACK;
        EFrontFaceKind front_face = EFrontFaceKind::CLOCKWISE;
        EBlendModeKind blend_mode = EBlendModeKind::OPAQUE;
        bool depth_test = false;
        bool depth_write = false;
        KeraStringView debug_name{};
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
            return api ? Renderer(api->create_renderer(&desc), api) : Renderer{};
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
            return isValid() ? m_api->get_drawable_extent(m_renderer) : KeraExtent2D{};
        }

        KeraRendererStats getStats() const noexcept
        {
            return isValid() ? m_api->get_stats(m_renderer) : KeraRendererStats{};
        }

        bool resize(KeraExtent2D extent) noexcept
        {
            return isValid() && m_api->resize(m_renderer, extent) != 0;
        }

        bool initializeUi() noexcept
        {
            return isValid() && m_api->initialize_ui(m_renderer) != 0;
        }

        void shutdownUi() noexcept
        {
            if (isValid()) m_api->shutdown_ui(m_renderer);
        }

        void handleUiEvent(const void* event) noexcept
        {
            if (isValid()) m_api->handle_ui_event(m_renderer, event);
        }

        void beginUi() noexcept
        {
            if (isValid()) m_api->begin_ui(m_renderer);
        }

        void endUi() noexcept
        {
            if (isValid()) m_api->end_ui(m_renderer);
        }

        void renderUi(KeraFrameHandle frame) noexcept
        {
            if (isValid()) m_api->render_ui(m_renderer, frame);
        }

        bool isUiAvailable() const noexcept
        {
            return isValid() && m_api->is_ui_available(m_renderer) != 0;
        }

        KeraShaderProgramHandle createGraphicsShaderProgram(const GraphicsShaderProgramDesc& desc) noexcept
        {
            if (!isValid()) return {};
            const auto abi_desc = desc.view();
            return m_api->create_graphics_shader_program(m_renderer, &abi_desc);
        }

        bool destroyShaderProgram(KeraShaderProgramHandle program) noexcept
        {
            return isValid() && m_api->destroy_shader_program(m_renderer, program) != 0;
        }

        KeraBufferHandle createBuffer(const BufferDesc& desc) noexcept
        {
            if (!isValid()) return {};
            const auto abi_desc = desc.view();
            return m_api->create_buffer(m_renderer, &abi_desc);
        }

        KeraBufferHandle createUniformRingBuffer(size_t element_size, uint32_t slot_count) noexcept
        {
            return isValid() ? m_api->create_uniform_ring_buffer(m_renderer, element_size, slot_count) : KeraBufferHandle{};
        }

        bool destroyBuffer(KeraBufferHandle buffer) noexcept
        {
            return isValid() && m_api->destroy_buffer(m_renderer, buffer) != 0;
        }

        bool mapBuffer(KeraBufferHandle buffer, void** data) noexcept
        {
            return isValid() && m_api->map_buffer(m_renderer, buffer, data) != 0;
        }

        void unmapBuffer(KeraBufferHandle buffer) noexcept
        {
            if (isValid()) m_api->unmap_buffer(m_renderer, buffer);
        }

        bool uploadBuffer(KeraBufferHandle buffer, const void* data, size_t size, size_t offset = 0) noexcept
        {
            return isValid() && m_api->upload_buffer(m_renderer, buffer, data, size, offset) != 0;
        }

        bool uploadUniformRingBuffer(KeraBufferHandle buffer, KeraFrameHandle frame, const void* data,
                                     size_t size) noexcept
        {
            return isValid() && m_api->upload_uniform_ring_buffer(m_renderer, buffer, frame, data, size) != 0;
        }

        KeraUniformRingBufferInfo getUniformRingBufferInfo(KeraBufferHandle buffer) const noexcept
        {
            return isValid() ? m_api->get_uniform_ring_buffer_info(m_renderer, buffer) : KeraUniformRingBufferInfo{};
        }

        uint32_t getUniformRingBufferSlot(KeraBufferHandle buffer, KeraFrameHandle frame) const noexcept
        {
            return isValid() ? m_api->get_uniform_ring_buffer_slot(m_renderer, buffer, frame) : 0;
        }

        size_t getUniformRingBufferSlotOffset(KeraBufferHandle buffer, uint32_t slot) const noexcept
        {
            return isValid() ? m_api->get_uniform_ring_buffer_slot_offset(m_renderer, buffer, slot) : 0;
        }

        KeraTextureHandle createTexture(const TextureDesc& desc) noexcept
        {
            if (!isValid()) return {};
            const auto abi_desc = desc.view();
            return m_api->create_texture(m_renderer, &abi_desc);
        }

        bool uploadTexture(KeraTextureHandle texture, const void* data, size_t size) noexcept
        {
            return isValid() && m_api->upload_texture(m_renderer, texture, data, size) != 0;
        }

        bool destroyTexture(KeraTextureHandle texture) noexcept
        {
            return isValid() && m_api->destroy_texture(m_renderer, texture) != 0;
        }

        KeraSamplerHandle createSampler(const KeraSamplerDesc& desc = {}) noexcept
        {
            return isValid() ? m_api->create_sampler(m_renderer, &desc) : KeraSamplerHandle{};
        }

        bool destroySampler(KeraSamplerHandle sampler) noexcept
        {
            return isValid() && m_api->destroy_sampler(m_renderer, sampler) != 0;
        }

        KeraRenderTargetHandle createRenderTarget(const KeraRenderTargetDesc& desc) noexcept
        {
            return isValid() ? m_api->create_render_target(m_renderer, &desc) : KeraRenderTargetHandle{};
        }

        bool destroyRenderTarget(KeraRenderTargetHandle target) noexcept
        {
            return isValid() && m_api->destroy_render_target(m_renderer, target) != 0;
        }

        KeraGraphicsPipelineHandle createGraphicsPipeline(const GraphicsPipelineCreateDesc& desc) noexcept
        {
            if (!isValid()) return {};
            const KeraGraphicsPipelineCreateDesc abi_desc{
                desc.shader_program,
                desc.vertex_input.view(),
                desc.render_target,
                static_cast<KeraPrimitiveTopologyKind>(desc.topology),
                static_cast<KeraCullModeKind>(desc.cull_mode),
                static_cast<KeraFrontFaceKind>(desc.front_face),
                static_cast<KeraBlendModeKind>(desc.blend_mode),
                static_cast<uint8_t>(desc.depth_test ? 1 : 0),
                static_cast<uint8_t>(desc.depth_write ? 1 : 0),
                desc.debug_name,
            };
            return m_api->create_graphics_pipeline(m_renderer, &abi_desc);
        }

        KeraRendererValidationReport validateVertexInputLayout(KeraShaderProgramHandle shader_program,
                                                               const VertexInputLayout& vertex_input) const noexcept
        {
            return isValid() ? m_api->validate_vertex_input_layout(m_renderer, shader_program, vertex_input.view())
                             : KeraRendererValidationReport{};
        }

        bool destroyGraphicsPipeline(KeraGraphicsPipelineHandle pipeline) noexcept
        {
            return isValid() && m_api->destroy_graphics_pipeline(m_renderer, pipeline) != 0;
        }

        KeraDescriptorSetHandle createDescriptorSet(KeraGraphicsPipelineHandle pipeline, uint32_t set = 0) noexcept
        {
            return isValid() ? m_api->create_descriptor_set(m_renderer, pipeline, set) : KeraDescriptorSetHandle{};
        }

        bool destroyDescriptorSet(KeraDescriptorSetHandle set) noexcept
        {
            return isValid() && m_api->destroy_descriptor_set(m_renderer, set) != 0;
        }

        DescriptorSetWriter updateDescriptors(KeraDescriptorSetHandle set) noexcept;
        KeraFrameHandle beginFrame() noexcept
        {
            return isValid() ? m_api->begin_frame(m_renderer) : KeraFrameHandle{};
        }

        void beginRenderPass(KeraFrameHandle frame, const KeraRenderPassDesc& desc) noexcept
        {
            if (isValid()) m_api->begin_render_pass(m_renderer, frame, &desc);
        }

        void beginRenderPass(KeraFrameHandle frame, KeraRenderTargetHandle target,
                             const KeraRenderPassDesc& desc) noexcept
        {
            if (isValid()) m_api->begin_render_pass_target(m_renderer, frame, target, &desc);
        }

        void endRenderPass(KeraFrameHandle frame) noexcept
        {
            if (isValid()) m_api->end_render_pass(m_renderer, frame);
        }

        void bindPipeline(KeraFrameHandle frame, KeraGraphicsPipelineHandle pipeline) noexcept
        {
            if (isValid()) m_api->bind_pipeline(m_renderer, frame, pipeline);
        }

        void bindVertexBuffer(KeraFrameHandle frame, uint32_t slot, KeraBufferHandle buffer, size_t offset = 0) noexcept
        {
            if (isValid()) m_api->bind_vertex_buffer(m_renderer, frame, slot, buffer, offset);
        }

        void bindIndexBuffer(KeraFrameHandle frame, KeraBufferHandle buffer, EIndexFormat format,
                             size_t offset = 0) noexcept
        {
            if (isValid())
                m_api->bind_index_buffer(m_renderer, frame, buffer, static_cast<KeraIndexFormat>(format), offset);
        }

        void bindDescriptorSet(KeraFrameHandle frame, KeraGraphicsPipelineHandle pipeline,
                               KeraDescriptorSetHandle set) noexcept
        {
            if (isValid()) m_api->bind_descriptor_set(m_renderer, frame, pipeline, 0, set);
        }

        void drawIndexed(KeraFrameHandle frame, uint32_t index_count, uint32_t instance_count = 1) noexcept
        {
            if (isValid()) m_api->draw_indexed(m_renderer, frame, index_count, instance_count);
        }

        bool endFrame(KeraFrameHandle frame) noexcept
        {
            return isValid() && m_api->end_frame(m_renderer, frame) != 0;
        }

        bool loadGltfModel(const KeraGltfLoadDesc& desc, KeraGltfLoadedModel& out_model) noexcept
        {
            return isValid() && m_api->load_gltf_model(m_renderer, &desc, &out_model) != 0;
        }

        void destroyGltfModel(KeraGltfLoadedModel& model) noexcept
        {
            if (isValid()) m_api->destroy_gltf_model(m_renderer, &model);
        }

        bool loadIblEnvironment(const IblEnvironmentLoadDesc& desc, KeraIblEnvironment& out_environment) noexcept
        {
            return isValid() && m_api->load_ibl_environment(m_renderer, &desc, &out_environment) != 0;
        }

        void destroyIblEnvironment(KeraIblEnvironment& env) noexcept
        {
            if (isValid()) m_api->destroy_ibl_environment(m_renderer, &env);
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
            m_success = m_success && m_renderer->api()->update_descriptor_buffer(
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
            m_success = m_success && m_renderer->api()->update_descriptor_texture(m_renderer->native(), m_set,
                                                                                stringView(name), texture) != 0;
            return *this;
        }

        DescriptorSetWriter& sampler(const char* name, KeraSamplerHandle sampler) noexcept
        {
            m_success = m_success && m_renderer->api()->update_descriptor_sampler(m_renderer->native(), m_set,
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
