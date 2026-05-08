#include "kera/renderer/swapchain.h"
#include "kera/renderer/physical_device.h"
#include "kera/renderer/device.h"
#include <vulkan/vulkan.h>
#include <algorithm>
#include <iostream>
#include <limits>

namespace kera 
{
    SwapChain::SwapChain()
        : device_(VK_NULL_HANDLE)
        , swap_chain_(VK_NULL_HANDLE)
        , image_format_(VK_FORMAT_UNDEFINED)
        , extent_({ 0, 0 })
    {
    }

    SwapChain::~SwapChain() 
    {
        shutdown();
    }

    SwapChain::SwapChain(SwapChain&& other) noexcept
        : device_(other.device_)
        , swap_chain_(other.swap_chain_)
        , image_format_(other.image_format_)
        , extent_(other.extent_)
        , images_(std::move(other.images_))
        , image_views_(std::move(other.image_views_)) 
    {
        other.device_ = VK_NULL_HANDLE;
        other.swap_chain_ = VK_NULL_HANDLE;
        other.image_format_ = VK_FORMAT_UNDEFINED;
    }

    SwapChain& SwapChain::operator=(SwapChain&& other) noexcept 
    {
        if (this != &other) {
            shutdown();
            device_ = other.device_;
            swap_chain_ = other.swap_chain_;
            image_format_ = other.image_format_;
            extent_ = other.extent_;
            images_ = std::move(other.images_);
            image_views_ = std::move(other.image_views_);

            other.device_ = VK_NULL_HANDLE;
            other.swap_chain_ = VK_NULL_HANDLE;
            other.image_format_ = VK_FORMAT_UNDEFINED;
        }
        return *this;
    }

    bool SwapChain::initialize(const PhysicalDevice& physicalDevice, const Device& device, VkSurfaceKHR surface, uint32_t width, uint32_t height) 
    {
        if (swap_chain_) 
        {
            shutdown();
        }

        VkDevice vkDevice = device.getVulkanDevice();
        device_ = vkDevice;
        const auto& swapChainSupport = physicalDevice.getSwapChainSupport();

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, width, height);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) 
        {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        const auto& indices = physicalDevice.getQueueFamilyIndices();
        uint32_t queueFamilyIndices[] = 
        {
            static_cast<uint32_t>(indices.graphicsFamily),
            static_cast<uint32_t>(indices.presentFamily)
        };

        if (indices.graphicsFamily != indices.presentFamily) 
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else 
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        VkResult result = vkCreateSwapchainKHR(vkDevice, &createInfo, nullptr, &swap_chain_);
        if (result != VK_SUCCESS) 
        {
            std::cerr << "Failed to create swap chain: " << result << std::endl;
            return false;
        }

        // Get swap chain images
        vkGetSwapchainImagesKHR(vkDevice, swap_chain_, &imageCount, nullptr);
        images_.resize(imageCount);
        vkGetSwapchainImagesKHR(vkDevice, swap_chain_, &imageCount, images_.data());

        image_format_ = surfaceFormat.format;
        extent_ = extent;

        if (!createImageViews()) 
        {
            std::cerr << "Failed to create image views" << std::endl;
            shutdown();
            return false;
        }

        std::cout << "Swap chain created with " << images_.size() << " images" << std::endl;
        return true;
    }

    void SwapChain::shutdown() 
    {
        for (VkImageView imageView : image_views_) 
        {
            if (imageView != VK_NULL_HANDLE) 
            {
                vkDestroyImageView(device_, imageView, nullptr);
            }
        }

        image_views_.clear();
        images_.clear();

        if (swap_chain_) 
        {
            vkDestroySwapchainKHR(device_, swap_chain_, nullptr);
            swap_chain_ = VK_NULL_HANDLE;
            std::cout << "Swap chain destroyed" << std::endl;
        }
        device_ = VK_NULL_HANDLE;
    }

    VkSurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const 
    {
        for (const auto& availableFormat : availableFormats) 
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) 
            {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }

    VkPresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const {
        for (const auto& availablePresentMode : availablePresentModes) 
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) 
            {
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D SwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height) const 
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) 
        {
            return capabilities.currentExtent;
        }
        else 
        {
            VkExtent2D actualExtent = { width, height };
            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
            return actualExtent;
        }
    }

    bool SwapChain::createImageViews() 
    {
        image_views_.resize(images_.size());
        for (size_t i = 0; i < images_.size(); ++i) 
        {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = images_[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = image_format_;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            VkResult result = vkCreateImageView(device_, &createInfo, nullptr, &image_views_[i]);
            if (result != VK_SUCCESS) 
            {
                std::cerr << "Failed to create image view " << i << ": " << result << std::endl;
                return false;
            }
        }
        return true;
    }

    VkResult SwapChain::acquireNextImage(VkSemaphore imageAvailableSemaphore, VkFence fence, uint32_t* imageIndex) const 
    {
        return vkAcquireNextImageKHR(
            device_,
            swap_chain_,
            std::numeric_limits<uint64_t>::max(),
            imageAvailableSemaphore,
            fence,
            imageIndex);
    }

    VkResult SwapChain::present(uint32_t imageIndex, VkSemaphore renderFinishedSemaphore, VkQueue presentQueue) const 
    {
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swap_chain_;
        presentInfo.pImageIndices = &imageIndex;
        return vkQueuePresentKHR(presentQueue, &presentInfo);
    }
} // namespace kera
