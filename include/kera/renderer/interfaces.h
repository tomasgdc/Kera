// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/renderer/descriptors.h"
#include "kera/renderer/reflection_contracts.h"
#include "kera/renderer/result.h"
#include "kera/renderer/slang_reflection.h"
#include "kera/types.h"

#include <SDL3/SDL_events.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace kera
{
    class DescriptorSetWriter;

    struct RendererResourceStats
    {
        uint32_t shader_modules = 0;
        uint32_t shader_programs = 0;
        uint32_t buffers = 0;
        uint32_t textures = 0;
        uint32_t samplers = 0;
        uint32_t render_targets = 0;
        uint32_t graphics_pipelines = 0;
        uint32_t descriptor_sets = 0;
        uint32_t frames = 0;
    };

    struct RendererStats
    {
        RendererResourceStats resources;
        uint32_t draw_calls_this_frame = 0;
        uint32_t pipelines_bound_this_frame = 0;
        uint32_t descriptor_sets_bound_this_frame = 0;
        uint32_t vertex_buffers_bound_this_frame = 0;
        uint32_t index_buffers_bound_this_frame = 0;
        uint32_t buffer_uploads_this_frame = 0;
        uint32_t texture_uploads_this_frame = 0;
        uint32_t validation_issues_this_frame = 0;
        uint64_t frame_index = 0;
    };

    struct TextureSubresourceUpload
    {
        uint32_t mip_level = 0;
        uint32_t array_layer = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        std::size_t offset = 0;
        std::size_t size = 0;
    };

    struct TexturePrepareUpload
    {
        const void* data = nullptr;
        std::size_t size = 0;
        const TextureSubresourceUpload* subresources = nullptr;
        std::size_t subresource_count = 0;
    };

    struct UniformRingBufferInfo
    {
        std::size_t element_size = 0;
        std::size_t slot_stride = 0;
        uint32_t slot_count = 0;
    };

    class IRenderer
    {
    public:
        virtual ~IRenderer() = default;

        virtual EGraphicsBackend getBackend() const = 0;
        virtual Extent2D getDrawableExtent() const = 0;
        virtual RendererStats getStats() const = 0;
        virtual void shutdown() = 0;
        // Recreates swapchain-dependent backend state. Renderer-owned buffers and
        // shader programs remain valid; live graphics pipelines are recreated.
        virtual bool resize(Extent2D new_extent) = 0;
        virtual bool initializeUi() = 0;
        virtual void shutdownUi() = 0;
        virtual void handleUiEvent(const SDL_Event& event) = 0;
        virtual void beginUi() = 0;
        virtual void endUi() = 0;
        virtual void renderUi(FrameHandle frame) = 0;
        virtual bool isUiAvailable() const = 0;

        virtual ShaderModuleHandle createShaderModule(const ShaderModuleDesc& desc) = 0;
        virtual bool destroyShaderModule(ShaderModuleHandle module) = 0;

        virtual ShaderProgramHandle createShaderProgram(const ShaderProgramDesc& desc) = 0;
        virtual ShaderProgramHandle createGraphicsShaderProgram(const GraphicsShaderProgramDesc& desc) = 0;
        virtual const SlangReflectionMetadata* getShaderProgramReflection(ShaderProgramHandle program) const = 0;
        virtual std::vector<SlangReflectionEntryPoint> getShaderProgramEntryPoints(ShaderProgramHandle program) const;
        virtual std::vector<SlangReflectionBinding> getShaderProgramDescriptorBindings(
            ShaderProgramHandle program) const;
        virtual std::vector<SlangReflectionInput> getShaderProgramVertexInputs(ShaderProgramHandle program,
                                                                               const std::string& entry_point) const;
        virtual bool destroyShaderProgram(ShaderProgramHandle program) = 0;

        virtual BufferHandle createBuffer(const BufferDesc& desc) = 0;
        virtual bool destroyBuffer(BufferHandle buffer) = 0;
        virtual bool mapBuffer(BufferHandle buffer_handle, void** data) = 0;
        virtual void unmapBuffer(BufferHandle buffer_handle) = 0;
        virtual bool uploadBuffer(BufferHandle buffer, const void* data, std::size_t size, std::size_t offset = 0) = 0;
        virtual BufferHandle createUniformRingBuffer(std::size_t element_size, uint32_t slot_count = 0) = 0;
        virtual bool uploadUniformRingBuffer(BufferHandle buffer, FrameHandle frame, const void* data,
                                             std::size_t size) = 0;
        virtual UniformRingBufferInfo getUniformRingBufferInfo(BufferHandle buffer) const = 0;
        virtual uint32_t getUniformRingBufferSlot(BufferHandle buffer, FrameHandle frame) const = 0;
        virtual std::size_t getUniformRingBufferSlotOffset(BufferHandle buffer, uint32_t slot) const = 0;

        virtual TextureHandle createTexture(const TextureDesc& desc) = 0;
        virtual bool beginUploadBatch() = 0;
        virtual bool endUploadBatch() = 0;
        virtual void cancelUploadBatch() = 0;
        virtual bool uploadTexture(TextureHandle texture, const void* data, std::size_t size) = 0;
        virtual bool uploadTextureSubresource(TextureHandle texture, const TexturePrepareUpload& upload) = 0;
        virtual bool destroyTexture(TextureHandle texture) = 0;
        virtual SamplerHandle createSampler(const SamplerDesc& desc) = 0;
        virtual bool destroySampler(SamplerHandle sampler) = 0;
        virtual RenderTargetHandle createRenderTarget(const RenderTargetDesc& desc) = 0;
        virtual bool destroyRenderTarget(RenderTargetHandle render_target) = 0;

        virtual GraphicsPipelineHandle createGraphicsPipeline(const GraphicsPipelineDesc& desc,
                                                              ShaderProgramHandle program) = 0;
        virtual GraphicsPipelineHandle createGraphicsPipeline(const GraphicsPipelineCreateDesc& desc) = 0;
        virtual std::vector<DescriptorSetLayoutDesc> getGraphicsPipelineDescriptorSets(
            GraphicsPipelineHandle pipeline) const = 0;
        virtual VertexLayoutDesc getGraphicsPipelineVertexLayout(GraphicsPipelineHandle pipeline) const;
        virtual bool destroyGraphicsPipeline(GraphicsPipelineHandle pipeline) = 0;
        virtual DescriptorSetHandle createDescriptorSet(GraphicsPipelineHandle pipeline) = 0;
        virtual DescriptorSetHandle createDescriptorSet(GraphicsPipelineHandle pipeline, uint32_t set) = 0;
        virtual bool updateDescriptorSet(DescriptorSetHandle set, uint32_t binding, BufferHandle buffer,
                                         std::size_t offset = 0, std::size_t range = 0) = 0;
        virtual bool updateDescriptorSet(DescriptorSetHandle set, uint32_t binding, TextureHandle texture) = 0;
        virtual bool updateDescriptorSet(DescriptorSetHandle set, uint32_t binding, SamplerHandle sampler) = 0;
        virtual bool updateDescriptorSet(DescriptorSetHandle set, const std::string& name, BufferHandle buffer,
                                         std::size_t offset = 0, std::size_t range = 0) = 0;
        virtual bool updateDescriptorSet(DescriptorSetHandle set, const std::string& name, TextureHandle texture) = 0;
        virtual bool updateDescriptorSet(DescriptorSetHandle set, const std::string& name, SamplerHandle sampler) = 0;
        virtual bool validateDescriptorSet(DescriptorSetHandle set) const;
        virtual bool validateGraphicsPipelineDescriptorSets(GraphicsPipelineHandle pipeline) const;
        virtual RendererValidationReport validateDescriptorSetDetailed(DescriptorSetHandle set) const;
        virtual RendererValidationReport validateGraphicsPipelineDescriptorSetsDetailed(
            GraphicsPipelineHandle pipeline) const;
        virtual bool destroyDescriptorSet(DescriptorSetHandle set) = 0;

        virtual bool setDebugName(BufferHandle buffer, const std::string& name);
        virtual bool setDebugName(TextureHandle texture, const std::string& name);
        virtual bool setDebugName(SamplerHandle sampler, const std::string& name);
        virtual bool setDebugName(GraphicsPipelineHandle pipeline, const std::string& name);
        virtual bool setDebugName(DescriptorSetHandle descriptor_set, const std::string& name);

        virtual FrameHandle beginFrame() = 0;
        virtual void beginRenderPass(FrameHandle frame, const RenderPassDesc& desc) = 0;
        virtual void beginRenderPass(FrameHandle frame, RenderTargetHandle target, const RenderPassDesc& desc) = 0;
        virtual void endRenderPass(FrameHandle frame) = 0;
        virtual void bindPipeline(FrameHandle frame, GraphicsPipelineHandle pipeline) = 0;
        virtual void bindVertexBuffer(FrameHandle frame, uint32_t slot, BufferHandle buffer,
                                      std::size_t offset = 0) = 0;
        virtual void bindIndexBuffer(FrameHandle frame, BufferHandle buffer, EIndexFormat format,
                                     std::size_t offset = 0) = 0;
        virtual void bindDescriptorSet(FrameHandle frame, GraphicsPipelineHandle pipeline,
                                       DescriptorSetHandle descriptor_set) = 0;
        virtual void bindDescriptorSet(FrameHandle frame, GraphicsPipelineHandle pipeline, uint32_t set,
                                       DescriptorSetHandle descriptor_set) = 0;
        virtual void drawIndexed(FrameHandle frame, uint32_t index_count, uint32_t instance_count = 1) = 0;
        virtual bool endFrame(FrameHandle frame) = 0;

        RendererResult<ShaderProgramHandle> tryCreateGraphicsShaderProgram(const GraphicsShaderProgramDesc& desc);
        RendererResult<GraphicsPipelineHandle> tryCreateGraphicsPipeline(const GraphicsPipelineCreateDesc& desc);
        RendererResult<DescriptorSetHandle> tryCreateDescriptorSet(GraphicsPipelineHandle pipeline);
        RendererResult<DescriptorSetHandle> tryCreateDescriptorSet(GraphicsPipelineHandle pipeline, uint32_t set);
        DescriptorSetWriter updateDescriptors(DescriptorSetHandle set);
    };

    class DescriptorSetWriter
    {
    public:
        DescriptorSetWriter(IRenderer& renderer, DescriptorSetHandle set) : m_renderer(&renderer), m_set(set) {}

        DescriptorSetWriter& uniform(const std::string& name, BufferHandle buffer, std::size_t offset = 0,
                                     std::size_t range = 0)
        {
            const bool updated = m_renderer->updateDescriptorSet(m_set, name, buffer, offset, range);
            recordFailure(updated, "uniform buffer", name);
            m_success = updated && m_success;
            return *this;
        }

        template <typename UniformT>
        DescriptorSetWriter& uniform(const std::string& name, BufferHandle buffer, std::size_t offset = 0)
        {
            return uniform(name, buffer, offset, sizeof(UniformT));
        }

        DescriptorSetWriter& sampledImage(const std::string& name, TextureHandle texture)
        {
            const bool updated = m_renderer->updateDescriptorSet(m_set, name, texture);
            recordFailure(updated, "sampled image", name);
            m_success = updated && m_success;
            return *this;
        }

        DescriptorSetWriter& sampler(const std::string& name, SamplerHandle sampler)
        {
            const bool updated = m_renderer->updateDescriptorSet(m_set, name, sampler);
            recordFailure(updated, "sampler", name);
            m_success = updated && m_success;
            return *this;
        }

        bool ok() const
        {
            return m_success;
        }
        const std::string& errorMessage() const
        {
            return m_error_message;
        }
        const RendererValidationReport& report() const
        {
            return m_report;
        }
        RendererResult<void> result() const
        {
            return ok() ? RendererResult<void>::success()
                        : RendererResult<void>::failure(ERendererErrorCode::VALIDATION_FAILED, errorMessage());
        }

    private:
        void recordFailure(bool updated, const char* descriptor_kind, const std::string& name)
        {
            if (updated)
            {
                return;
            }

            const std::string message =
                std::string("Failed to update ") + descriptor_kind + " descriptor '" + name + "'.";
            m_report.addIssue(ERendererErrorCode::VALIDATION_FAILED, ERendererValidationCategory::DESCRIPTOR, message,
                              0, 0, name);
            if (m_error_message.empty())
            {
                m_error_message = message;
            }
        }

        IRenderer* m_renderer = nullptr;
        DescriptorSetHandle m_set;
        RendererValidationReport m_report;
        std::string m_error_message;
        bool m_success = true;
    };

    using DescriptorSetUpdate = DescriptorSetWriter;

    inline std::vector<SlangReflectionEntryPoint> IRenderer::getShaderProgramEntryPoints(
        ShaderProgramHandle program) const
    {
        const SlangReflectionMetadata* reflection = getShaderProgramReflection(program);
        return reflection ? reflection->entry_points : std::vector<SlangReflectionEntryPoint>{};
    }

    inline std::vector<SlangReflectionBinding> IRenderer::getShaderProgramDescriptorBindings(
        ShaderProgramHandle program) const
    {
        const SlangReflectionMetadata* reflection = getShaderProgramReflection(program);
        return reflection ? reflection->bindings : std::vector<SlangReflectionBinding>{};
    }

    inline std::vector<SlangReflectionInput> IRenderer::getShaderProgramVertexInputs(
        ShaderProgramHandle program, const std::string& entry_point) const
    {
        const SlangReflectionMetadata* reflection = getShaderProgramReflection(program);
        const SlangReflectionEntryPoint* reflected_entry_point =
            reflection ? reflection->findEntryPoint(entry_point) : nullptr;
        return reflected_entry_point ? reflected_entry_point->inputs : std::vector<SlangReflectionInput>{};
    }

    inline VertexLayoutDesc IRenderer::getGraphicsPipelineVertexLayout(GraphicsPipelineHandle) const
    {
        return {};
    }

    inline bool IRenderer::validateDescriptorSet(DescriptorSetHandle) const
    {
        return false;
    }

    inline bool IRenderer::validateGraphicsPipelineDescriptorSets(GraphicsPipelineHandle) const
    {
        return false;
    }

    inline RendererValidationReport IRenderer::validateDescriptorSetDetailed(DescriptorSetHandle) const
    {
        RendererValidationReport report;
        report.addIssue(ERendererErrorCode::UNSUPPORTED, ERendererValidationCategory::DESCRIPTOR,
                        "Detailed descriptor-set validation is not supported by this renderer backend.");
        return report;
    }

    inline RendererValidationReport IRenderer::validateGraphicsPipelineDescriptorSetsDetailed(
        GraphicsPipelineHandle) const
    {
        RendererValidationReport report;
        report.addIssue(ERendererErrorCode::UNSUPPORTED, ERendererValidationCategory::DESCRIPTOR,
                        "Detailed graphics-pipeline descriptor validation is not supported by this renderer backend.");
        return report;
    }

    inline bool IRenderer::setDebugName(BufferHandle, const std::string&)
    {
        return false;
    }

    inline bool IRenderer::setDebugName(TextureHandle, const std::string&)
    {
        return false;
    }

    inline bool IRenderer::setDebugName(SamplerHandle, const std::string&)
    {
        return false;
    }

    inline bool IRenderer::setDebugName(GraphicsPipelineHandle, const std::string&)
    {
        return false;
    }

    inline bool IRenderer::setDebugName(DescriptorSetHandle, const std::string&)
    {
        return false;
    }

    inline RendererResult<ShaderProgramHandle> IRenderer::tryCreateGraphicsShaderProgram(
        const GraphicsShaderProgramDesc& desc)
    {
        ShaderProgramHandle program = createGraphicsShaderProgram(desc);
        if (!program.isValid())
        {
            return RendererResult<ShaderProgramHandle>::failure("Failed to create graphics shader program.");
        }
        return RendererResult<ShaderProgramHandle>::success(program);
    }

    inline RendererResult<GraphicsPipelineHandle> IRenderer::tryCreateGraphicsPipeline(
        const GraphicsPipelineCreateDesc& desc)
    {
        GraphicsPipelineHandle pipeline = createGraphicsPipeline(desc);
        if (!pipeline.isValid())
        {
            return RendererResult<GraphicsPipelineHandle>::failure("Failed to create graphics pipeline.");
        }
        return RendererResult<GraphicsPipelineHandle>::success(pipeline);
    }

    inline RendererResult<DescriptorSetHandle> IRenderer::tryCreateDescriptorSet(GraphicsPipelineHandle pipeline)
    {
        DescriptorSetHandle descriptor_set = createDescriptorSet(pipeline);
        if (!descriptor_set.isValid())
        {
            return RendererResult<DescriptorSetHandle>::failure("Failed to create descriptor set.");
        }
        return RendererResult<DescriptorSetHandle>::success(descriptor_set);
    }

    inline RendererResult<DescriptorSetHandle> IRenderer::tryCreateDescriptorSet(GraphicsPipelineHandle pipeline,
                                                                                 uint32_t set)
    {
        DescriptorSetHandle descriptor_set = createDescriptorSet(pipeline, set);
        if (!descriptor_set.isValid())
        {
            return RendererResult<DescriptorSetHandle>::failure("Failed to create descriptor set.");
        }
        return RendererResult<DescriptorSetHandle>::success(descriptor_set);
    }

    inline DescriptorSetWriter IRenderer::updateDescriptors(DescriptorSetHandle set)
    {
        return DescriptorSetWriter(*this, set);
    }

}  // namespace kera
