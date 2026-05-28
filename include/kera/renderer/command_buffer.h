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
            return command_buffer_;
        }
        bool isValid() const
        {
            return command_buffer_ != VK_NULL_HANDLE;
        }
        bool isRecording() const
        {
            return state_ == State::Recording;
        }
        bool isPending() const
        {
            return state_ == State::Pending;
        }

        // Command buffer operations
        bool begin();
        bool end();
        bool reset();
        bool markSubmitted();
        void markCompleted();

    private:
        enum class State
        {
            Uninitialized,
            Ready,
            Recording,
            Executable,
            Pending
        };

        VkDevice device_;
        VkCommandPool command_pool_;
        VkCommandBuffer command_buffer_;
        State state_;
    };

}  // namespace kera
