// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/renderer/descriptors.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <span>
#include <vector>

namespace kera
{

    class Device;
    class Shader;

    class Pipeline
    {
    public:
        Pipeline();
        ~Pipeline();

        // Delete copy operations
        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;

        // Move operations
        Pipeline(Pipeline&& other) noexcept;
        Pipeline& operator=(Pipeline&& other) noexcept;

        bool initialize(const Device& device, VkPipelineCache pipeline_cache, VkFormat color_format,
                        VkFormat depth_format, std::span<const Shader* const> shaders,
                        const GraphicsPipelineDesc& desc = {});
        void shutdown();

        VkPipeline getVulkanPipeline() const
        {
            return m_pipeline;
        }
        VkPipelineLayout getPipelineLayout() const
        {
            return m_pipeline_layout;
        }
        VkDescriptorSetLayout getDescriptorSetLayout(uint32_t set) const;

        bool isValid() const
        {
            return m_pipeline != VK_NULL_HANDLE;
        }

    private:
        VkDevice m_device;
        VkPipeline m_pipeline;
        VkPipelineLayout m_pipeline_layout;
        std::vector<VkDescriptorSetLayout> m_descriptor_set_layouts;
    };

}  // namespace kera
