#pragma once

#include "kera/renderer/interfaces.h"
#include <memory>
#include <vector>
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
    void shutdown() override;

    GraphicsBackend getBackend() const override { return GraphicsBackend::Vulkan; }
    Extent2D getDrawableExtent() const override;
    bool resize(Extent2D newExtent) override;

    std::shared_ptr<IShaderModule> createShaderModule(const ShaderModuleDesc& desc) override;
    std::shared_ptr<IShaderProgram> createShaderProgram(const ShaderProgramDesc& desc) override;
    std::shared_ptr<IBuffer> createBuffer(const BufferDesc& desc) override;
    std::shared_ptr<IGraphicsPipeline> createGraphicsPipeline(
        const GraphicsPipelineDesc& desc,
        IShaderProgram& program) override;

    std::unique_ptr<IFrame> beginFrame() override;
    bool endFrame(std::unique_ptr<IFrame> frame) override;

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
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    VkFence inFlightFence_;
};

} // namespace kera
