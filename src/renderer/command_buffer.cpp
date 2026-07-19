// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/command_buffer.h"

#include "kera/renderer/device.h"
#include "kera/utilities/logger.h"

#include <vulkan/vulkan.h>

#include <string>

namespace kera
{

    CommandBuffer::CommandBuffer()
        : m_device(VK_NULL_HANDLE)
        , m_command_pool(VK_NULL_HANDLE)
        , m_command_buffer(VK_NULL_HANDLE)
        , m_state(EState::UNINITIALIZED)
    {
    }

    CommandBuffer::~CommandBuffer()
    {
        shutdown();
    }

    CommandBuffer::CommandBuffer(CommandBuffer&& other) noexcept
        : m_device(other.m_device)
        , m_command_pool(other.m_command_pool)
        , m_command_buffer(other.m_command_buffer)
        , m_state(other.m_state)
    {
        other.m_device = VK_NULL_HANDLE;
        other.m_command_pool = VK_NULL_HANDLE;
        other.m_command_buffer = VK_NULL_HANDLE;
        other.m_state = EState::UNINITIALIZED;
    }

    CommandBuffer& CommandBuffer::operator=(CommandBuffer&& other) noexcept
    {
        if (this != &other)
        {
            shutdown();
            m_device = other.m_device;
            m_command_pool = other.m_command_pool;
            m_command_buffer = other.m_command_buffer;
            m_state = other.m_state;
            other.m_device = VK_NULL_HANDLE;
            other.m_command_pool = VK_NULL_HANDLE;
            other.m_command_buffer = VK_NULL_HANDLE;
            other.m_state = EState::UNINITIALIZED;
        }
        return *this;
    }

    bool CommandBuffer::initialize(const Device& device)
    {
        if (m_command_buffer)
        {
            shutdown();
        }

        m_device = device.getVulkanDevice();
        m_command_pool = device.getCommandPool();
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = m_command_pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;

        VkResult result = vkAllocateCommandBuffers(device.getVulkanDevice(), &alloc_info, &m_command_buffer);
        if (result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to allocate command buffer: " + std::to_string(result));
            m_state = EState::UNINITIALIZED;
            return false;
        }

        m_state = EState::READY;
        return true;
    }

    void CommandBuffer::shutdown()
    {
        if (m_command_buffer)
        {
            vkFreeCommandBuffers(m_device, m_command_pool, 1, &m_command_buffer);
            m_command_buffer = VK_NULL_HANDLE;
        }
        m_command_pool = VK_NULL_HANDLE;
        m_device = VK_NULL_HANDLE;
        m_state = EState::UNINITIALIZED;
    }

    bool CommandBuffer::begin()
    {
        if (!m_command_buffer)
        {
            Logger::getInstance().error("Cannot begin an uninitialized Vulkan command buffer.");
            return false;
        }

        if (m_state != EState::READY)
        {
            Logger::getInstance().error("Cannot begin a Vulkan command buffer that is not ready.");
            return false;
        }

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VkResult result = vkBeginCommandBuffer(m_command_buffer, &begin_info);
        if (result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to begin Vulkan command buffer: " + std::to_string(result));
            return false;
        }

        m_state = EState::RECORDING;
        return true;
    }

    bool CommandBuffer::end()
    {
        if (!m_command_buffer)
        {
            Logger::getInstance().error("Cannot end an uninitialized Vulkan command buffer.");
            return false;
        }

        if (m_state != EState::RECORDING)
        {
            Logger::getInstance().error("Cannot end a Vulkan command buffer that is not recording.");
            return false;
        }

        VkResult result = vkEndCommandBuffer(m_command_buffer);
        if (result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to end Vulkan command buffer: " + std::to_string(result));
            return false;
        }

        m_state = EState::EXECUTABLE;
        return true;
    }

    bool CommandBuffer::reset()
    {
        if (!m_command_buffer)
        {
            Logger::getInstance().error("Cannot reset an uninitialized Vulkan command buffer.");
            return false;
        }

        if (m_state == EState::PENDING)
        {
            Logger::getInstance().error("Cannot reset a pending Vulkan command buffer.");
            return false;
        }

        VkResult result = vkResetCommandBuffer(m_command_buffer, 0);
        if (result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to reset Vulkan command buffer: " + std::to_string(result));
            return false;
        }

        m_state = EState::READY;
        return true;
    }

    bool CommandBuffer::markSubmitted()
    {
        if (m_state != EState::EXECUTABLE)
        {
            Logger::getInstance().error("Cannot mark a Vulkan command buffer submitted before it is executable.");
            return false;
        }

        m_state = EState::PENDING;
        return true;
    }

    void CommandBuffer::markCompleted()
    {
        if (m_state == EState::PENDING)
        {
            m_state = EState::EXECUTABLE;
        }
    }

}  // namespace kera
