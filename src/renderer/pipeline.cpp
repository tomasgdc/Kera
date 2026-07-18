// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/pipeline.h"

#include "kera/renderer/device.h"
#include "kera/renderer/shader.h"
#include "kera/utilities/logger.h"

#include <vulkan/vulkan.h>

#include <utility>

namespace kera
{
    namespace
    {
        VkPrimitiveTopology toVkPrimitiveTopology(EPrimitiveTopologyKind topology)
        {
            switch (topology)
            {
                case EPrimitiveTopologyKind::TRIANGLE_LIST:
                default:
                    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            }
        }

        VkCullModeFlags toVkCullMode(ECullModeKind cull_mode)
        {
            switch (cull_mode)
            {
                case ECullModeKind::NONE:
                    return VK_CULL_MODE_NONE;
                case ECullModeKind::FRONT:
                    return VK_CULL_MODE_FRONT_BIT;
                case ECullModeKind::BACK:
                default:
                    return VK_CULL_MODE_BACK_BIT;
            }
        }

        VkFrontFace toVkFrontFace(EFrontFaceKind front_face)
        {
            switch (front_face)
            {
                case EFrontFaceKind::COUNTER_CLOCKWISE:
                    return VK_FRONT_FACE_COUNTER_CLOCKWISE;
                case EFrontFaceKind::CLOCKWISE:
                default:
                    return VK_FRONT_FACE_CLOCKWISE;
            }
        }

        VkFormat toVkVertexFormat(EVertexFormat format)
        {
            switch (format)
            {
                case EVertexFormat::FLOAT2:
                    return VK_FORMAT_R32G32_SFLOAT;
                case EVertexFormat::FLOAT3:
                    return VK_FORMAT_R32G32B32_SFLOAT;
                case EVertexFormat::FLOAT4:
                    return VK_FORMAT_R32G32B32A32_SFLOAT;
                default:
                    return VK_FORMAT_R32G32B32_SFLOAT;
            }
        }

        VkVertexInputRate toVkVertexInputRate(EVertexInputRate rate)
        {
            switch (rate)
            {
                case EVertexInputRate::INSTANCE:
                    return VK_VERTEX_INPUT_RATE_INSTANCE;
                case EVertexInputRate::VERTEX:
                default:
                    return VK_VERTEX_INPUT_RATE_VERTEX;
            }
        }

        VkDescriptorType toVkDescriptorType(EDescriptorType type)
        {
            switch (type)
            {
                case EDescriptorType::UNIFORM_BUFFER:
                    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                case EDescriptorType::SAMPLED_IMAGE:
                    return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                case EDescriptorType::SAMPLER:
                    return VK_DESCRIPTOR_TYPE_SAMPLER;
                default:
                    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            }
        }

        VkShaderStageFlags toVkShaderStageFlags(EShaderStage stage)
        {
            switch (stage)
            {
                case EShaderStage::VERTEX:
                    return VK_SHADER_STAGE_VERTEX_BIT;
                case EShaderStage::FRAGMENT:
                    return VK_SHADER_STAGE_FRAGMENT_BIT;
                case EShaderStage::COMPUTE:
                    return VK_SHADER_STAGE_COMPUTE_BIT;
                case EShaderStage::ALL_GRAPHICS:
                    return VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
                default:
                    return VK_SHADER_STAGE_VERTEX_BIT;
            }
        }

    }  // anonymous namespace

    Pipeline::Pipeline() : m_device(VK_NULL_HANDLE), m_pipeline(VK_NULL_HANDLE), m_pipeline_layout(VK_NULL_HANDLE) {}

    Pipeline::~Pipeline()
    {
        shutdown();
    }

    Pipeline::Pipeline(Pipeline&& other) noexcept
        : m_device(other.m_device)
        , m_pipeline(other.m_pipeline)
        , m_pipeline_layout(other.m_pipeline_layout)
        , m_descriptor_set_layouts(std::move(other.m_descriptor_set_layouts))
    {
        other.m_device = VK_NULL_HANDLE;
        other.m_pipeline = VK_NULL_HANDLE;
        other.m_pipeline_layout = VK_NULL_HANDLE;
        other.m_descriptor_set_layouts.clear();
    }

    Pipeline& Pipeline::operator=(Pipeline&& other) noexcept
    {
        if (this != &other)
        {
            shutdown();
            m_device = other.m_device;
            m_pipeline = other.m_pipeline;
            m_pipeline_layout = other.m_pipeline_layout;
            m_descriptor_set_layouts = std::move(other.m_descriptor_set_layouts);

            other.m_device = VK_NULL_HANDLE;
            other.m_pipeline = VK_NULL_HANDLE;
            other.m_pipeline_layout = VK_NULL_HANDLE;
            other.m_descriptor_set_layouts.clear();
        }
        return *this;
    }

    bool Pipeline::initialize(const Device& device, VkPipelineCache pipeline_cache, VkFormat color_format,
                              VkFormat depth_format, std::span<const Shader* const> shaders,
                              const GraphicsPipelineDesc& desc)
    {
        if (m_pipeline || m_pipeline_layout || !m_descriptor_set_layouts.empty())
        {
            shutdown();
        }

        VkDevice vk_device = device.getVulkanDevice();
        m_device = vk_device;

        m_descriptor_set_layouts.reserve(desc.descriptor_sets.size());
        for (uint32_t set_index = 0; set_index < static_cast<uint32_t>(desc.descriptor_sets.size()); ++set_index)
        {
            const DescriptorSetLayoutDesc& set_desc = desc.descriptor_sets[set_index];
            if (set_desc.set != set_index)
            {
                Logger::getInstance().error("Descriptor set layouts must be dense and sorted from set 0.");
                return false;
            }

            std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
            layout_bindings.reserve(set_desc.bindings.size());
            for (const DescriptorBindingDesc& binding : set_desc.bindings)
            {
                VkDescriptorSetLayoutBinding layout_binding{};
                layout_binding.binding = binding.binding;
                layout_binding.descriptorType = toVkDescriptorType(binding.type);
                layout_binding.descriptorCount = binding.count;
                layout_binding.stageFlags = toVkShaderStageFlags(binding.stage);
                layout_bindings.push_back(layout_binding);
            }

            VkDescriptorSetLayoutCreateInfo layout_info{};
            layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_info.bindingCount = static_cast<uint32_t>(layout_bindings.size());
            layout_info.pBindings = layout_bindings.empty() ? nullptr : layout_bindings.data();

            VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
            VkResult result = vkCreateDescriptorSetLayout(vk_device, &layout_info, nullptr, &descriptor_set_layout);
            if (result != VK_SUCCESS)
            {
                Logger::getInstance().error("Failed to create descriptor set layout: " + std::to_string(result));
                shutdown();
                return false;
            }

            m_descriptor_set_layouts.push_back(descriptor_set_layout);
        }

        // Create pipeline layout
        VkPipelineLayoutCreateInfo pipeline_layout_info{};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(m_descriptor_set_layouts.size());
        pipeline_layout_info.pSetLayouts = m_descriptor_set_layouts.empty() ? nullptr : m_descriptor_set_layouts.data();

        VkResult result = vkCreatePipelineLayout(vk_device, &pipeline_layout_info, nullptr, &m_pipeline_layout);
        if (result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to create pipeline layout: " + std::to_string(result));
            shutdown();
            return false;
        }

        std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
        shader_stages.reserve(shaders.size());
        for (const Shader* shader : shaders)
        {
            if (!shader)
            {
                Logger::getInstance().error("Graphics pipeline initialization received a null shader stage.");
                shutdown();
                return false;
            }
            shader_stages.push_back(shader->getPipelineStageInfo());
        }

        std::vector<VkVertexInputBindingDescription> binding_descriptions;
        binding_descriptions.reserve(desc.vertex_layout.bindings.size());
        for (const VertexBindingDesc& binding : desc.vertex_layout.bindings)
        {
            VkVertexInputBindingDescription binding_description{};
            binding_description.binding = binding.binding;
            binding_description.stride = binding.stride;
            binding_description.inputRate = toVkVertexInputRate(binding.input_rate);
            binding_descriptions.push_back(binding_description);
        }

        std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
        attribute_descriptions.reserve(desc.vertex_layout.attributes.size());
        for (const VertexAttributeDesc& attribute : desc.vertex_layout.attributes)
        {
            VkVertexInputAttributeDescription attribute_description{};
            attribute_description.location = attribute.location;
            attribute_description.binding = attribute.binding;
            attribute_description.format = toVkVertexFormat(attribute.format);
            attribute_description.offset = attribute.offset;
            attribute_descriptions.push_back(attribute_description);
        }

        VkPipelineVertexInputStateCreateInfo vertex_input_info{};
        vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(binding_descriptions.size());
        vertex_input_info.pVertexBindingDescriptions = binding_descriptions.empty() ? nullptr : binding_descriptions.data();
        vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
        vertex_input_info.pVertexAttributeDescriptions =
            attribute_descriptions.empty() ? nullptr : attribute_descriptions.data();

        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo input_assembly{};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = toVkPrimitiveTopology(desc.topology);

        // Viewport and scissor
        VkPipelineViewportStateCreateInfo viewport_state{};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.scissorCount = 1;

        // Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = toVkCullMode(desc.cull_mode);
        rasterizer.frontFace = toVkFrontFace(desc.front_face);

        // Multisampling
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // Color blending
        VkPipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        if (desc.blend_mode == EBlendModeKind::ALPHA)
        {
            color_blend_attachment.blendEnable = VK_TRUE;
            color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
            color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        }

        VkPipelineColorBlendStateCreateInfo color_blending{};
        color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending.attachmentCount = 1;
        color_blending.pAttachments = &color_blend_attachment;

        VkPipelineDepthStencilStateCreateInfo depth_stencil{};
        depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil.depthTestEnable = desc.depth_test ? VK_TRUE : VK_FALSE;
        depth_stencil.depthWriteEnable = desc.depth_write ? VK_TRUE : VK_FALSE;
        depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depth_stencil.depthBoundsTestEnable = VK_FALSE;
        depth_stencil.stencilTestEnable = VK_FALSE;

        // Dynamic state
        std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dynamic_state{};
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
        dynamic_state.pDynamicStates = dynamic_states.data();

        VkPipelineRenderingCreateInfo rendering_info{};
        rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachmentFormats = &color_format;
        rendering_info.depthAttachmentFormat = depth_format;

        // Pipeline creation
        VkGraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.pNext = &rendering_info;
        pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
        pipeline_info.pStages = shader_stages.data();
        pipeline_info.pVertexInputState = &vertex_input_info;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.pDepthStencilState = depth_format != VK_FORMAT_UNDEFINED ? &depth_stencil : nullptr;
        pipeline_info.pColorBlendState = &color_blending;
        pipeline_info.pDynamicState = &dynamic_state;
        pipeline_info.layout = m_pipeline_layout;
        pipeline_info.renderPass = VK_NULL_HANDLE;
        pipeline_info.subpass = 0;

        result = vkCreateGraphicsPipelines(vk_device, pipeline_cache, 1, &pipeline_info, nullptr, &m_pipeline);
        if (result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to create graphics pipeline: " + std::to_string(result));
            shutdown();
            return false;
        }

        Logger::getInstance().debug("Graphics pipeline created successfully");
        return true;
    }

    void Pipeline::shutdown()
    {
        if (m_pipeline)
        {
            vkDestroyPipeline(m_device, m_pipeline, nullptr);
            m_pipeline = VK_NULL_HANDLE;
        }

        if (m_pipeline_layout)
        {
            vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
            m_pipeline_layout = VK_NULL_HANDLE;
        }

        for (VkDescriptorSetLayout descriptor_set_layout : m_descriptor_set_layouts)
        {
            if (descriptor_set_layout != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorSetLayout(m_device, descriptor_set_layout, nullptr);
            }
        }
        m_descriptor_set_layouts.clear();

        m_device = VK_NULL_HANDLE;
    }

    VkDescriptorSetLayout Pipeline::getDescriptorSetLayout(uint32_t set) const
    {
        if (set >= m_descriptor_set_layouts.size())
        {
            return VK_NULL_HANDLE;
        }

        return m_descriptor_set_layouts[set];
    }
}  // namespace kera
