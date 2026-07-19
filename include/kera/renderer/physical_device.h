// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <vector>

namespace kera
{

    struct QueueFamilyIndices
    {
        int graphics_family = -1;
        int present_family = -1;

        bool isComplete() const
        {
            return graphics_family >= 0 && present_family >= 0;
        }
    };

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
    };

    class PhysicalDevice
    {
    public:
        PhysicalDevice();
        ~PhysicalDevice() = default;

        bool initialize(VkInstance instance, VkSurfaceKHR surface);
        void shutdown();

        VkPhysicalDevice getVulkanPhysicalDevice() const
        {
            return m_physical_device;
        }
        const VkPhysicalDeviceProperties& getProperties() const
        {
            return m_properties;
        }
        const VkPhysicalDeviceFeatures& getFeatures() const
        {
            return m_features;
        }

        const QueueFamilyIndices& getQueueFamilyIndices() const
        {
            return m_queue_families;
        }
        const SwapChainSupportDetails& getSwapChainSupport() const
        {
            return m_swap_chain_support;
        }

        bool isSuitable() const;
        std::string getDeviceName() const;

        bool refreshSwapchainSupport(VkSurfaceKHR surface);

    private:
        void pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
        bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) const;
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) const;

        VkPhysicalDevice m_physical_device;
        VkPhysicalDeviceProperties m_properties;
        VkPhysicalDeviceFeatures m_features;
        QueueFamilyIndices m_queue_families;
        SwapChainSupportDetails m_swap_chain_support;
    };

}  // namespace kera
