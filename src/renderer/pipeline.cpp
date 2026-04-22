#include "kera/renderer/pipeline.h"
#include "kera/renderer/device.h"
#include "kera/renderer/render_pass.h"
#include "kera/renderer/shader.h"
#include <vulkan/vulkan.h>
#include <iostream>

namespace kera {

namespace {

VkPrimitiveTopology toVkPrimitiveTopology(PrimitiveTopologyKind topology) {
    switch (topology) {
        case PrimitiveTopologyKind::TriangleList:
        default:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}

VkFormat toVkVertexFormat(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float2: return VK_FORMAT_R32G32_SFLOAT;
        case VertexFormat::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
        case VertexFormat::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
        default: return VK_FORMAT_R32G32B32_SFLOAT;
    }
}

} // anonymous namespace

Pipeline::Pipeline()
    : device_(VK_NULL_HANDLE)
    , pipeline_(VK_NULL_HANDLE)
    , pipeline_layout_(VK_NULL_HANDLE) {
}

Pipeline::~Pipeline() {
    shutdown();
}

Pipeline::Pipeline(Pipeline&& other) noexcept
    : device_(other.device_)
    , pipeline_(other.pipeline_)
    , pipeline_layout_(other.pipeline_layout_) {
    other.device_ = VK_NULL_HANDLE;
    other.pipeline_ = VK_NULL_HANDLE;
    other.pipeline_layout_ = VK_NULL_HANDLE;
}

Pipeline& Pipeline::operator=(Pipeline&& other) noexcept {
    if (this != &other) {
        shutdown();
        device_ = other.device_;
        pipeline_ = other.pipeline_;
        pipeline_layout_ = other.pipeline_layout_;

        other.device_ = VK_NULL_HANDLE;
        other.pipeline_ = VK_NULL_HANDLE;
        other.pipeline_layout_ = VK_NULL_HANDLE;
    }
    return *this;
}

bool Pipeline::initialize(
    const Device& device,
    const RenderPass& renderPass,
    std::span<const Shader* const> shaders,
    const GraphicsPipelineDesc& desc) {
    if (pipeline_) {
        shutdown();
    }

    VkDevice vkDevice = device.getVulkanDevice();
    device_ = vkDevice;

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    VkResult result = vkCreatePipelineLayout(vkDevice, &pipelineLayoutInfo, nullptr, &pipeline_layout_);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout: " << result << std::endl;
        return false;
    }

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    shaderStages.reserve(shaders.size());
    for (const Shader* shader : shaders) {
        if (!shader) {
            std::cerr << "Graphics pipeline initialization received a null shader stage." << std::endl;
            vkDestroyPipelineLayout(vkDevice, pipeline_layout_, nullptr);
            pipeline_layout_ = VK_NULL_HANDLE;
            return false;
        }
        shaderStages.push_back(shader->getPipelineStageInfo());
    }

    std::vector<VkVertexInputBindingDescription> bindingDescriptions;
    bindingDescriptions.reserve(desc.vertexLayout.bindings.size());
    for (const VertexBindingDesc& binding : desc.vertexLayout.bindings) {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = binding.binding;
        bindingDescription.stride = binding.stride;
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindingDescriptions.push_back(bindingDescription);
    }

    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    attributeDescriptions.reserve(desc.vertexLayout.attributes.size());
    for (const VertexAttributeDesc& attribute : desc.vertexLayout.attributes) {
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
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.empty() ? nullptr : attributeDescriptions.data();

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
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Dynamic state
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

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
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create graphics pipeline: " << result << std::endl;
        vkDestroyPipelineLayout(vkDevice, pipeline_layout_, nullptr);
        pipeline_layout_ = VK_NULL_HANDLE;
        return false;
    }

    std::cout << "Graphics pipeline created successfully" << std::endl;
    return true;
}

void Pipeline::shutdown() {
    if (pipeline_) {
        vkDestroyPipeline(device_, pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }

    if (pipeline_layout_) {
        vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
        pipeline_layout_ = VK_NULL_HANDLE;
    }
    device_ = VK_NULL_HANDLE;
}

} // namespace kera
