#pragma once

#include "kera/renderer/descriptors.h"
#include "kera/types.h"

#include <SDL3/SDL_events.h>

#include <cstddef>
#include <cstdint>

namespace kera
{

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
        virtual bool destroyShaderProgram(ShaderProgramHandle program) = 0;

        virtual BufferHandle createBuffer(const BufferDesc& desc) = 0;
        virtual bool destroyBuffer(BufferHandle buffer) = 0;
        virtual bool mapBuffer(BufferHandle bufferHandle, void** data) = 0;
        virtual void unmapBuffer(BufferHandle bufferHandle) = 0;
        virtual bool uploadBuffer(BufferHandle buffer, const void* data, std::size_t size, std::size_t offset = 0) = 0;

        virtual TextureHandle createTexture(const TextureDesc& desc) = 0;
        virtual bool destroyTexture(TextureHandle texture) = 0;
        virtual SamplerHandle createSampler(const SamplerDesc& desc) = 0;
        virtual bool destroySampler(SamplerHandle sampler) = 0;
        virtual RenderTargetHandle createRenderTarget(const RenderTargetDesc& desc) = 0;
        virtual bool destroyRenderTarget(RenderTargetHandle renderTarget) = 0;

        virtual GraphicsPipelineHandle createGraphicsPipeline(const GraphicsPipelineDesc& desc,
                                                              ShaderProgramHandle program) = 0;
        virtual bool destroyGraphicsPipeline(GraphicsPipelineHandle pipeline) = 0;
        virtual DescriptorSetHandle createDescriptorSet(GraphicsPipelineHandle pipeline, uint32_t set) = 0;
        virtual bool updateDescriptorSet(DescriptorSetHandle set, uint32_t binding, BufferHandle buffer,
                                         std::size_t offset = 0, std::size_t range = 0) = 0;
        virtual bool updateDescriptorSet(DescriptorSetHandle set, uint32_t binding, TextureHandle texture) = 0;
        virtual bool updateDescriptorSet(DescriptorSetHandle set, uint32_t binding, SamplerHandle sampler) = 0;
        virtual bool destroyDescriptorSet(DescriptorSetHandle set) = 0;

        virtual FrameHandle beginFrame() = 0;
        virtual void beginRenderPass(FrameHandle frame, const RenderPassDesc& desc) = 0;
        virtual void beginRenderPass(FrameHandle frame, RenderTargetHandle target, const RenderPassDesc& desc) = 0;
        virtual void endRenderPass(FrameHandle frame) = 0;
        virtual void bindPipeline(FrameHandle frame, GraphicsPipelineHandle pipeline) = 0;
        virtual void bindVertexBuffer(FrameHandle frame, uint32_t slot, BufferHandle buffer,
                                      std::size_t offset = 0) = 0;
        virtual void bindIndexBuffer(FrameHandle frame, BufferHandle buffer, IndexFormat format,
                                     std::size_t offset = 0) = 0;
        virtual void bindDescriptorSet(FrameHandle frame, GraphicsPipelineHandle pipeline, uint32_t set,
                                       DescriptorSetHandle descriptorSet) = 0;
        virtual void drawIndexed(FrameHandle frame, uint32_t indexCount, uint32_t instanceCount = 1) = 0;
        virtual bool endFrame(FrameHandle frame) = 0;
    };

}  // namespace kera
