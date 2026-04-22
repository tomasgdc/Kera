#include "kera/renderer/buffer.h"
#include "kera/renderer/device.h"
#include <vulkan/vulkan.h>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace kera {

namespace {

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

} // anonymous namespace

Buffer::Buffer()
    : device_(VK_NULL_HANDLE)
    , buffer_(VK_NULL_HANDLE)
    , memory_(VK_NULL_HANDLE)
    , size_(0)
    , mapped_data_(nullptr) {
}

Buffer::~Buffer() {
    shutdown();
}

Buffer::Buffer(Buffer&& other) noexcept
    : device_(other.device_)
    , buffer_(other.buffer_)
    , memory_(other.memory_)
    , size_(other.size_)
    , mapped_data_(other.mapped_data_) {
    other.device_ = VK_NULL_HANDLE;
    other.buffer_ = VK_NULL_HANDLE;
    other.memory_ = VK_NULL_HANDLE;
    other.size_ = 0;
    other.mapped_data_ = nullptr;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        shutdown();
        device_ = other.device_;
        buffer_ = other.buffer_;
        memory_ = other.memory_;
        size_ = other.size_;
        mapped_data_ = other.mapped_data_;

        other.device_ = VK_NULL_HANDLE;
        other.buffer_ = VK_NULL_HANDLE;
        other.memory_ = VK_NULL_HANDLE;
        other.size_ = 0;
        other.mapped_data_ = nullptr;
    }
    return *this;
}

bool Buffer::initialize(const Device& device, VkDeviceSize size, BufferUsage usage, VkMemoryPropertyFlags properties) {
    if (buffer_) {
        shutdown();
    }

    VkDevice vkDevice = device.getVulkanDevice();
    VkPhysicalDevice vkPhysicalDevice = device.getVulkanPhysicalDevice();
    device_ = vkDevice;
    size_ = size;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;

    switch (usage) {
        case BufferUsage::Vertex:
            bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            break;
        case BufferUsage::Index:
            bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            break;
        case BufferUsage::Uniform:
            bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            break;
        case BufferUsage::Storage:
            bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            break;
        case BufferUsage::TransferSrc:
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            break;
        case BufferUsage::TransferDst:
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            break;
    }

    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(vkDevice, &bufferInfo, nullptr, &buffer_);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create buffer: " << result << std::endl;
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(vkDevice, buffer_, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(vkPhysicalDevice, memRequirements.memoryTypeBits, properties);

    result = vkAllocateMemory(vkDevice, &allocInfo, nullptr, &memory_);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to allocate buffer memory: " << result << std::endl;
        vkDestroyBuffer(vkDevice, buffer_, nullptr);
        buffer_ = VK_NULL_HANDLE;
        return false;
    }

    vkBindBufferMemory(vkDevice, buffer_, memory_, 0);

    std::cout << "Buffer created successfully (size: " << size << " bytes)" << std::endl;
    return true;
}

void Buffer::shutdown() {
    if (mapped_data_) {
        unmap();
    }

    if (buffer_) {
        vkDestroyBuffer(device_, buffer_, nullptr);
        buffer_ = VK_NULL_HANDLE;
    }

    if (memory_) {
        vkFreeMemory(device_, memory_, nullptr);
        memory_ = VK_NULL_HANDLE;
    }

    device_ = VK_NULL_HANDLE;
    size_ = 0;
}

bool Buffer::map(void** data) {
    if (!memory_ || mapped_data_) {
        return false;
    }

    VkResult result = vkMapMemory(device_, memory_, 0, size_, 0, &mapped_data_);
    if (result != VK_SUCCESS) {
        return false;
    }

    *data = mapped_data_;
    return true;
}

void Buffer::unmap() {
    if (mapped_data_ && memory_) {
        vkUnmapMemory(device_, memory_);
        mapped_data_ = nullptr;
    }
}

bool Buffer::copyFrom(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (!data || offset + size > size_) {
        return false;
    }

    void* mappedData = nullptr;
    if (!map(&mappedData)) {
        return false;
    }

    memcpy(static_cast<char*>(mappedData) + offset, data, size);
    unmap();

    return true;
}

} // namespace kera
