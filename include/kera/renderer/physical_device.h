#pragma once

#include <memory>
#include <vector>
#include <string>
#include <vulkan/vulkan.h>

namespace kera {

struct QueueFamilyIndices {
    int graphicsFamily = -1;
    int presentFamily = -1;

    bool isComplete() const {
        return graphicsFamily >= 0 && presentFamily >= 0;
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class PhysicalDevice {
public:
    PhysicalDevice();
    ~PhysicalDevice() = default;

    bool initialize(VkInstance instance, VkSurfaceKHR surface);
    void shutdown();

    VkPhysicalDevice getVulkanPhysicalDevice() const { return physical_device_; }
    const VkPhysicalDeviceProperties& getProperties() const { return properties_; }
    const VkPhysicalDeviceFeatures& getFeatures() const { return features_; }

    const QueueFamilyIndices& getQueueFamilyIndices() const { return queue_families_; }
    const SwapChainSupportDetails& getSwapChainSupport() const { return swap_chain_support_; }

    bool isSuitable() const;
    std::string getDeviceName() const;

private:
    void pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) const;
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) const;

    VkPhysicalDevice physical_device_;
    VkPhysicalDeviceProperties properties_;
    VkPhysicalDeviceFeatures features_;
    QueueFamilyIndices queue_families_;
    SwapChainSupportDetails swap_chain_support_;
};

} // namespace kera