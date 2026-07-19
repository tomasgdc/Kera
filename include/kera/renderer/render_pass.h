// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

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

        bool initialize(const Device& device, const SwapChain& swap_chain);
        bool initializeColorTarget(const Device& device, VkFormat color_format,
                                   VkFormat depth_format = VK_FORMAT_UNDEFINED);
        void shutdown();

        VkRenderPass getVulkanRenderPass() const
        {
            return m_render_pass;
        }
        bool isValid() const
        {
            return m_render_pass != VK_NULL_HANDLE;
        }

    private:
        VkDevice m_device;
        VkRenderPass m_render_pass;
    };

}  // namespace kera
