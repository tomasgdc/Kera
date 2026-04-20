#include "kera/renderer/command_buffer.h"
#include "kera/renderer/device.h"
#include <vulkan/vulkan.h>
#include <iostream>

namespace kera {

CommandBuffer::CommandBuffer()
    : command_buffer_(VK_NULL_HANDLE) {
}

CommandBuffer::~CommandBuffer() {
    shutdown();
}

CommandBuffer::CommandBuffer(CommandBuffer&& other) noexcept
    : command_buffer_(other.command_buffer_) {
    other.command_buffer_ = VK_NULL_HANDLE;
}

CommandBuffer& CommandBuffer::operator=(CommandBuffer&& other) noexcept {
    if (this != &other) {
        shutdown();
        command_buffer_ = other.command_buffer_;
        other.command_buffer_ = VK_NULL_HANDLE;
    }
    return *this;
}

bool CommandBuffer::initialize(const Device& device) {
    if (command_buffer_) {
        shutdown();
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = device.getCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkResult result = vkAllocateCommandBuffers(device.getVulkanDevice(), &allocInfo, &command_buffer_);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to allocate command buffer: " << result << std::endl;
        return false;
    }

    return true;
}

void CommandBuffer::shutdown() {
    if (command_buffer_) {
        // TODO: Need device reference to free command buffer
        // vkFreeCommandBuffers(device, commandPool, 1, &command_buffer_);
        command_buffer_ = VK_NULL_HANDLE;
    }
}

bool CommandBuffer::begin() {
    if (!command_buffer_) {
        return false;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkResult result = vkBeginCommandBuffer(command_buffer_, &beginInfo);
    return result == VK_SUCCESS;
}

bool CommandBuffer::end() {
    if (!command_buffer_) {
        return false;
    }

    VkResult result = vkEndCommandBuffer(command_buffer_);
    return result == VK_SUCCESS;
}

void CommandBuffer::reset() {
    if (command_buffer_) {
        vkResetCommandBuffer(command_buffer_, 0);
    }
}

} // namespace kera