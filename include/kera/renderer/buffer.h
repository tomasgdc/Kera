// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace kera
{
    enum class EBufferUsage
    {
        VERTEX,
        INDEX,
        UNIFORM,
        STORAGE,
        TRANSFER_SRC,
        TRANSFER_DST
    };

    class Device;

    class Buffer
    {
    public:
        Buffer();
        ~Buffer();

        // Delete copy operations
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;

        // Move operations
        Buffer(Buffer&& other) noexcept;
        Buffer& operator=(Buffer&& other) noexcept;

        bool initialize(const Device& device, VkDeviceSize size, EBufferUsage usage, VkMemoryPropertyFlags properties);
        void shutdown();

        VkBuffer getVulkanBuffer() const
        {
            return m_buffer;
        }
        VkDeviceMemory getDeviceMemory() const
        {
            return m_memory;
        }
        VkDeviceSize getSize() const
        {
            return m_size;
        }

        bool isValid() const
        {
            return m_buffer != VK_NULL_HANDLE;
        }

        // Data operations
        bool map(void** data);
        void unmap();
        bool copyFrom(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    private:
        VkDevice m_device;
        VkBuffer m_buffer;
        VkDeviceMemory m_memory;
        VkDeviceSize m_size;
        VkDeviceSize m_non_coherent_atom_size;
        VkMemoryPropertyFlags m_memory_properties;
        void* m_mapped_data;
    };

}  // namespace kera
