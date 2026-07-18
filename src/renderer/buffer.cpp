// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/buffer.h"

#include "kera/renderer/device.h"
#include "kera/utilities/logger.h"

#include <vulkan/vulkan.h>

#include <cstring>

namespace kera
{

    namespace
    {

        bool findMemoryType(VkPhysicalDevice physical_device, uint32_t type_filter, VkMemoryPropertyFlags properties,
                            uint32_t& out_memory_type)
        {
            VkPhysicalDeviceMemoryProperties mem_properties;
            vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

            for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
            {
                if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
                {
                    out_memory_type = i;
                    return true;
                }
            }

            return false;
        }

    }  // anonymous namespace

    Buffer::Buffer()
        : m_device(VK_NULL_HANDLE)
        , m_buffer(VK_NULL_HANDLE)
        , m_memory(VK_NULL_HANDLE)
        , m_size(0)
        , m_non_coherent_atom_size(1)
        , m_memory_properties(0)
        , m_mapped_data(nullptr)
    {
    }

    Buffer::~Buffer()
    {
        shutdown();
    }

    Buffer::Buffer(Buffer&& other) noexcept
        : m_device(other.m_device)
        , m_buffer(other.m_buffer)
        , m_memory(other.m_memory)
        , m_size(other.m_size)
        , m_non_coherent_atom_size(other.m_non_coherent_atom_size)
        , m_memory_properties(other.m_memory_properties)
        , m_mapped_data(other.m_mapped_data)
    {
        other.m_device = VK_NULL_HANDLE;
        other.m_buffer = VK_NULL_HANDLE;
        other.m_memory = VK_NULL_HANDLE;
        other.m_size = 0;
        other.m_non_coherent_atom_size = 1;
        other.m_memory_properties = 0;
        other.m_mapped_data = nullptr;
    }

    Buffer& Buffer::operator=(Buffer&& other) noexcept
    {
        if (this != &other)
        {
            shutdown();
            m_device = other.m_device;
            m_buffer = other.m_buffer;
            m_memory = other.m_memory;
            m_size = other.m_size;
            m_non_coherent_atom_size = other.m_non_coherent_atom_size;
            m_memory_properties = other.m_memory_properties;
            m_mapped_data = other.m_mapped_data;

            other.m_device = VK_NULL_HANDLE;
            other.m_buffer = VK_NULL_HANDLE;
            other.m_memory = VK_NULL_HANDLE;
            other.m_size = 0;
            other.m_non_coherent_atom_size = 1;
            other.m_memory_properties = 0;
            other.m_mapped_data = nullptr;
        }
        return *this;
    }

    bool Buffer::initialize(const Device& device, VkDeviceSize size, EBufferUsage usage,
                            VkMemoryPropertyFlags properties)
    {
        if (m_buffer)
        {
            shutdown();
        }

        VkDevice vk_device = device.getVulkanDevice();
        VkPhysicalDevice vk_physical_device = device.getVulkanPhysicalDevice();
        m_device = vk_device;
        m_size = size;
        m_memory_properties = properties;

        VkPhysicalDeviceProperties physical_device_properties{};
        vkGetPhysicalDeviceProperties(vk_physical_device, &physical_device_properties);
        m_non_coherent_atom_size = physical_device_properties.limits.nonCoherentAtomSize;

        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;

        switch (usage)
        {
            case EBufferUsage::VERTEX:
                buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
                break;
            case EBufferUsage::INDEX:
                buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
                break;
            case EBufferUsage::UNIFORM:
                buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
                break;
            case EBufferUsage::STORAGE:
                buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
                break;
            case EBufferUsage::TRANSFER_SRC:
                buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
                break;
            case EBufferUsage::TRANSFER_DST:
                buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                break;
        }

        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkResult result = vkCreateBuffer(vk_device, &buffer_info, nullptr, &m_buffer);
        if (result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to create buffer: " + std::to_string(result));
            return false;
        }

        VkMemoryRequirements mem_requirements;
        vkGetBufferMemoryRequirements(vk_device, m_buffer, &mem_requirements);

        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_requirements.size;
        if (!findMemoryType(vk_physical_device, mem_requirements.memoryTypeBits, properties, alloc_info.memoryTypeIndex))
        {
            Logger::getInstance().error("Failed to find suitable buffer memory type");
            vkDestroyBuffer(vk_device, m_buffer, nullptr);
            m_buffer = VK_NULL_HANDLE;
            return false;
        }

        result = vkAllocateMemory(vk_device, &alloc_info, nullptr, &m_memory);
        if (result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to allocate buffer memory: " + std::to_string(result));
            vkDestroyBuffer(vk_device, m_buffer, nullptr);
            m_buffer = VK_NULL_HANDLE;
            return false;
        }

        vkBindBufferMemory(vk_device, m_buffer, m_memory, 0);

        Logger::getInstance().debug("Buffer created successfully (size: " + std::to_string(size) + " bytes)");
        return true;
    }

    void Buffer::shutdown()
    {
        if (m_mapped_data)
        {
            unmap();
        }

        if (m_buffer)
        {
            vkDestroyBuffer(m_device, m_buffer, nullptr);
            m_buffer = VK_NULL_HANDLE;
        }

        if (m_memory)
        {
            vkFreeMemory(m_device, m_memory, nullptr);
            m_memory = VK_NULL_HANDLE;
        }

        m_device = VK_NULL_HANDLE;
        m_size = 0;
        m_non_coherent_atom_size = 1;
        m_memory_properties = 0;
    }

    bool Buffer::map(void** data)
    {
        if (!m_memory || m_mapped_data)
        {
            return false;
        }

        VkResult result = vkMapMemory(m_device, m_memory, 0, m_size, 0, &m_mapped_data);
        if (result != VK_SUCCESS)
        {
            return false;
        }

        *data = m_mapped_data;
        return true;
    }

    void Buffer::unmap()
    {
        if (m_mapped_data && m_memory)
        {
            vkUnmapMemory(m_device, m_memory);
            m_mapped_data = nullptr;
        }
    }

    bool Buffer::copyFrom(const void* data, VkDeviceSize size, VkDeviceSize offset)
    {
        if (!data || offset + size > m_size)
        {
            return false;
        }

        void* mapped_data = nullptr;
        if (!map(&mapped_data))
        {
            return false;
        }

        memcpy(static_cast<char*>(mapped_data) + offset, data, size);
        if ((m_memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
        {
            const VkDeviceSize atom_size = m_non_coherent_atom_size == 0 ? 1 : m_non_coherent_atom_size;
            const VkDeviceSize aligned_offset = (offset / atom_size) * atom_size;
            const VkDeviceSize end = offset + size;
            const VkDeviceSize aligned_end = ((end + atom_size - 1) / atom_size) * atom_size;
            VkMappedMemoryRange range{};
            range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            range.memory = m_memory;
            range.offset = aligned_offset;
            range.size = aligned_end > m_size ? VK_WHOLE_SIZE : aligned_end - aligned_offset;
            vkFlushMappedMemoryRanges(m_device, 1, &range);
        }
        unmap();

        return true;
    }

}  // namespace kera
