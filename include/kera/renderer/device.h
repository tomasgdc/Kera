#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace kera {

class PhysicalDevice;

class Device {
public:
    Device();
    ~Device();

    // Delete copy operations
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    // Move operations
    Device(Device&& other) noexcept;
    Device& operator=(Device&& other) noexcept;

    bool initialize(const PhysicalDevice& physicalDevice);
    void shutdown();

    VkDevice getVulkanDevice() const { return device_; }
    VkQueue getGraphicsQueue() const { return graphics_queue_; }
    VkQueue getPresentQueue() const { return present_queue_; }
    VkCommandPool getCommandPool() const { return command_pool_; }

    bool isValid() const { return device_ != VK_NULL_HANDLE; }

    // Command buffer management
    bool createCommandPool();
    void destroyCommandPool();

private:
    VkDevice device_;
    VkQueue graphics_queue_;
    VkQueue present_queue_;
    VkCommandPool command_pool_;
};

} // namespace kera