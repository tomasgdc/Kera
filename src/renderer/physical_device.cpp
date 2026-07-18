// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/physical_device.h"

#include "kera/utilities/logger.h"

#include <vulkan/vulkan.h>

#include <set>
#include <string>

namespace kera
{

    namespace
    {

        // Required device extensions
        const std::vector<const char*> kDeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        bool checkDeviceExtensionSupport(VkPhysicalDevice device)
        {
            uint32_t extension_count;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

            std::vector<VkExtensionProperties> available_extensions(extension_count);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

            std::set<std::string> required_extensions(kDeviceExtensions.begin(), kDeviceExtensions.end());

            for (const auto& extension : available_extensions)
            {
                required_extensions.erase(extension.extensionName);
            }

            return required_extensions.empty();
        }

        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
        {
            QueueFamilyIndices indices;

            uint32_t queue_family_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

            std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

            int i = 0;
            for (const auto& queue_family : queue_families)
            {
                if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    indices.graphics_family = i;
                }

                VkBool32 present_support = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);

                if (present_support)
                {
                    indices.present_family = i;
                }

                if (indices.isComplete())
                {
                    break;
                }

                i++;
            }

            return indices;
        }

        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
        {
            SwapChainSupportDetails details;

            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

            uint32_t format_count;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);

            if (format_count != 0)
            {
                details.formats.resize(format_count);
                vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats.data());
            }

            uint32_t present_mode_count;
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);

            if (present_mode_count != 0)
            {
                details.present_modes.resize(present_mode_count);
                vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count,
                                                          details.present_modes.data());
            }

            return details;
        }

        bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface)
        {
            VkPhysicalDeviceProperties device_properties;
            vkGetPhysicalDeviceProperties(device, &device_properties);

            VkPhysicalDeviceFeatures device_features;
            vkGetPhysicalDeviceFeatures(device, &device_features);

            bool extensions_supported = checkDeviceExtensionSupport(device);
            bool swap_chain_adequate = false;

            if (extensions_supported)
            {
                SwapChainSupportDetails swap_chain_support = querySwapChainSupport(device, surface);
                swap_chain_adequate = !swap_chain_support.formats.empty() && !swap_chain_support.present_modes.empty();
            }

            QueueFamilyIndices indices = findQueueFamilies(device, surface);

            return device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
                   device_features.geometryShader && indices.isComplete() && extensions_supported && swap_chain_adequate;
        }

    }  // anonymous namespace

    PhysicalDevice::PhysicalDevice() : m_physical_device(VK_NULL_HANDLE) {}

    bool PhysicalDevice::initialize(VkInstance instance, VkSurfaceKHR surface)
    {
        pickPhysicalDevice(instance, surface);
        return m_physical_device != VK_NULL_HANDLE;
    }

    void PhysicalDevice::shutdown()
    {
        m_physical_device = VK_NULL_HANDLE;
    }

    void PhysicalDevice::pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface)
    {
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

        if (device_count == 0)
        {
            Logger::getInstance().error("Failed to find GPUs with Vulkan support");
            return;
        }

        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

        for (const auto& device : devices)
        {
            if (isDeviceSuitable(device, surface))
            {
                m_physical_device = device;
                vkGetPhysicalDeviceProperties(device, &m_properties);
                vkGetPhysicalDeviceFeatures(device, &m_features);
                m_queue_families = findQueueFamilies(device, surface);
                m_swap_chain_support = querySwapChainSupport(device, surface);
                break;
            }
        }

        if (m_physical_device == VK_NULL_HANDLE)
        {
            Logger::getInstance().error("Failed to find a suitable GPU");
        }
        else
        {
            Logger::getInstance().info("Selected physical device: " + getDeviceName());
        }
    }

    bool PhysicalDevice::checkDeviceExtensionSupport(VkPhysicalDevice device) const
    {
        return ::kera::checkDeviceExtensionSupport(device);
    }

    QueueFamilyIndices PhysicalDevice::findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) const
    {
        return ::kera::findQueueFamilies(device, surface);
    }

    SwapChainSupportDetails PhysicalDevice::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) const
    {
        return ::kera::querySwapChainSupport(device, surface);
    }

    bool PhysicalDevice::isSuitable() const
    {
        return m_physical_device != VK_NULL_HANDLE && m_queue_families.isComplete();
    }

    std::string PhysicalDevice::getDeviceName() const
    {
        if (m_physical_device)
        {
            return m_properties.deviceName;
        }
        return "No device selected";
    }

    bool PhysicalDevice::refreshSwapchainSupport(VkSurfaceKHR surface)
    {
        if (m_physical_device == VK_NULL_HANDLE || surface == VK_NULL_HANDLE)
        {
            return false;
        }

        m_swap_chain_support = querySwapChainSupport(m_physical_device, surface);
        return !m_swap_chain_support.formats.empty() && !m_swap_chain_support.present_modes.empty();
    }

}  // namespace kera
