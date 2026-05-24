#include "kera/renderer/device.h"

#include "kera/renderer/physical_device.h"

#include <vulkan/vulkan.h>

#include <iostream>
#include <set>

namespace kera
{

    namespace
    {

        // Required device extensions
        const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    }  // anonymous namespace

    Device::Device()
        : physical_device_(VK_NULL_HANDLE)
        , device_(VK_NULL_HANDLE)
        , graphics_queue_(VK_NULL_HANDLE)
        , present_queue_(VK_NULL_HANDLE)
        , command_pool_(VK_NULL_HANDLE)
        , graphics_queue_family_index_(0)
        , synchronization2_enabled_(false)
    {
    }

    Device::~Device()
    {
        shutdown();
    }

    Device::Device(Device&& other) noexcept
        : physical_device_(other.physical_device_)
        , device_(other.device_)
        , graphics_queue_(other.graphics_queue_)
        , present_queue_(other.present_queue_)
        , command_pool_(other.command_pool_)
        , graphics_queue_family_index_(other.graphics_queue_family_index_)
        , synchronization2_enabled_(other.synchronization2_enabled_)
    {
        other.physical_device_ = VK_NULL_HANDLE;
        other.device_ = VK_NULL_HANDLE;
        other.graphics_queue_ = VK_NULL_HANDLE;
        other.present_queue_ = VK_NULL_HANDLE;
        other.command_pool_ = VK_NULL_HANDLE;
        other.graphics_queue_family_index_ = 0;
        other.synchronization2_enabled_ = false;
    }

    Device& Device::operator=(Device&& other) noexcept
    {
        if (this != &other)
        {
            shutdown();
            physical_device_ = other.physical_device_;
            device_ = other.device_;
            graphics_queue_ = other.graphics_queue_;
            present_queue_ = other.present_queue_;
            command_pool_ = other.command_pool_;
            graphics_queue_family_index_ = other.graphics_queue_family_index_;
            synchronization2_enabled_ = other.synchronization2_enabled_;

            other.physical_device_ = VK_NULL_HANDLE;
            other.device_ = VK_NULL_HANDLE;
            other.graphics_queue_ = VK_NULL_HANDLE;
            other.present_queue_ = VK_NULL_HANDLE;
            other.command_pool_ = VK_NULL_HANDLE;
            other.graphics_queue_family_index_ = 0;
            other.synchronization2_enabled_ = false;
        }
        return *this;
    }

    bool Device::initialize(const PhysicalDevice& physicalDevice)
    {
        if (device_)
        {
            shutdown();
        }

        VkPhysicalDevice vkPhysicalDevice = physicalDevice.getVulkanPhysicalDevice();
        const auto& queueFamilies = physicalDevice.getQueueFamilyIndices();
        physical_device_ = vkPhysicalDevice;
        graphics_queue_family_index_ = static_cast<uint32_t>(queueFamilies.graphicsFamily);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {static_cast<uint32_t>(queueFamilies.graphicsFamily),
                                                  static_cast<uint32_t>(queueFamilies.presentFamily)};

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceSynchronization2Features sync2Features{};
        sync2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;

        VkPhysicalDeviceFeatures2 supportedFeatures{};
        supportedFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        supportedFeatures.pNext = &sync2Features;
        vkGetPhysicalDeviceFeatures2(vkPhysicalDevice, &supportedFeatures);

        VkPhysicalDeviceSynchronization2Features enabledSync2Features{};
        enabledSync2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
        enabledSync2Features.synchronization2 = sync2Features.synchronization2;

        VkPhysicalDeviceFeatures2 enabledFeatures{};
        enabledFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        enabledFeatures.features.samplerAnisotropy = VK_TRUE;
        enabledFeatures.pNext = enabledSync2Features.synchronization2 ? &enabledSync2Features : nullptr;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pNext = &enabledFeatures;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = nullptr;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        VkResult result = vkCreateDevice(vkPhysicalDevice, &createInfo, nullptr, &device_);
        if (result != VK_SUCCESS)
        {
            std::cerr << "Failed to create logical device: " << result << std::endl;
            return false;
        }
        synchronization2_enabled_ = enabledSync2Features.synchronization2 == VK_TRUE;

        // Get queue handles
        vkGetDeviceQueue(device_, static_cast<uint32_t>(queueFamilies.graphicsFamily), 0, &graphics_queue_);
        vkGetDeviceQueue(device_, static_cast<uint32_t>(queueFamilies.presentFamily), 0, &present_queue_);

        // Create command pool
        if (!createCommandPool())
        {
            std::cerr << "Failed to create command pool" << std::endl;
            shutdown();
            return false;
        }

        std::cout << "Logical device created successfully" << std::endl;
        return true;
    }

    void Device::shutdown()
    {
        destroyCommandPool();

        if (device_)
        {
            vkDestroyDevice(device_, nullptr);
            physical_device_ = VK_NULL_HANDLE;
            device_ = VK_NULL_HANDLE;
            graphics_queue_ = VK_NULL_HANDLE;
            present_queue_ = VK_NULL_HANDLE;
            graphics_queue_family_index_ = 0;
            synchronization2_enabled_ = false;
            std::cout << "Logical device destroyed" << std::endl;
        }
    }

    bool Device::createCommandPool()
    {
        if (!device_)
        {
            return false;
        }

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphics_queue_family_index_;

        VkResult result = vkCreateCommandPool(device_, &poolInfo, nullptr, &command_pool_);
        if (result != VK_SUCCESS)
        {
            std::cerr << "Failed to create command pool: " << result << std::endl;
            return false;
        }

        return true;
    }

    void Device::destroyCommandPool()
    {
        if (command_pool_ && device_)
        {
            vkDestroyCommandPool(device_, command_pool_, nullptr);
            command_pool_ = VK_NULL_HANDLE;
        }
    }

}  // namespace kera
