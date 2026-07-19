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

    Framebuffer::Framebuffer() : m_device(VK_NULL_HANDLE) {}

    Framebuffer::~Framebuffer()
    {
        shutdown();
    }

    Framebuffer::Framebuffer(Framebuffer&& other) noexcept
        : m_device(other.m_device), m_framebuffers(std::move(other.m_framebuffers))
    {
        other.m_device = VK_NULL_HANDLE;
    }

    Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept
    {
        if (this != &other)
        {
            shutdown();
            m_device = other.m_device;
            m_framebuffers = std::move(other.m_framebuffers);
            other.m_device = VK_NULL_HANDLE;
        }
        return *this;
    }

    bool Framebuffer::initialize(const Device& device, const RenderPass& render_pass, const SwapChain& swap_chain)
    {
        shutdown();

        VkDevice vk_device = device.getVulkanDevice();
        m_device = vk_device;
        const auto& image_views = swap_chain.getImageViews();
        VkExtent2D extent = swap_chain.getExtent();

        m_framebuffers.resize(image_views.size());

        for (size_t i = 0; i < image_views.size(); i++)
        {
            VkImageView attachments[] = {image_views[i]};

            VkFramebufferCreateInfo framebuffer_info{};
            framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_info.renderPass = render_pass.getVulkanRenderPass();
            framebuffer_info.attachmentCount = 1;
            framebuffer_info.pAttachments = attachments;
            framebuffer_info.width = extent.width;
            framebuffer_info.height = extent.height;
            framebuffer_info.layers = 1;

            VkResult result = vkCreateFramebuffer(vk_device, &framebuffer_info, nullptr, &m_framebuffers[i]);
            if (result != VK_SUCCESS)
            {
                Logger::getInstance().error("Failed to create framebuffer " + std::to_string(i) + ": " +
                                            std::to_string(result));
                shutdown();
                return false;
            }
        }

        Logger::getInstance().debug("Framebuffers created successfully (" + std::to_string(m_framebuffers.size()) +
                                    " framebuffers)");
        return true;
    }

    bool Framebuffer::initializeSingleColorTarget(const Device& device, const RenderPass& render_pass,
                                                  VkImageView color_image_view, VkExtent2D extent,
                                                  VkImageView depth_image_view)
    {
        shutdown();

        if (color_image_view == VK_NULL_HANDLE || extent.width == 0 || extent.height == 0)
        {
            return false;
        }

        VkDevice vk_device = device.getVulkanDevice();
        m_device = vk_device;

        VkImageView attachments[] = {color_image_view, depth_image_view};
        const uint32_t attachment_count = depth_image_view == VK_NULL_HANDLE ? 1u : 2u;

        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = render_pass.getVulkanRenderPass();
        framebuffer_info.attachmentCount = attachment_count;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = extent.width;
        framebuffer_info.height = extent.height;
        framebuffer_info.layers = 1;

        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VkResult result = vkCreateFramebuffer(vk_device, &framebuffer_info, nullptr, &framebuffer);
        if (result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to create color target framebuffer: " + std::to_string(result));
            return false;
        }

        m_framebuffers.push_back(framebuffer);
        Logger::getInstance().debug("Color target framebuffer created successfully");
        return true;
    }

    void Framebuffer::shutdown()
    {
        for (auto framebuffer : m_framebuffers)
        {
            if (framebuffer)
            {
                vkDestroyFramebuffer(m_device, framebuffer, nullptr);
            }
        }
        m_framebuffers.clear();
        m_device = VK_NULL_HANDLE;
    }

    VkFramebuffer Framebuffer::getFramebuffer(uint32_t index) const
    {
        if (index < m_framebuffers.size())
        {
            return m_framebuffers[index];
        }
        return VK_NULL_HANDLE;
    }

}  // namespace kera
