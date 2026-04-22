#pragma once

#include <memory>
#include <vulkan/vulkan.h>

namespace kera {

class Device;
class SwapChain;

class RenderPass {
public:
    RenderPass();
    ~RenderPass();

    // Delete copy operations
    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;

    // Move operations
    RenderPass(RenderPass&& other) noexcept;
    RenderPass& operator=(RenderPass&& other) noexcept;

    bool initialize(const Device& device, const SwapChain& swapChain);
    void shutdown();

    VkRenderPass getVulkanRenderPass() const { return render_pass_; }
    bool isValid() const { return render_pass_ != VK_NULL_HANDLE; }

private:
    VkDevice device_;
    VkRenderPass render_pass_;
};

} // namespace kera
