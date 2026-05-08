#include "kera/renderer/pipeline.h"

#include "kera/renderer/device.h"
#include "kera/renderer/render_pass.h"
#include "kera/renderer/shader.h"

#include <vulkan/vulkan.h>

#include <iostream>
#include <utility>

namespace kera
{
    namespace
    {
        VkPrimitiveTopology toVkPrimitiveTopology(PrimitiveTopologyKind topology)
        {
            switch (topology)
            {
                case PrimitiveTopologyKind::TriangleList:
                default:
                    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            }
        }

        VkCullModeFlags toVkCullMode(CullModeKind cullMode)
        {
            switch (cullMode)
            {
                case CullModeKind::None:
                    return VK_CULL_MODE_NONE;
                case CullModeKind::Front:
                    return VK_CULL_MODE_FRONT_BIT;
                case CullModeKind::Back:
                default:
                    return VK_CULL_MODE_BACK_BIT;
            }
        }

        VkFrontFace toVkFrontFace(FrontFaceKind frontFace)
        {
            switch (frontFace)
            {
                case FrontFaceKind::CounterClockwise:
                    return VK_FRONT_FACE_COUNTER_CLOCKWISE;
                case FrontFaceKind::Clockwise:
                default:
                    return VK_FRONT_FACE_CLOCKWISE;
            }
        }

        VkFormat toVkVertexFormat(VertexFormat format)
        {
            switch (format)
            {
                case VertexFormat::Float2:
                    return VK_FORMAT_R32G32_SFLOAT;
                case VertexFormat::Float3:
                    return VK_FORMAT_R32G32B32_SFLOAT;
                case VertexFormat::Float4:
                    return VK_FORMAT_R32G32B32A32_SFLOAT;
                default:
                    return VK_FORMAT_R32G32B32_SFLOAT;
            }
        }

        VkVertexInputRate toVkVertexInputRate(VertexInputRate rate)
        {
            switch (rate)
            {
                case VertexInputRate::Instance:
                    return VK_VERTEX_INPUT_RATE_INSTANCE;
                case VertexInputRate::Vertex:
                default:
                    return VK_VERTEX_INPUT_RATE_VERTEX;
            }
        }

        VkDescriptorType toVkDescriptorType(DescriptorType type)
        {
            switch (type)
            {
                case DescriptorType::UniformBuffer:
                    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                case DescriptorType::SampledImage:
                    return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                case DescriptorType::Sampler:
                    return VK_DESCRIPTOR_TYPE_SAMPLER;
                default:
                    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            }
        }

        VkShaderStageFlags toVkShaderStageFlags(ShaderStage stage)
        {
            switch (stage)
            {
                case ShaderStage::Vertex:
                    return VK_SHADER_STAGE_VERTEX_BIT;
                case ShaderStage::Fragment:
                    return VK_SHADER_STAGE_FRAGMENT_BIT;
                case ShaderStage::Compute:
                    return VK_SHADER_STAGE_COMPUTE_BIT;
                default:
                    return VK_SHADER_STAGE_VERTEX_BIT;
            }
        }

    }  // anonymous namespace

    Pipeline::Pipeline() : device_(VK_NULL_HANDLE), pipeline_(VK_NULL_HANDLE), pipeline_layout_(VK_NULL_HANDLE) {}

    Pipeline::~Pipeline()
    {
        shutdown();
    }

    Pipeline::Pipeline(Pipeline&& other) noexcept
        : device_(other.device_)
        , pipeline_(other.pipeline_)
        , pipeline_layout_(other.pipeline_layout_)
        , descriptor_set_layouts_(std::move(other.descriptor_set_layouts_))
    {
        other.device_ = VK_NULL_HANDLE;
        other.pipeline_ = VK_NULL_HANDLE;
        other.pipeline_layout_ = VK_NULL_HANDLE;
        other.descriptor_set_layouts_.clear();
    }

    Pipeline& Pipeline::operator=(Pipeline&& other) noexcept
    {
        if (this != &other)
        {
            shutdown();
            device_ = other.device_;
            pipeline_ = other.pipeline_;
            pipeline_layout_ = other.pipeline_layout_;
            descriptor_set_layouts_ = std::move(other.descriptor_set_layouts_);

            other.device_ = VK_NULL_HANDLE;
            other.pipeline_ = VK_NULL_HANDLE;
            other.pipeline_layout_ = VK_NULL_HANDLE;
            other.descriptor_set_layouts_.clear();
        }
        return *this;
    }

    bool Pipeline::initialize(const Device& device, const RenderPass& renderPass,
                              std::span<const Shader* const> shaders, const GraphicsPipelineDesc& desc)
    {
        if (pipeline_ || pipeline_layout_ || !descriptor_set_layouts_.empty())
        {
            shutdown();
        }

        VkDevice vkDevice = device.getVulkanDevice();
        device_ = vkDevice;

        descriptor_set_layouts_.reserve(desc.descriptorSets.size());
        for (uint32_t setIndex = 0; setIndex < static_cast<uint32_t>(desc.descriptorSets.size()); ++setIndex)
        {
            const DescriptorSetLayoutDesc& setDesc = desc.descriptorSets[setIndex];
            if (setDesc.set != setIndex)
            {
                std::cerr << "Descriptor set layouts must be dense and sorted from set 0." << std::endl;
                return false;
            }

            std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
            layoutBindings.reserve(setDesc.bindings.size());
            for (const DescriptorBindingDesc& binding : setDesc.bindings)
            {
                VkDescriptorSetLayoutBinding layoutBinding{};
                layoutBinding.binding = binding.binding;
                layoutBinding.descriptorType = toVkDescriptorType(binding.type);
                layoutBinding.descriptorCount = binding.count;
                layoutBinding.stageFlags = toVkShaderStageFlags(binding.stage);
                layoutBindings.push_back(layoutBinding);
            }

            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
            layoutInfo.pBindings = layoutBindings.empty() ? nullptr : layoutBindings.data();

            VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
            VkResult result = vkCreateDescriptorSetLayout(vkDevice, &layoutInfo, nullptr, &descriptorSetLayout);
            if (result != VK_SUCCESS)
            {
                std::cerr << "Failed to create descriptor set layout: " << result << std::endl;
                shutdown();
                return false;
            }

            descriptor_set_layouts_.push_back(descriptorSetLayout);
        }

        // Create pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts_.size());
        pipelineLayoutInfo.pSetLayouts = descriptor_set_layouts_.empty() ? nullptr : descriptor_set_layouts_.data();

        VkResult result = vkCreatePipelineLayout(vkDevice, &pipelineLayoutInfo, nullptr, &pipeline_layout_);
        if (result != VK_SUCCESS)
        {
            std::cerr << "Failed to create pipeline layout: " << result << std::endl;
            shutdown();
            return false;
        }

        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        shaderStages.reserve(shaders.size());
        for (const Shader* shader : shaders)
        {
            if (!shader)
            {
                std::cerr << "Graphics pipeline initialization received a null shader stage." << std::endl;
                shutdown();
                return false;
            }
            shaderStages.push_back(shader->getPipelineStageInfo());
        }

        std::vector<VkVertexInputBindingDescription> bindingDescriptions;
        bindingDescriptions.reserve(desc.vertexLayout.bindings.size());
        for (const VertexBindingDesc& binding : desc.vertexLayout.bindings)
        {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = binding.binding;
            bindingDescription.stride = binding.stride;
            bindingDescription.inputRate = toVkVertexInputRate(binding.inputRate);
            bindingDescriptions.push_back(bindingDescription);
        }

        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
        attributeDescriptions.reserve(desc.vertexLayout.attributes.size());
        for (const VertexAttributeDesc& attribute : desc.vertexLayout.attributes)
        {
            VkVertexInputAttributeDescription attributeDescription{};
            attributeDescription.location = attribute.location;
            attributeDescription.binding = attribute.binding;
            attributeDescription.format = toVkVertexFormat(attribute.format);
            attributeDescription.offset = attribute.offset;
            attributeDescriptions.push_back(attributeDescription);
        }

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.empty() ? nullptr : bindingDescriptions.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions =
            attributeDescriptions.empty() ? nullptr : attributeDescriptions.data();

        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = toVkPrimitiveTopology(desc.topology);

        // Viewport and scissor
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        // Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = toVkCullMode(desc.cullMode);
        rasterizer.frontFace = toVkFrontFace(desc.frontFace);

        // Multisampling
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // Color blending
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        // Dynamic state
        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // Pipeline creation
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipeline_layout_;
        pipelineInfo.renderPass = renderPass.getVulkanRenderPass();
        pipelineInfo.subpass = 0;

        result = vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_);
        if (result != VK_SUCCESS)
        {
            std::cerr << "Failed to create graphics pipeline: " << result << std::endl;
            shutdown();
            return false;
        }

        std::cout << "Graphics pipeline created successfully" << std::endl;
        return true;
    }

    void Pipeline::shutdown()
    {
        if (pipeline_)
        {
            vkDestroyPipeline(device_, pipeline_, nullptr);
            pipeline_ = VK_NULL_HANDLE;
        }

        if (pipeline_layout_)
        {
            vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
            pipeline_layout_ = VK_NULL_HANDLE;
        }

        for (VkDescriptorSetLayout descriptorSetLayout : descriptor_set_layouts_)
        {
            if (descriptorSetLayout != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorSetLayout(device_, descriptorSetLayout, nullptr);
            }
        }
        descriptor_set_layouts_.clear();

        device_ = VK_NULL_HANDLE;
    }

    VkDescriptorSetLayout Pipeline::getDescriptorSetLayout(uint32_t set) const
    {
        if (set >= descriptor_set_layouts_.size())
        {
            return VK_NULL_HANDLE;
        }

        return descriptor_set_layouts_[set];
    }
}  // namespace kera
