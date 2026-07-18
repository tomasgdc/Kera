// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/swapchain.h"

#include "kera/renderer/device.h"
#include "kera/renderer/physical_device.h"
#include "kera/utilities/logger.h"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <limits>

namespace kera
{
    SwapChain::SwapChain()
        : m_device(VK_NULL_HANDLE), m_swap_chain(VK_NULL_HANDLE), m_image_format(VK_FORMAT_UNDEFINED), m_extent({0, 0})
    {
    }

    SwapChain::~SwapChain()
    {
        shutdown();
    }

    SwapChain::SwapChain(SwapChain&& other) noexcept
        : m_device(other.m_device)
        , m_swap_chain(other.m_swap_chain)
        , m_image_format(other.m_image_format)
        , m_extent(other.m_extent)
        , m_images(std::move(other.m_images))
        , m_image_views(std::move(other.m_image_views))
    {
        other.m_device = VK_NULL_HANDLE;
        other.m_swap_chain = VK_NULL_HANDLE;
        other.m_image_format = VK_FORMAT_UNDEFINED;
    }

    SwapChain& SwapChain::operator=(SwapChain&& other) noexcept
    {
        if (this != &other)
        {
            shutdown();
            m_device = other.m_device;
            m_swap_chain = other.m_swap_chain;
            m_image_format = other.m_image_format;
            m_extent = other.m_extent;
            m_images = std::move(other.m_images);
            m_image_views = std::move(other.m_image_views);

            other.m_device = VK_NULL_HANDLE;
            other.m_swap_chain = VK_NULL_HANDLE;
            other.m_image_format = VK_FORMAT_UNDEFINED;
        }
        return *this;
    }

    bool SwapChain::initialize(const PhysicalDevice& physical_device, const Device& device, VkSurfaceKHR surface,
                               uint32_t width, uint32_t height)
    {
        VkDevice vk_device = device.getVulkanDevice();
        m_device = vk_device;
        const auto& swap_chain_support = physical_device.getSwapChainSupport();

        VkSurfaceFormatKHR surface_format = chooseSwapSurfaceFormat(swap_chain_support.formats);
        VkPresentModeKHR present_mode = chooseSwapPresentMode(swap_chain_support.present_modes);
        VkExtent2D extent = chooseSwapExtent(swap_chain_support.capabilities, width, height);
        if (extent.width == 0 || extent.height == 0)
        {
            Logger::getInstance().debug("Skipping swap chain creation while the surface extent is zero.");
            return false;
        }

        if (m_swap_chain)
        {
            shutdown();
        }
        m_device = vk_device;

        uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
        if (swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount)
        {
            image_count = swap_chain_support.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface = surface;
        create_info.minImageCount = image_count;
        create_info.imageFormat = surface_format.format;
        create_info.imageColorSpace = surface_format.colorSpace;
        create_info.imageExtent = extent;
        create_info.imageArrayLayers = 1;
        create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        const auto& indices = physical_device.getQueueFamilyIndices();
        uint32_t queue_family_indices[] = {static_cast<uint32_t>(indices.graphics_family),
                                         static_cast<uint32_t>(indices.present_family)};

        if (indices.graphics_family != indices.present_family)
        {
            create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            create_info.queueFamilyIndexCount = 2;
            create_info.pQueueFamilyIndices = queue_family_indices;
        }
        else
        {
            create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        create_info.preTransform = swap_chain_support.capabilities.currentTransform;
        create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        create_info.presentMode = present_mode;
        create_info.clipped = VK_TRUE;

        VkResult result = vkCreateSwapchainKHR(vk_device, &create_info, nullptr, &m_swap_chain);
        if (result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to create swap chain: " + std::to_string(result));
            return false;
        }

        // Get swap chain images
        vkGetSwapchainImagesKHR(vk_device, m_swap_chain, &image_count, nullptr);
        m_images.resize(image_count);
        vkGetSwapchainImagesKHR(vk_device, m_swap_chain, &image_count, m_images.data());

        m_image_format = surface_format.format;
        m_extent = extent;

        if (!createImageViews())
        {
            Logger::getInstance().error("Failed to create image views");
            shutdown();
            return false;
        }

        Logger::getInstance().debug("Swap chain created with " + std::to_string(m_images.size()) + " images");
        return true;
    }

    void SwapChain::shutdown()
    {
        for (VkImageView image_view : m_image_views)
        {
            if (image_view != VK_NULL_HANDLE)
            {
                vkDestroyImageView(m_device, image_view, nullptr);
            }
        }

        m_image_views.clear();
        m_images.clear();

        if (m_swap_chain)
        {
            vkDestroySwapchainKHR(m_device, m_swap_chain, nullptr);
            m_swap_chain = VK_NULL_HANDLE;
            Logger::getInstance().debug("Swap chain destroyed");
        }
        m_device = VK_NULL_HANDLE;
        m_image_format = VK_FORMAT_UNDEFINED;
        m_extent = {0, 0};
    }

    VkSurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) const
    {
        for (const auto& available_format : available_formats)
        {
            if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return available_format;
            }
        }
        return available_formats[0];
    }

    VkPresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes) const
    {
        for (const auto& available_present_mode : available_present_modes)
        {
            if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return available_present_mode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D SwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width,
                                           uint32_t height) const
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }
        else
        {
            VkExtent2D actual_extent = {width, height};
            actual_extent.width =
                std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actual_extent.height =
                std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
            return actual_extent;
        }
    }

    bool SwapChain::createImageViews()
    {
        m_image_views.resize(m_images.size());
        for (size_t i = 0; i < m_images.size(); ++i)
        {
            VkImageViewCreateInfo create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            create_info.image = m_images[i];
            create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            create_info.format = m_image_format;
            create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            create_info.subresourceRange.baseMipLevel = 0;
            create_info.subresourceRange.levelCount = 1;
            create_info.subresourceRange.baseArrayLayer = 0;
            create_info.subresourceRange.layerCount = 1;

            VkResult result = vkCreateImageView(m_device, &create_info, nullptr, &m_image_views[i]);
            if (result != VK_SUCCESS)
            {
                Logger::getInstance().error("Failed to create image view " + std::to_string(i) + ": " +
                                            std::to_string(result));
                return false;
            }
        }
        return true;
    }

    VkResult SwapChain::acquireNextImage(VkSemaphore image_available_semaphore, VkFence fence, uint32_t* image_index) const
    {
        return vkAcquireNextImageKHR(m_device, m_swap_chain, std::numeric_limits<uint64_t>::max(),
                                     image_available_semaphore, fence, image_index);
    }

    VkResult SwapChain::present(uint32_t image_index, VkSemaphore render_finished_semaphore, VkQueue present_queue) const
    {
        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &render_finished_semaphore;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &m_swap_chain;
        present_info.pImageIndices = &image_index;
        return vkQueuePresentKHR(present_queue, &present_info);
    }
}  // namespace kera
