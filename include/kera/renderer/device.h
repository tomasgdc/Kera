// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace kera
{

    class PhysicalDevice;

    class Device
    {
    public:
        Device();
        ~Device();

        // Delete copy operations
        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;

        // Move operations
        Device(Device&& other) noexcept;
        Device& operator=(Device&& other) noexcept;

        bool initialize(const PhysicalDevice& physical_device);
        void shutdown();

        VkDevice getVulkanDevice() const
        {
            return m_device;
        }
        VkPhysicalDevice getVulkanPhysicalDevice() const
        {
            return m_physical_device;
        }
        VkQueue getGraphicsQueue() const
        {
            return m_graphics_queue;
        }
        VkQueue getPresentQueue() const
        {
            return m_present_queue;
        }
        VkCommandPool getCommandPool() const
        {
            return m_command_pool;
        }
        uint32_t getGraphicsQueueFamilyIndex() const
        {
            return m_graphics_queue_family_index;
        }

        bool isValid() const
        {
            return m_device != VK_NULL_HANDLE;
        }

        // Command buffer management
        bool createCommandPool();
        void destroyCommandPool();

    private:
        VkPhysicalDevice m_physical_device;
        VkDevice m_device;
        VkQueue m_graphics_queue;
        VkQueue m_present_queue;
        VkCommandPool m_command_pool;
        uint32_t m_graphics_queue_family_index;
    };

}  // namespace kera
