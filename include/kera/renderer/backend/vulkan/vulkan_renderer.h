#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

#include "kera/renderer/buffer.h"
#include "kera/renderer/interfaces.h"
#include "kera/renderer/pipeline.h"
#include "kera/renderer/resource_registry.h"
#include "kera/renderer/shader.h"

namespace kera {

class Window;
class Instance;
class PhysicalDevice;
class Device;
class Surface;
class SwapChain;
class RenderPass;
class Framebuffer;
class CommandBuffer;

struct VulkanShaderModuleResource {
  Shader m_shader;
  ShaderStage m_stage = ShaderStage::Vertex;
};

struct VulkanShaderProgramResource {
  std::vector<Shader> m_shaders;
};

struct VulkanBufferResource {
  Buffer m_buffer;
};

struct VulkanGraphicsPipelineResource {
  Pipeline m_pipeline;
  GraphicsPipelineDesc m_desc;
  ShaderProgramHandle m_program;
};

struct VulkanFrameResource {
  VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
  VkRenderPass m_renderPass = VK_NULL_HANDLE;
  VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
  VkExtent2D m_extent{};
  uint32_t m_imageIndex = 0;
};

class VulkanRenderer : public IRenderer {
 public:
  VulkanRenderer();
  ~VulkanRenderer() override;

  VulkanRenderer(const VulkanRenderer&) = delete;
  VulkanRenderer& operator=(const VulkanRenderer&) = delete;

  bool initialize(Window& window);
  void shutdown() override;

  GraphicsBackend getBackend() const override {
    return GraphicsBackend::Vulkan;
  }
  Extent2D getDrawableExtent() const override;
  bool resize(Extent2D newExtent) override;

  ShaderModuleHandle createShaderModule(const ShaderModuleDesc& desc) override;
  bool destroyShaderModule(ShaderModuleHandle module) override;

  ShaderProgramHandle createShaderProgram(
      const ShaderProgramDesc& desc) override;
  bool destroyShaderProgram(ShaderProgramHandle program) override;

  BufferHandle createBuffer(const BufferDesc& desc) override;
  bool destroyBuffer(BufferHandle buffer) override;
  bool uploadBuffer(BufferHandle buffer, const void* data, std::size_t size,
                    std::size_t offset = 0) override;

  GraphicsPipelineHandle createGraphicsPipeline(
      const GraphicsPipelineDesc& desc, ShaderProgramHandle program) override;
  bool destroyGraphicsPipeline(GraphicsPipelineHandle pipeline) override;

  FrameHandle beginFrame() override;
  void beginRenderPass(FrameHandle frame, const RenderPassDesc& desc) override;
  void endRenderPass(FrameHandle frame) override;
  void bindPipeline(FrameHandle frame,
                    GraphicsPipelineHandle pipeline) override;
  void bindVertexBuffer(FrameHandle frame, uint32_t slot, BufferHandle buffer,
                        std::size_t offset = 0) override;
  void bindIndexBuffer(FrameHandle frame, BufferHandle buffer,
                       IndexFormat format, std::size_t offset = 0) override;
  void drawIndexed(FrameHandle frame, uint32_t indexCount,
                   uint32_t instanceCount = 1) override;
  bool endFrame(FrameHandle frame) override;

 private:
  bool recreateSwapchainResources(uint32_t width, uint32_t height);
  bool recreateLiveGraphicsPipelines();
  void waitForDeviceIdle();
  void destroySyncObjects();
  bool createSyncObjects();

  Window* m_window;
  std::shared_ptr<Instance> m_instance;
  std::shared_ptr<PhysicalDevice> m_physicalDevice;
  std::shared_ptr<Device> m_device;
  std::shared_ptr<Surface> m_surface;
  std::shared_ptr<SwapChain> m_swapchain;
  std::unique_ptr<RenderPass> m_renderPass;
  std::unique_ptr<Framebuffer> m_framebuffer;
  std::unique_ptr<CommandBuffer> m_commandBuffer;

  VkSemaphore m_imageAvailableSemaphore;
  std::vector<VkSemaphore> m_renderFinishedSemaphores;
  VkFence m_inFlightFence;

  ResourceRegistry<VulkanShaderModuleResource, ShaderModuleHandle>
      m_shaderModules;
  ResourceRegistry<VulkanShaderProgramResource, ShaderProgramHandle>
      m_shaderPrograms;
  ResourceRegistry<VulkanBufferResource, BufferHandle> m_buffers;
  ResourceRegistry<VulkanGraphicsPipelineResource, GraphicsPipelineHandle>
      m_graphicsPipelines;
  ResourceRegistry<VulkanFrameResource, FrameHandle> m_frames;
};

}  // namespace kera
