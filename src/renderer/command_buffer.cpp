#include "kera/renderer/command_buffer.h"

#include "kera/renderer/device.h"
#include "kera/utilities/logger.h"

#include <vulkan/vulkan.h>

#include <string>

namespace kera
{

    CommandBuffer::CommandBuffer()
        : device_(VK_NULL_HANDLE)
        , command_pool_(VK_NULL_HANDLE)
        , command_buffer_(VK_NULL_HANDLE)
        , state_(State::Uninitialized)
    {
    }

    CommandBuffer::~CommandBuffer()
    {
        shutdown();
    }

    CommandBuffer::CommandBuffer(CommandBuffer&& other) noexcept
        : device_(other.device_)
        , command_pool_(other.command_pool_)
        , command_buffer_(other.command_buffer_)
        , state_(other.state_)
    {
        other.device_ = VK_NULL_HANDLE;
        other.command_pool_ = VK_NULL_HANDLE;
        other.command_buffer_ = VK_NULL_HANDLE;
        other.state_ = State::Uninitialized;
    }

    CommandBuffer& CommandBuffer::operator=(CommandBuffer&& other) noexcept
    {
        if (this != &other)
        {
            shutdown();
            device_ = other.device_;
            command_pool_ = other.command_pool_;
            command_buffer_ = other.command_buffer_;
            state_ = other.state_;
            other.device_ = VK_NULL_HANDLE;
            other.command_pool_ = VK_NULL_HANDLE;
            other.command_buffer_ = VK_NULL_HANDLE;
            other.state_ = State::Uninitialized;
        }
        return *this;
    }

    bool CommandBuffer::initialize(const Device& device)
    {
        if (command_buffer_)
        {
            shutdown();
        }

        device_ = device.getVulkanDevice();
        command_pool_ = device.getCommandPool();
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = command_pool_;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkResult result = vkAllocateCommandBuffers(device.getVulkanDevice(), &allocInfo, &command_buffer_);
        if (result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to allocate command buffer: " + std::to_string(result));
            state_ = State::Uninitialized;
            return false;
        }

        state_ = State::Ready;
        return true;
    }

    void CommandBuffer::shutdown()
    {
        if (command_buffer_)
        {
            vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer_);
            command_buffer_ = VK_NULL_HANDLE;
        }
        command_pool_ = VK_NULL_HANDLE;
        device_ = VK_NULL_HANDLE;
        state_ = State::Uninitialized;
    }

    bool CommandBuffer::begin()
    {
        if (!command_buffer_)
        {
            Logger::getInstance().error("Cannot begin an uninitialized Vulkan command buffer.");
            return false;
        }

        if (state_ != State::Ready)
        {
            Logger::getInstance().error("Cannot begin a Vulkan command buffer that is not ready.");
            return false;
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VkResult result = vkBeginCommandBuffer(command_buffer_, &beginInfo);
        if (result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to begin Vulkan command buffer: " + std::to_string(result));
            return false;
        }

        state_ = State::Recording;
        return true;
    }

    bool CommandBuffer::end()
    {
        if (!command_buffer_)
        {
            Logger::getInstance().error("Cannot end an uninitialized Vulkan command buffer.");
            return false;
        }

        if (state_ != State::Recording)
        {
            Logger::getInstance().error("Cannot end a Vulkan command buffer that is not recording.");
            return false;
        }

        VkResult result = vkEndCommandBuffer(command_buffer_);
        if (result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to end Vulkan command buffer: " + std::to_string(result));
            return false;
        }

        state_ = State::Executable;
        return true;
    }

    bool CommandBuffer::reset()
    {
        if (!command_buffer_)
        {
            Logger::getInstance().error("Cannot reset an uninitialized Vulkan command buffer.");
            return false;
        }

        if (state_ == State::Pending)
        {
            Logger::getInstance().error("Cannot reset a pending Vulkan command buffer.");
            return false;
        }

        VkResult result = vkResetCommandBuffer(command_buffer_, 0);
        if (result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to reset Vulkan command buffer: " + std::to_string(result));
            return false;
        }

        state_ = State::Ready;
        return true;
    }

    bool CommandBuffer::markSubmitted()
    {
        if (state_ != State::Executable)
        {
            Logger::getInstance().error("Cannot mark a Vulkan command buffer submitted before it is executable.");
            return false;
        }

        state_ = State::Pending;
        return true;
    }

    void CommandBuffer::markCompleted()
    {
        if (state_ == State::Pending)
        {
            state_ = State::Executable;
        }
    }

}  // namespace kera
