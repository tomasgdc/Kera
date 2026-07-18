// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/render_pass.h"

#include "kera/renderer/device.h"
#include "kera/renderer/swapchain.h"
#include "kera/utilities/logger.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace kera
{

    RenderPass::RenderPass() : m_device(VK_NULL_HANDLE), m_render_pass(VK_NULL_HANDLE) {}

    RenderPass::~RenderPass()
    {
        shutdown();
    }

    RenderPass::RenderPass(RenderPass&& other) noexcept : m_device(other.m_device), m_render_pass(other.m_render_pass)
    {
        other.m_device = VK_NULL_HANDLE;
        other.m_render_pass = VK_NULL_HANDLE;
    }

    RenderPass& RenderPass::operator=(RenderPass&& other) noexcept
    {
        if (this != &other)
        {
            shutdown();
            m_device = other.m_device;
            m_render_pass = other.m_render_pass;
            other.m_device = VK_NULL_HANDLE;
            other.m_render_pass = VK_NULL_HANDLE;
        }
        return *this;
    }

    bool RenderPass::initialize(const Device& device, const SwapChain& swap_chain)
    {
        if (m_render_pass)
        {
            shutdown();
        }

        VkDevice vk_device = device.getVulkanDevice();
        m_device = vk_device;

        VkAttachmentDescription color_attachment{};
        color_attachment.format = swap_chain.getImageFormat();
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment_ref{};
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount = 1;
        render_pass_info.pAttachments = &color_attachment;
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;
        render_pass_info.dependencyCount = 1;
        render_pass_info.pDependencies = &dependency;

        VkResult result = vkCreateRenderPass(vk_device, &render_pass_info, nullptr, &m_render_pass);
        if (result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to create render pass: " + std::to_string(result));
            return false;
        }

        Logger::getInstance().debug("Render pass created successfully");
        return true;
    }

    bool RenderPass::initializeColorTarget(const Device& device, VkFormat color_format, VkFormat depth_format)
    {
        if (m_render_pass)
        {
            shutdown();
        }

        VkDevice vk_device = device.getVulkanDevice();
        m_device = vk_device;

        std::vector<VkAttachmentDescription> attachments;
        attachments.reserve(depth_format == VK_FORMAT_UNDEFINED ? 1 : 2);

        VkAttachmentDescription color_attachment{};
        color_attachment.format = color_format;
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachments.push_back(color_attachment);

        VkAttachmentReference color_attachment_ref{};
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depth_attachment_ref{};
        if (depth_format != VK_FORMAT_UNDEFINED)
        {
            VkAttachmentDescription depth_attachment{};
            depth_attachment.format = depth_format;
            depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depth_attachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachments.push_back(depth_attachment);

            depth_attachment_ref.attachment = 1;
            depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;
        subpass.pDepthStencilAttachment = depth_format == VK_FORMAT_UNDEFINED ? nullptr : &depth_attachment_ref;

        VkSubpassDependency dependencies[2]{};
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        render_pass_info.pAttachments = attachments.data();
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;
        render_pass_info.dependencyCount = 2;
        render_pass_info.pDependencies = dependencies;

        VkResult result = vkCreateRenderPass(vk_device, &render_pass_info, nullptr, &m_render_pass);
        if (result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to create color target render pass: " + std::to_string(result));
            return false;
        }

        Logger::getInstance().debug("Color target render pass created successfully");
        return true;
    }

    void RenderPass::shutdown()
    {
        if (m_render_pass)
        {
            vkDestroyRenderPass(m_device, m_render_pass, nullptr);
            m_render_pass = VK_NULL_HANDLE;
        }
        m_device = VK_NULL_HANDLE;
    }

}  // namespace kera
