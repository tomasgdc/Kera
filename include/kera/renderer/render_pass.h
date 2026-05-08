#pragma once

#include <vulkan/vulkan.h>

#include <memory>

namespace kera
{

    class Device;
    class SwapChain;

    class RenderPass
    {
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
        bool initializeColorTarget(const Device& device, VkFormat colorFormat);
        void shutdown();

        VkRenderPass getVulkanRenderPass() const
        {
            return render_pass_;
        }
        bool isValid() const
        {
            return render_pass_ != VK_NULL_HANDLE;
        }

    private:
        VkDevice device_;
        VkRenderPass render_pass_;
    };

}  // namespace kera
