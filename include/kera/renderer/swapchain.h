// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace kera
{

    class PhysicalDevice;
    class Device;

    class SwapChain
    {
    public:
        SwapChain();
        ~SwapChain();

        // Delete copy operations
        SwapChain(const SwapChain&) = delete;
        SwapChain& operator=(const SwapChain&) = delete;

        // Move operations
        SwapChain(SwapChain&& other) noexcept;
        SwapChain& operator=(SwapChain&& other) noexcept;

        bool initialize(const PhysicalDevice& physical_device, const Device& device, VkSurfaceKHR surface,
                        uint32_t width, uint32_t height);
        void shutdown();

        VkSwapchainKHR getVulkanSwapChain() const
        {
            return m_swap_chain;
        }
        VkFormat getImageFormat() const
        {
            return m_image_format;
        }
        VkExtent2D getExtent() const
        {
            return m_extent;
        }
        const std::vector<VkImage>& getImages() const
        {
            return m_images;
        }
        const std::vector<VkImageView>& getImageViews() const
        {
            return m_image_views;
        }

        uint32_t getImageCount() const
        {
            return static_cast<uint32_t>(m_images.size());
        }

        bool isValid() const
        {
            return m_swap_chain != VK_NULL_HANDLE;
        }

        VkResult acquireNextImage(VkSemaphore image_available_semaphore, VkFence fence, uint32_t* image_index) const;
        VkResult present(uint32_t image_index, VkSemaphore render_finished_semaphore, VkQueue present_queue) const;

    private:
        struct SwapChainSupportDetails
        {
            VkSurfaceCapabilitiesKHR capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> present_modes;
        };

        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) const;
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes) const;
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width,
                                    uint32_t height) const;

        bool createImageViews();

        VkDevice m_device;
        VkSwapchainKHR m_swap_chain;
        VkFormat m_image_format;
        VkExtent2D m_extent;
        std::vector<VkImage> m_images;
        std::vector<VkImageView> m_image_views;
    };

}  // namespace kera
