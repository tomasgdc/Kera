#include "kera/renderer/render_pass.h"
#include "kera/renderer/device.h"
#include "kera/renderer/swapchain.h"
#include <vulkan/vulkan.h>
#include <iostream>

namespace kera {

RenderPass::RenderPass()
    : render_pass_(VK_NULL_HANDLE) {
}

RenderPass::~RenderPass() {
    shutdown();
}

RenderPass::RenderPass(RenderPass&& other) noexcept
    : render_pass_(other.render_pass_) {
    other.render_pass_ = VK_NULL_HANDLE;
}

RenderPass& RenderPass::operator=(RenderPass&& other) noexcept {
    if (this != &other) {
        shutdown();
        render_pass_ = other.render_pass_;
        other.render_pass_ = VK_NULL_HANDLE;
    }
    return *this;
}

bool RenderPass::initialize(const Device& device, const SwapChain& swapChain) {
    if (render_pass_) {
        shutdown();
    }

    VkDevice vkDevice = device.getVulkanDevice();

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
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create render pass: " << result << std::endl;
        return false;
    }

    std::cout << "Render pass created successfully" << std::endl;
    return true;
}

void RenderPass::shutdown() {
    if (render_pass_) {
        // TODO: Need device reference
        // vkDestroyRenderPass(device, render_pass_, nullptr);
        render_pass_ = VK_NULL_HANDLE;
    }
}

} // namespace kera