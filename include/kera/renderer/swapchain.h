#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace kera {

class PhysicalDevice;
class Device;

class SwapChain {
public:
    SwapChain();
    ~SwapChain();

    // Delete copy operations
    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;

    // Move operations
    SwapChain(SwapChain&& other) noexcept;
    SwapChain& operator=(SwapChain&& other) noexcept;

    bool initialize(const PhysicalDevice& physicalDevice, const Device& device, VkSurfaceKHR surface, uint32_t width, uint32_t height);
    void shutdown();

    VkSwapchainKHR getVulkanSwapChain() const { return swap_chain_; }
    VkFormat getImageFormat() const { return image_format_; }
    VkExtent2D getExtent() const { return extent_; }
    const std::vector<VkImage>& getImages() const { return images_; }
    const std::vector<VkImageView>& getImageViews() const { return image_views_; }

    uint32_t getImageCount() const { return static_cast<uint32_t>(images_.size()); }

    bool isValid() const { return swap_chain_ != VK_NULL_HANDLE; }

    VkResult acquireNextImage(VkSemaphore imageAvailableSemaphore, VkFence fence, uint32_t* imageIndex) const;
    VkResult present(uint32_t imageIndex, VkSemaphore renderFinishedSemaphore, VkQueue presentQueue) const;

private:
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height) const;

    bool createImageViews();

    VkDevice device_;
    VkSwapchainKHR swap_chain_;
    VkFormat image_format_;
    VkExtent2D extent_;
    std::vector<VkImage> images_;
    std::vector<VkImageView> image_views_;
};

} // namespace kera
