// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

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

        bool initialize(const Device& device, const RenderPass& render_pass, const SwapChain& swap_chain);
        bool initializeSingleColorTarget(const Device& device, const RenderPass& render_pass, VkImageView color_image_view,
                                         VkExtent2D extent, VkImageView depth_image_view = VK_NULL_HANDLE);
        void shutdown();

        const std::vector<VkFramebuffer>& getVulkanFramebuffers() const
        {
            return m_framebuffers;
        }
        VkFramebuffer getFramebuffer(uint32_t index) const;
        uint32_t getFramebufferCount() const
        {
            return static_cast<uint32_t>(m_framebuffers.size());
        }

        bool isValid() const
        {
            return !m_framebuffers.empty();
        }

    private:
        VkDevice m_device;
        std::vector<VkFramebuffer> m_framebuffers;
    };

}  // namespace kera
