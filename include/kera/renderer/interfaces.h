#pragma once

#include "kera/renderer/descriptors.h"
#include "kera/types.h"
#include <cstddef>
#include <cstdint>
#include <memory>

namespace kera {

class IShaderModule {
public:
    virtual ~IShaderModule() = default;
    virtual ShaderStage getStage() const = 0;
};

class IShaderProgram {
public:
    virtual ~IShaderProgram() = default;
};

class IBuffer {
public:
    virtual ~IBuffer() = default;
    virtual std::size_t getSize() const = 0;
    virtual bool upload(const void* data, std::size_t size, std::size_t offset = 0) = 0;
};

class IGraphicsPipeline {
public:
    virtual ~IGraphicsPipeline() = default;
};

class IFrame {
public:
    virtual ~IFrame() = default;

    virtual void beginRenderPass(const RenderPassDesc& desc) = 0;
    virtual void endRenderPass() = 0;
    virtual void bindPipeline(IGraphicsPipeline& pipeline) = 0;
    virtual void bindVertexBuffer(uint32_t slot, IBuffer& buffer, std::size_t offset = 0) = 0;
    virtual void bindIndexBuffer(IBuffer& buffer, IndexFormat format, std::size_t offset = 0) = 0;
    virtual void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1) = 0;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual GraphicsBackend getBackend() const = 0;
    virtual Extent2D getDrawableExtent() const = 0;
    virtual void shutdown() = 0;
    // Recreates swapchain-dependent backend state only. Sample-owned buffers,
    // shader programs, and pipelines must remain valid across resize.
    virtual bool resize(Extent2D newExtent) = 0;

    virtual std::shared_ptr<IShaderModule> createShaderModule(const ShaderModuleDesc& desc) = 0;
    virtual std::shared_ptr<IShaderProgram> createShaderProgram(const ShaderProgramDesc& desc) = 0;
    virtual std::shared_ptr<IBuffer> createBuffer(const BufferDesc& desc) = 0;
    virtual std::shared_ptr<IGraphicsPipeline> createGraphicsPipeline(
        const GraphicsPipelineDesc& desc,
        IShaderProgram& program) = 0;

    virtual std::unique_ptr<IFrame> beginFrame() = 0;
    virtual bool endFrame(std::unique_ptr<IFrame> frame) = 0;
};

} // namespace kera
