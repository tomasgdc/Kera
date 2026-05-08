#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace kera
{

    class Device;
    class RenderPass;
    class SwapChain;

    class Framebuffer
    {
    public:
        Framebuffer();
        ~Framebuffer();

        // Delete copy operations
        Framebuffer(const Framebuffer&) = delete;
        Framebuffer& operator=(const Framebuffer&) = delete;

        // Move operations
        Framebuffer(Framebuffer&& other) noexcept;
        Framebuffer& operator=(Framebuffer&& other) noexcept;

        bool initialize(const Device& device, const RenderPass& renderPass, const SwapChain& swapChain);
        bool initializeSingleColorTarget(const Device& device, const RenderPass& renderPass, VkImageView colorImageView,
                                         VkExtent2D extent);
        void shutdown();

        const std::vector<VkFramebuffer>& getVulkanFramebuffers() const
        {
            return framebuffers_;
        }
        VkFramebuffer getFramebuffer(uint32_t index) const;
        uint32_t getFramebufferCount() const
        {
            return static_cast<uint32_t>(framebuffers_.size());
        }

        bool isValid() const
        {
            return !framebuffers_.empty();
        }

    private:
        VkDevice device_;
        std::vector<VkFramebuffer> framebuffers_;
    };

}  // namespace kera
