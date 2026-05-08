#include "kera/renderer/render_pass.h"

#include "kera/renderer/device.h"
#include "kera/renderer/swapchain.h"

#include <vulkan/vulkan.h>

#include <iostream>

namespace kera
{

    RenderPass::RenderPass() : device_(VK_NULL_HANDLE), render_pass_(VK_NULL_HANDLE) {}

    RenderPass::~RenderPass()
    {
        shutdown();
    }

    RenderPass::RenderPass(RenderPass&& other) noexcept : device_(other.device_), render_pass_(other.render_pass_)
    {
        other.device_ = VK_NULL_HANDLE;
        other.render_pass_ = VK_NULL_HANDLE;
    }

    RenderPass& RenderPass::operator=(RenderPass&& other) noexcept
    {
        if (this != &other)
        {
            shutdown();
            device_ = other.device_;
            render_pass_ = other.render_pass_;
            other.device_ = VK_NULL_HANDLE;
            other.render_pass_ = VK_NULL_HANDLE;
        }
        return *this;
    }

    bool RenderPass::initialize(const Device& device, const SwapChain& swapChain)
    {
        if (render_pass_)
        {
            shutdown();
        }

        VkDevice vkDevice = device.getVulkanDevice();
        device_ = vkDevice;

        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapChain.getImageFormat();
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        VkResult result = vkCreateRenderPass(vkDevice, &renderPassInfo, nullptr, &render_pass_);
        if (result != VK_SUCCESS)
        {
            std::cerr << "Failed to create render pass: " << result << std::endl;
            return false;
        }

        std::cout << "Render pass created successfully" << std::endl;
        return true;
    }

    bool RenderPass::initializeColorTarget(const Device& device, VkFormat colorFormat)
    {
        if (render_pass_)
        {
            shutdown();
        }

        VkDevice vkDevice = device.getVulkanDevice();
        device_ = vkDevice;

        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = colorFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkSubpassDependency dependencies[2]{};
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 2;
        renderPassInfo.pDependencies = dependencies;

        VkResult result = vkCreateRenderPass(vkDevice, &renderPassInfo, nullptr, &render_pass_);
        if (result != VK_SUCCESS)
        {
            std::cerr << "Failed to create color target render pass: " << result << std::endl;
            return false;
        }

        std::cout << "Color target render pass created successfully" << std::endl;
        return true;
    }

    void RenderPass::shutdown()
    {
        if (render_pass_)
        {
            vkDestroyRenderPass(device_, render_pass_, nullptr);
            render_pass_ = VK_NULL_HANDLE;
        }
        device_ = VK_NULL_HANDLE;
    }

}  // namespace kera
