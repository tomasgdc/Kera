#include "kera/renderer/swapchain.h"
#include "kera/renderer/physical_device.h"
#include "kera/renderer/device.h"
#include <vulkan/vulkan.h>
#include <algorithm>
#include <iostream>

namespace kera {

SwapChain::SwapChain()
    : swap_chain_(VK_NULL_HANDLE)
    , image_format_(VK_FORMAT_UNDEFINED) {
}

SwapChain::~SwapChain() {
    shutdown();
}

SwapChain::SwapChain(SwapChain&& other) noexcept
    : swap_chain_(other.swap_chain_)
    , image_format_(other.image_format_)
    , extent_(other.extent_)
    , images_(std::move(other.images_))
    , image_views_(std::move(other.image_views_)) {
    other.swap_chain_ = VK_NULL_HANDLE;
    other.image_format_ = VK_FORMAT_UNDEFINED;
}

SwapChain& SwapChain::operator=(SwapChain&& other) noexcept {
    if (this != &other) {
        shutdown();
        swap_chain_ = other.swap_chain_;
        image_format_ = other.image_format_;
        extent_ = other.extent_;
        images_ = std::move(other.images_);
        image_views_ = std::move(other.image_views_);

        other.swap_chain_ = VK_NULL_HANDLE;
        other.image_format_ = VK_FORMAT_UNDEFINED;
    }
    return *this;
}

bool SwapChain::initialize(const PhysicalDevice& physicalDevice, const Device& device, VkSurfaceKHR surface, uint32_t width, uint32_t height) {
    if (swap_chain_) {
        shutdown();
    }

    //VkPhysicalDevice vkPhysicalDevice = physicalDevice.getVulkanPhysicalDevice();
    VkDevice vkDevice = device.getVulkanDevice();
    const auto& swapChainSupport = physicalDevice.getSwapChainSupport();

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, width, height);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
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
    uint32_t queueFamilyIndices[] = {
        static_cast<uint32_t>(indices.graphicsFamily),
        static_cast<uint32_t>(indices.presentFamily)
    };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    VkResult result = vkCreateSwapchainKHR(vkDevice, &createInfo, nullptr, &swap_chain_);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create swap chain: " << result << std::endl;
        return false;
    }

    // Get swap chain images
    vkGetSwapchainImagesKHR(vkDevice, swap_chain_, &imageCount, nullptr);
    images_.resize(imageCount);
    vkGetSwapchainImagesKHR(vkDevice, swap_chain_, &imageCount, images_.data());

    image_format_ = surfaceFormat.format;
    extent_ = extent;

    if (!createImageViews()) {
        std::cerr << "Failed to create image views" << std::endl;
        shutdown();
        return false;
    }

    std::cout << "Swap chain created with " << images_.size() << " images" << std::endl;
    return true;
}

void SwapChain::shutdown() {
    image_views_.clear();
    images_.clear();

    if (swap_chain_) {
        // TODO: Need device reference to destroy
        // vkDestroySwapchainKHR(device, swap_chain_, nullptr);
        swap_chain_ = VK_NULL_HANDLE;
        std::cout << "Swap chain destroyed" << std::endl;
    }
}

VkSurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height) const {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        VkExtent2D actualExtent = {width, height};
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }
}

bool SwapChain::createImageViews() {
    // TODO: Implement image view creation
    // This requires device reference
    image_views_.resize(images_.size());
    return true;
}

} // namespace kera