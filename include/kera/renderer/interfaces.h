#pragma once

#include <cstddef>
#include <cstdint>

#include "kera/renderer/descriptors.h"
#include "kera/types.h"

namespace kera {

template <typename Tag>
struct Handle {
  int32_t m_index = -1;
  uint32_t m_generation = 0;

  constexpr Handle() noexcept = default;
  constexpr Handle(int32_t index, uint32_t generation) noexcept
      : m_index(index), m_generation(generation) {}

  constexpr bool isValid() const noexcept { return m_index != -1; }
  constexpr explicit operator bool() const noexcept { return isValid(); }
  constexpr bool operator==(const Handle&) const noexcept = default;
};

using ShaderModuleHandle = Handle<struct ShaderModuleTag>;
using ShaderProgramHandle = Handle<struct ShaderProgramTag>;
using BufferHandle = Handle<struct BufferTag>;
using GraphicsPipelineHandle = Handle<struct GraphicsPipelineTag>;
using FrameHandle = Handle<struct FrameTag>;

class IRenderer {
 public:
  virtual ~IRenderer() = default;

  virtual GraphicsBackend getBackend() const = 0;
  virtual Extent2D getDrawableExtent() const = 0;
  virtual void shutdown() = 0;
  // Recreates swapchain-dependent backend state. Renderer-owned buffers and
  // shader programs remain valid; live graphics pipelines are recreated.
  virtual bool resize(Extent2D newExtent) = 0;

  virtual ShaderModuleHandle createShaderModule(
      const ShaderModuleDesc& desc) = 0;
  virtual bool destroyShaderModule(ShaderModuleHandle module) = 0;

  virtual ShaderProgramHandle createShaderProgram(
      const ShaderProgramDesc& desc) = 0;
  virtual bool destroyShaderProgram(ShaderProgramHandle program) = 0;

  virtual BufferHandle createBuffer(const BufferDesc& desc) = 0;
  virtual bool destroyBuffer(BufferHandle buffer) = 0;
  virtual bool uploadBuffer(BufferHandle buffer, const void* data,
                            std::size_t size, std::size_t offset = 0) = 0;

  virtual GraphicsPipelineHandle createGraphicsPipeline(
      const GraphicsPipelineDesc& desc, ShaderProgramHandle program) = 0;
  virtual bool destroyGraphicsPipeline(GraphicsPipelineHandle pipeline) = 0;

  virtual FrameHandle beginFrame() = 0;
  virtual void beginRenderPass(FrameHandle frame,
                               const RenderPassDesc& desc) = 0;
  virtual void endRenderPass(FrameHandle frame) = 0;
  virtual void bindPipeline(FrameHandle frame,
                            GraphicsPipelineHandle pipeline) = 0;
  virtual void bindVertexBuffer(FrameHandle frame, uint32_t slot,
                                BufferHandle buffer,
                                std::size_t offset = 0) = 0;
  virtual void bindIndexBuffer(FrameHandle frame, BufferHandle buffer,
                               IndexFormat format, std::size_t offset = 0) = 0;
  virtual void drawIndexed(FrameHandle frame, uint32_t indexCount,
                           uint32_t instanceCount = 1) = 0;
  virtual bool endFrame(FrameHandle frame) = 0;
};

}  // namespace kera
