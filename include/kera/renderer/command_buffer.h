#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace kera {

class Device;

class CommandBuffer {
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

    VkCommandBuffer getVulkanCommandBuffer() const { return command_buffer_; }
    bool isValid() const { return command_buffer_ != VK_NULL_HANDLE; }

    // Command buffer operations
    bool begin();
    bool end();
    void reset();

private:
    VkCommandBuffer command_buffer_;
};

} // namespace kera