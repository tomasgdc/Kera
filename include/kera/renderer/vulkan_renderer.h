#pragma once

#include "kera/renderer/interfaces.h"
#include <memory>
#include <vulkan/vulkan.h>

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

class VulkanRenderer : public IRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer() override;

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    bool initialize(Window& window);
    void shutdown();

    GraphicsBackend getBackend() const override { return GraphicsBackend::Vulkan; }
    Extent2D getDrawableExtent() const override;
    Result<void> resize(Extent2D newExtent) override;

    Result<std::shared_ptr<IShaderModule>> createShaderModule(const ShaderModuleDesc& desc) override;
    Result<std::shared_ptr<IBuffer>> createBuffer(const BufferDesc& desc) override;
    Result<std::shared_ptr<IGraphicsPipeline>> createGraphicsPipeline(
        const GraphicsPipelineDesc& desc,
        IShaderModule& vertexShader,
        IShaderModule& fragmentShader) override;

    Result<std::unique_ptr<IFrame>> beginFrame() override;
    Result<void> endFrame(std::unique_ptr<IFrame> frame) override;

private:
    bool recreateSwapchainResources(uint32_t width, uint32_t height);
    void destroySyncObjects();
    bool createSyncObjects();

    Window* window_;
    std::shared_ptr<Instance> instance_;
    std::shared_ptr<PhysicalDevice> physicalDevice_;
    std::shared_ptr<Device> device_;
    std::shared_ptr<Surface> surface_;
    std::shared_ptr<SwapChain> swapchain_;
    std::unique_ptr<RenderPass> renderPass_;
    std::unique_ptr<Framebuffer> framebuffer_;
    std::unique_ptr<CommandBuffer> commandBuffer_;

    VkSemaphore imageAvailableSemaphore_;
    VkSemaphore renderFinishedSemaphore_;
    VkFence inFlightFence_;
};

} // namespace kera
