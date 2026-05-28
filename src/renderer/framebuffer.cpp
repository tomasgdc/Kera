// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/framebuffer.h"

#include "kera/renderer/device.h"
#include "kera/renderer/render_pass.h"
#include "kera/renderer/swapchain.h"
#include "kera/utilities/logger.h"

#include <vulkan/vulkan.h>

namespace kera
{

    Framebuffer::Framebuffer() : device_(VK_NULL_HANDLE) {}

    Framebuffer::~Framebuffer()
    {
        shutdown();
    }

    Framebuffer::Framebuffer(Framebuffer&& other) noexcept
        : device_(other.device_), framebuffers_(std::move(other.framebuffers_))
    {
        other.device_ = VK_NULL_HANDLE;
    }

    Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept
    {
        if (this != &other)
        {
            shutdown();
            device_ = other.device_;
            framebuffers_ = std::move(other.framebuffers_);
            other.device_ = VK_NULL_HANDLE;
        }
        return *this;
    }

    bool Framebuffer::initialize(const Device& device, const RenderPass& renderPass, const SwapChain& swapChain)
    {
        shutdown();

        VkDevice vkDevice = device.getVulkanDevice();
        device_ = vkDevice;
        const auto& imageViews = swapChain.getImageViews();
        VkExtent2D extent = swapChain.getExtent();

        framebuffers_.resize(imageViews.size());

        for (size_t i = 0; i < imageViews.size(); i++)
        {
            VkImageView attachments[] = {imageViews[i]};

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass.getVulkanRenderPass();
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = extent.width;
            framebufferInfo.height = extent.height;
            framebufferInfo.layers = 1;

            VkResult result = vkCreateFramebuffer(vkDevice, &framebufferInfo, nullptr, &framebuffers_[i]);
            if (result != VK_SUCCESS)
            {
                Logger::getInstance().error("Failed to create framebuffer " + std::to_string(i) + ": " +
                                            std::to_string(result));
                shutdown();
                return false;
            }
        }

        Logger::getInstance().debug("Framebuffers created successfully (" + std::to_string(framebuffers_.size()) +
                                    " framebuffers)");
        return true;
    }

    bool Framebuffer::initializeSingleColorTarget(const Device& device, const RenderPass& renderPass,
                                                  VkImageView colorImageView, VkExtent2D extent,
                                                  VkImageView depthImageView)
    {
        shutdown();

        if (colorImageView == VK_NULL_HANDLE || extent.width == 0 || extent.height == 0)
        {
            return false;
        }

        VkDevice vkDevice = device.getVulkanDevice();
        device_ = vkDevice;

        VkImageView attachments[] = {colorImageView, depthImageView};
        const uint32_t attachmentCount = depthImageView == VK_NULL_HANDLE ? 1u : 2u;

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass.getVulkanRenderPass();
        framebufferInfo.attachmentCount = attachmentCount;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;

        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VkResult result = vkCreateFramebuffer(vkDevice, &framebufferInfo, nullptr, &framebuffer);
        if (result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to create color target framebuffer: " + std::to_string(result));
            return false;
        }

        framebuffers_.push_back(framebuffer);
        Logger::getInstance().debug("Color target framebuffer created successfully");
        return true;
    }

    void Framebuffer::shutdown()
    {
        for (auto framebuffer : framebuffers_)
        {
            if (framebuffer)
            {
                vkDestroyFramebuffer(device_, framebuffer, nullptr);
            }
        }
        framebuffers_.clear();
        device_ = VK_NULL_HANDLE;
    }

    VkFramebuffer Framebuffer::getFramebuffer(uint32_t index) const
    {
        if (index < framebuffers_.size())
        {
            return framebuffers_[index];
        }
        return VK_NULL_HANDLE;
    }

}  // namespace kera
