#include "kera/renderer/buffer.h"
#include "kera/renderer/device.h"
#include <vulkan/vulkan.h>
#include <iostream>

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
    : buffer_(VK_NULL_HANDLE)
    , memory_(VK_NULL_HANDLE)
    , size_(0)
    , mapped_data_(nullptr) {
}

Buffer::~Buffer() {
    shutdown();
}

Buffer::Buffer(Buffer&& other) noexcept
    : buffer_(other.buffer_)
    , memory_(other.memory_)
    , size_(other.size_)
    , mapped_data_(other.mapped_data_) {
    other.buffer_ = VK_NULL_HANDLE;
    other.memory_ = VK_NULL_HANDLE;
    other.size_ = 0;
    other.mapped_data_ = nullptr;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        shutdown();
        buffer_ = other.buffer_;
        memory_ = other.memory_;
        size_ = other.size_;
        mapped_data_ = other.mapped_data_;

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
    allocInfo.memoryTypeIndex = findMemoryType(VK_NULL_HANDLE, memRequirements.memoryTypeBits, properties); // TODO: Need physical device

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
        // TODO: Need device reference
        // vkDestroyBuffer(device, buffer_, nullptr);
        buffer_ = VK_NULL_HANDLE;
    }

    if (memory_) {
        // TODO: Need device reference
        // vkFreeMemory(device, memory_, nullptr);
        memory_ = VK_NULL_HANDLE;
    }

    size_ = 0;
}

bool Buffer::map(void** data) {
    if (!memory_ || mapped_data_) {
        return false;
    }

    // TODO: Need device reference
    // VkResult result = vkMapMemory(device, memory_, 0, size_, 0, &mapped_data_);
    // if (result != VK_SUCCESS) {
    //     return false;
    // }

    *data = mapped_data_;
    return true;
}

void Buffer::unmap() {
    if (mapped_data_ && memory_) {
        // TODO: Need device reference
        // vkUnmapMemory(device, memory_);
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