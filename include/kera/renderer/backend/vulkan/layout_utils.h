// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vulkan/vulkan.h>

namespace kera
{
    inline VkAccessFlags2 vulkanAccessMaskForImageLayout(VkImageLayout layout) noexcept
    {
        switch (layout)
        {
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                return VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                return VK_ACCESS_2_TRANSFER_WRITE_BIT;
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                return VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            case VK_IMAGE_LAYOUT_UNDEFINED:
            default:
                return 0;
        }
    }

    inline VkPipelineStageFlags2 vulkanStageMaskForImageLayout(VkImageLayout layout) noexcept
    {
        switch (layout)
        {
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                return VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                return VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            case VK_IMAGE_LAYOUT_UNDEFINED:
            default:
                return VK_PIPELINE_STAGE_2_NONE;
        }
    }

}  // namespace kera
