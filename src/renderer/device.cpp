// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/device.h"

#include "kera/renderer/physical_device.h"
#include "kera/utilities/logger.h"

#include <vulkan/vulkan.h>

#include <set>

namespace kera
{

    namespace
    {

        // Required device extensions
        const std::vector<const char*> kDeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        struct VulkanRequiredFeaturePolicy
        {
            VkPhysicalDeviceFeatures2 features{};
            VkPhysicalDeviceSynchronization2Features synchronization2{};
            VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering{};
            VkPhysicalDeviceTimelineSemaphoreFeatures timeline_semaphore{};
        };

        void linkRequiredFeaturePolicy(VulkanRequiredFeaturePolicy& policy)
        {
            policy.features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            policy.synchronization2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
            policy.dynamic_rendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
            policy.timeline_semaphore.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;

            policy.features.pNext = &policy.synchronization2;
            policy.synchronization2.pNext = &policy.dynamic_rendering;
            policy.dynamic_rendering.pNext = &policy.timeline_semaphore;
        }

        bool validateRequiredFeaturePolicy(const VulkanRequiredFeaturePolicy& policy)
        {
            if (policy.synchronization2.synchronization2 != VK_TRUE)
            {
                Logger::getInstance().error("Kera requires Vulkan 1.3 synchronization2 support.");
                return false;
            }
            if (policy.dynamic_rendering.dynamicRendering != VK_TRUE)
            {
                Logger::getInstance().error("Kera requires Vulkan 1.3 dynamic rendering support.");
                return false;
            }
            if (policy.timeline_semaphore.timelineSemaphore != VK_TRUE)
            {
                Logger::getInstance().error("Kera requires Vulkan 1.3 timeline semaphore support.");
                return false;
            }
            return true;
        }

    }  // anonymous namespace

    Device::Device()
        : m_physical_device(VK_NULL_HANDLE)
        , m_device(VK_NULL_HANDLE)
        , m_graphics_queue(VK_NULL_HANDLE)
        , m_present_queue(VK_NULL_HANDLE)
        , m_command_pool(VK_NULL_HANDLE)
        , m_graphics_queue_family_index(0)
    {
    }

    Device::~Device()
    {
        shutdown();
    }

    Device::Device(Device&& other) noexcept
        : m_physical_device(other.m_physical_device)
        , m_device(other.m_device)
        , m_graphics_queue(other.m_graphics_queue)
        , m_present_queue(other.m_present_queue)
        , m_command_pool(other.m_command_pool)
        , m_graphics_queue_family_index(other.m_graphics_queue_family_index)
    {
        other.m_physical_device = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
        other.m_graphics_queue = VK_NULL_HANDLE;
        other.m_present_queue = VK_NULL_HANDLE;
        other.m_command_pool = VK_NULL_HANDLE;
        other.m_graphics_queue_family_index = 0;
    }

    Device& Device::operator=(Device&& other) noexcept
    {
        if (this != &other)
        {
            shutdown();
            m_physical_device = other.m_physical_device;
            m_device = other.m_device;
            m_graphics_queue = other.m_graphics_queue;
            m_present_queue = other.m_present_queue;
            m_command_pool = other.m_command_pool;
            m_graphics_queue_family_index = other.m_graphics_queue_family_index;

            other.m_physical_device = VK_NULL_HANDLE;
            other.m_device = VK_NULL_HANDLE;
            other.m_graphics_queue = VK_NULL_HANDLE;
            other.m_present_queue = VK_NULL_HANDLE;
            other.m_command_pool = VK_NULL_HANDLE;
            other.m_graphics_queue_family_index = 0;
        }
        return *this;
    }

    bool Device::initialize(const PhysicalDevice& physical_device)
    {
        if (m_device)
        {
            shutdown();
        }

        VkPhysicalDevice vk_physical_device = physical_device.getVulkanPhysicalDevice();
        const auto& queue_families = physical_device.getQueueFamilyIndices();
        m_physical_device = vk_physical_device;
        m_graphics_queue_family_index = static_cast<uint32_t>(queue_families.graphics_family);

        VkPhysicalDeviceProperties physical_device_properties{};
        vkGetPhysicalDeviceProperties(vk_physical_device, &physical_device_properties);
        if (physical_device_properties.apiVersion < VK_API_VERSION_1_3)
        {
            Logger::getInstance().error("Kera requires a Vulkan 1.3 physical device.");
            return false;
        }

        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        std::set<uint32_t> unique_queue_families = {static_cast<uint32_t>(queue_families.graphics_family),
                                                    static_cast<uint32_t>(queue_families.present_family)};

        float queue_priority = 1.0f;
        for (uint32_t queue_family : unique_queue_families)
        {
            VkDeviceQueueCreateInfo queue_create_info{};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = queue_family;
            queue_create_info.queueCount = 1;
            queue_create_info.pQueuePriorities = &queue_priority;
            queue_create_infos.push_back(queue_create_info);
        }

        VulkanRequiredFeaturePolicy supported_feature_policy{};
        linkRequiredFeaturePolicy(supported_feature_policy);
        vkGetPhysicalDeviceFeatures2(vk_physical_device, &supported_feature_policy.features);
        if (!validateRequiredFeaturePolicy(supported_feature_policy))
        {
            return false;
        }

        VulkanRequiredFeaturePolicy enabled_feature_policy{};
        linkRequiredFeaturePolicy(enabled_feature_policy);
        enabled_feature_policy.features.features.samplerAnisotropy = VK_TRUE;
        enabled_feature_policy.synchronization2.synchronization2 = VK_TRUE;
        enabled_feature_policy.dynamic_rendering.dynamicRendering = VK_TRUE;
        enabled_feature_policy.timeline_semaphore.timelineSemaphore = VK_TRUE;

        VkDeviceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.pNext = &enabled_feature_policy.features;
        create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        create_info.pQueueCreateInfos = queue_create_infos.data();
        create_info.pEnabledFeatures = nullptr;
        create_info.enabledExtensionCount = static_cast<uint32_t>(kDeviceExtensions.size());
        create_info.ppEnabledExtensionNames = kDeviceExtensions.data();

        VkResult result = vkCreateDevice(vk_physical_device, &create_info, nullptr, &m_device);
        if (result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to create logical device: " + std::to_string(result));
            return false;
        }

        // Get queue handles
        vkGetDeviceQueue(m_device, static_cast<uint32_t>(queue_families.graphics_family), 0, &m_graphics_queue);
        vkGetDeviceQueue(m_device, static_cast<uint32_t>(queue_families.present_family), 0, &m_present_queue);

        // Create command pool
        if (!createCommandPool())
        {
            Logger::getInstance().error("Failed to create command pool");
            shutdown();
            return false;
        }

        Logger::getInstance().debug("Logical device created successfully");
        return true;
    }

    void Device::shutdown()
    {
        destroyCommandPool();

        if (m_device)
        {
            vkDestroyDevice(m_device, nullptr);
            m_physical_device = VK_NULL_HANDLE;
            m_device = VK_NULL_HANDLE;
            m_graphics_queue = VK_NULL_HANDLE;
            m_present_queue = VK_NULL_HANDLE;
            m_graphics_queue_family_index = 0;
            Logger::getInstance().debug("Logical device destroyed");
        }
    }

    bool Device::createCommandPool()
    {
        if (!m_device)
        {
            return false;
        }

        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = m_graphics_queue_family_index;

        VkResult result = vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool);
        if (result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to create command pool: " + std::to_string(result));
            return false;
        }

        return true;
    }

    void Device::destroyCommandPool()
    {
        if (m_command_pool && m_device)
        {
            vkDestroyCommandPool(m_device, m_command_pool, nullptr);
            m_command_pool = VK_NULL_HANDLE;
        }
    }

}  // namespace kera
