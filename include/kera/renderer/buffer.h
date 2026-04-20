#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace kera {

enum class BufferUsage {
    Vertex,
    Index,
    Uniform,
    Storage,
    TransferSrc,
    TransferDst
};

class Device;

class Buffer {
public:
    Buffer();
    ~Buffer();

    // Delete copy operations
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    // Move operations
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    bool initialize(const Device& device, VkDeviceSize size, BufferUsage usage, VkMemoryPropertyFlags properties);
    void shutdown();

    VkBuffer getVulkanBuffer() const { return buffer_; }
    VkDeviceMemory getDeviceMemory() const { return memory_; }
    VkDeviceSize getSize() const { return size_; }

    bool isValid() const { return buffer_ != VK_NULL_HANDLE; }

    // Data operations
    bool map(void** data);
    void unmap();
    bool copyFrom(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

private:
    VkBuffer buffer_;
    VkDeviceMemory memory_;
    VkDeviceSize size_;
    void* mapped_data_;
};

} // namespace kera