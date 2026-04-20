#pragma once

#include <memory>
#include <vector>
#include <string>
#include <vulkan/vulkan.h>

namespace kera {

class Instance {
public:
    Instance();
    ~Instance();

    // Delete copy operations
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;

    // Move operations
    Instance(Instance&& other) noexcept;
    Instance& operator=(Instance&& other) noexcept;

    bool initialize(const std::string& appName, uint32_t appVersion, bool enableValidation = true);
    void shutdown();

    VkInstance getVulkanInstance() const { return instance_; }
    bool isValid() const { return instance_ != VK_NULL_HANDLE; }

    // Extension and layer queries
    std::vector<const char*> getRequiredExtensions() const;
    std::vector<const char*> getRequiredLayers() const;

    bool isValidationEnabled() const { return validation_enabled_; }

private:
    bool createInstance(const std::string& appName, uint32_t appVersion);
    bool setupDebugMessenger();
    void destroyDebugMessenger();

    VkInstance instance_;
    VkDebugUtilsMessengerEXT debug_messenger_;
    bool validation_enabled_;
};

} // namespace kera