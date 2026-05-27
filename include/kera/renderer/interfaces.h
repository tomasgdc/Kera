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
        uint32_t shaderModules = 0;
        uint32_t shaderPrograms = 0;
        uint32_t buffers = 0;
        uint32_t textures = 0;
        uint32_t samplers = 0;
        uint32_t renderTargets = 0;
        uint32_t graphicsPipelines = 0;
        uint32_t descriptorSets = 0;
        uint32_t frames = 0;
    };

    struct RendererStats
    {
        RendererResourceStats resources;
        uint32_t drawCallsThisFrame = 0;
        uint32_t pipelinesBoundThisFrame = 0;
        uint32_t descriptorSetsBoundThisFrame = 0;
        uint32_t vertexBuffersBoundThisFrame = 0;
        uint32_t indexBuffersBoundThisFrame = 0;
        uint32_t bufferUploadsThisFrame = 0;
        uint32_t textureUploadsThisFrame = 0;
        uint32_t validationIssuesThisFrame = 0;
        uint64_t frameIndex = 0;
    };

    class IRenderer
    {
    public:
        virtual ~IRenderer() = default;

        virtual GraphicsBackend getBackend() const = 0;
        virtual Extent2D getDrawableExtent() const = 0;
        virtual RendererStats getStats() const = 0;
        virtual void shutdown() = 0;
        // Recreates swapchain-dependent backend state. Renderer-owned buffers and
        // shader programs remain valid; live graphics pipelines are recreated.
        virtual bool resize(Extent2D newExtent) = 0;
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
                                                                               const std::string& entryPoint) const;
        virtual bool destroyShaderProgram(ShaderProgramHandle program) = 0;

        virtual BufferHandle createBuffer(const BufferDesc& desc) = 0;
        virtual bool destroyBuffer(BufferHandle buffer) = 0;
        virtual bool mapBuffer(BufferHandle bufferHandle, void** data) = 0;
        virtual void unmapBuffer(BufferHandle bufferHandle) = 0;
        virtual bool uploadBuffer(BufferHandle buffer, const void* data, std::size_t size, std::size_t offset = 0) = 0;
        virtual BufferHandle createUniformRingBuffer(std::size_t elementSize, uint32_t slotCount = 0) = 0;
        virtual bool uploadUniformRingBuffer(BufferHandle buffer, FrameHandle frame, const void* data,
                                             std::size_t size) = 0;
        virtual std::size_t getUniformRingBufferOffset(BufferHandle buffer, FrameHandle frame) const = 0;

        virtual TextureHandle createTexture(const TextureDesc& desc) = 0;
        virtual bool uploadTexture(TextureHandle texture, const void* data, std::size_t size) = 0;
        virtual bool destroyTexture(TextureHandle texture) = 0;
        virtual SamplerHandle createSampler(const SamplerDesc& desc) = 0;
        virtual bool destroySampler(SamplerHandle sampler) = 0;
        virtual RenderTargetHandle createRenderTarget(const RenderTargetDesc& desc) = 0;
        virtual bool destroyRenderTarget(RenderTargetHandle renderTarget) = 0;

        virtual GraphicsPipelineHandle createGraphicsPipeline(const GraphicsPipelineDesc& desc,
                                                              ShaderProgramHandle program) = 0;
        virtual GraphicsPipelineHandle createGraphicsPipeline(const GraphicsPipelineCreateDesc& desc) = 0;
        virtual std::vector<DescriptorSetLayoutDesc> getGraphicsPipelineDescriptorSets(
            GraphicsPipelineHandle pipeline) const = 0;
        virtual VertexLayoutDesc getGraphicsPipelineVertexLayout(GraphicsPipelineHandle pipeline) const;
        virtual PipelineReflectionContract getGraphicsPipelineReflectionContract(GraphicsPipelineHandle pipeline) const;
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
        virtual bool setDebugName(DescriptorSetHandle descriptorSet, const std::string& name);

        virtual FrameHandle beginFrame() = 0;
        virtual void beginRenderPass(FrameHandle frame, const RenderPassDesc& desc) = 0;
        virtual void beginRenderPass(FrameHandle frame, RenderTargetHandle target, const RenderPassDesc& desc) = 0;
        virtual void endRenderPass(FrameHandle frame) = 0;
        virtual void bindPipeline(FrameHandle frame, GraphicsPipelineHandle pipeline) = 0;
        virtual void bindVertexBuffer(FrameHandle frame, uint32_t slot, BufferHandle buffer,
                                      std::size_t offset = 0) = 0;
        virtual void bindIndexBuffer(FrameHandle frame, BufferHandle buffer, IndexFormat format,
                                     std::size_t offset = 0) = 0;
        virtual void bindDescriptorSet(FrameHandle frame, GraphicsPipelineHandle pipeline,
                                       DescriptorSetHandle descriptorSet) = 0;
        virtual void bindDescriptorSet(FrameHandle frame, GraphicsPipelineHandle pipeline, uint32_t set,
                                       DescriptorSetHandle descriptorSet) = 0;
        virtual void drawIndexed(FrameHandle frame, uint32_t indexCount, uint32_t instanceCount = 1) = 0;
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
            return m_errorMessage;
        }
        const RendererValidationReport& report() const
        {
            return m_report;
        }
        RendererResult<void> result() const
        {
            return ok() ? RendererResult<void>::success()
                        : RendererResult<void>::failure(RendererErrorCode::ValidationFailed, errorMessage());
        }

    private:
        void recordFailure(bool updated, const char* descriptorKind, const std::string& name)
        {
            if (updated)
            {
                return;
            }

            const std::string message =
                std::string("Failed to update ") + descriptorKind + " descriptor '" + name + "'.";
            m_report.addIssue(RendererErrorCode::ValidationFailed, RendererValidationCategory::Descriptor, message, 0,
                              0, name);
            if (m_errorMessage.empty())
            {
                m_errorMessage = message;
            }
        }

        IRenderer* m_renderer = nullptr;
        DescriptorSetHandle m_set;
        RendererValidationReport m_report;
        std::string m_errorMessage;
        bool m_success = true;
    };

    using DescriptorSetUpdate = DescriptorSetWriter;

    inline std::vector<SlangReflectionEntryPoint> IRenderer::getShaderProgramEntryPoints(
        ShaderProgramHandle program) const
    {
        const SlangReflectionMetadata* reflection = getShaderProgramReflection(program);
        return reflection ? reflection->entryPoints : std::vector<SlangReflectionEntryPoint>{};
    }

    inline std::vector<SlangReflectionBinding> IRenderer::getShaderProgramDescriptorBindings(
        ShaderProgramHandle program) const
    {
        const SlangReflectionMetadata* reflection = getShaderProgramReflection(program);
        return reflection ? reflection->bindings : std::vector<SlangReflectionBinding>{};
    }

    inline std::vector<SlangReflectionInput> IRenderer::getShaderProgramVertexInputs(
        ShaderProgramHandle program, const std::string& entryPoint) const
    {
        const SlangReflectionMetadata* reflection = getShaderProgramReflection(program);
        const SlangReflectionEntryPoint* reflectedEntryPoint =
            reflection ? reflection->findEntryPoint(entryPoint) : nullptr;
        return reflectedEntryPoint ? reflectedEntryPoint->inputs : std::vector<SlangReflectionInput>{};
    }

    inline VertexLayoutDesc IRenderer::getGraphicsPipelineVertexLayout(GraphicsPipelineHandle) const
    {
        return {};
    }

    inline PipelineReflectionContract IRenderer::getGraphicsPipelineReflectionContract(GraphicsPipelineHandle) const
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
        report.addIssue(RendererErrorCode::Unsupported, RendererValidationCategory::Descriptor,
                        "Detailed descriptor-set validation is not supported by this renderer backend.");
        return report;
    }

    inline RendererValidationReport IRenderer::validateGraphicsPipelineDescriptorSetsDetailed(
        GraphicsPipelineHandle) const
    {
        RendererValidationReport report;
        report.addIssue(RendererErrorCode::Unsupported, RendererValidationCategory::Descriptor,
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
        DescriptorSetHandle descriptorSet = createDescriptorSet(pipeline);
        if (!descriptorSet.isValid())
        {
            return RendererResult<DescriptorSetHandle>::failure("Failed to create descriptor set.");
        }
        return RendererResult<DescriptorSetHandle>::success(descriptorSet);
    }

    inline RendererResult<DescriptorSetHandle> IRenderer::tryCreateDescriptorSet(GraphicsPipelineHandle pipeline,
                                                                                 uint32_t set)
    {
        DescriptorSetHandle descriptorSet = createDescriptorSet(pipeline, set);
        if (!descriptorSet.isValid())
        {
            return RendererResult<DescriptorSetHandle>::failure("Failed to create descriptor set.");
        }
        return RendererResult<DescriptorSetHandle>::success(descriptorSet);
    }

    inline DescriptorSetWriter IRenderer::updateDescriptors(DescriptorSetHandle set)
    {
        return DescriptorSetWriter(*this, set);
    }

}  // namespace kera
