// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace kera
{

    class Device;

    class CommandBuffer
    {
    public:
        CommandBuffer();
        ~CommandBuffer();

        // Delete copy operations
        CommandBuffer(const CommandBuffer&) = delete;
        CommandBuffer& operator=(const CommandBuffer&) = delete;

        // Move operations
        CommandBuffer(CommandBuffer&& other) noexcept;
        CommandBuffer& operator=(CommandBuffer&& other) noexcept;

        bool initialize(const Device& device);
        void shutdown();

        VkCommandBuffer getVulkanCommandBuffer() const
        {
            return m_command_buffer;
        }
        bool isValid() const
        {
            return m_command_buffer != VK_NULL_HANDLE;
        }
        bool isRecording() const
        {
            return m_state == EState::RECORDING;
        }
        bool isPending() const
        {
            return m_state == EState::PENDING;
        }

        // Command buffer operations
        bool begin();
        bool end();
        bool reset();
        bool markSubmitted();
        void markCompleted();

    private:
        enum class EState
        {
            UNINITIALIZED,
            READY,
            RECORDING,
            EXECUTABLE,
            PENDING
        };

        VkDevice m_device;
        VkCommandPool m_command_pool;
        VkCommandBuffer m_command_buffer;
        EState m_state;
    };

}  // namespace kera
