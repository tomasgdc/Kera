#include "kera/renderer/shader.h"
#include "kera/renderer/device.h"
#include <vulkan/vulkan.h>
#include <fstream>
#include <iostream>

namespace kera {

namespace {

VkShaderStageFlagBits shaderTypeToStageFlag(ShaderType type) {
    switch (type) {
        case ShaderType::Vertex: return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderType::Fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderType::Compute: return VK_SHADER_STAGE_COMPUTE_BIT;
        case ShaderType::Geometry: return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderType::TessellationControl: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case ShaderType::TessellationEvaluation: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        default: return VK_SHADER_STAGE_VERTEX_BIT;
    }
}

} // anonymous namespace

Shader::Shader()
    : shader_module_(VK_NULL_HANDLE)
    , type_(ShaderType::Vertex) {
}

Shader::~Shader() {
    shutdown();
}

Shader::Shader(Shader&& other) noexcept
    : shader_module_(other.shader_module_)
    , type_(other.type_) {
    other.shader_module_ = VK_NULL_HANDLE;
    other.type_ = ShaderType::Vertex;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        shutdown();
        shader_module_ = other.shader_module_;
        type_ = other.type_;

        other.shader_module_ = VK_NULL_HANDLE;
        other.type_ = ShaderType::Vertex;
    }
    return *this;
}

bool Shader::initialize(const Device& device, ShaderType type, const std::vector<uint32_t>& spirvCode) {
    if (shader_module_) {
        shutdown();
    }

    VkDevice vkDevice = device.getVulkanDevice();
    type_ = type;

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirvCode.size() * sizeof(uint32_t);
    createInfo.pCode = spirvCode.data();

    VkResult result = vkCreateShaderModule(vkDevice, &createInfo, nullptr, &shader_module_);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create shader module: " << result << std::endl;
        return false;
    }

    std::cout << "Shader module created successfully" << std::endl;
    return true;
}

bool Shader::initializeFromFile(const Device& device, ShaderType type, const std::string& filepath) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Failed to open shader file: " << filepath << std::endl;
        return false;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    file.close();

    return initialize(device, type, buffer);
}

void Shader::shutdown() {
    if (shader_module_) {
        // TODO: Need device reference
        // vkDestroyShaderModule(device, shader_module_, nullptr);
        shader_module_ = VK_NULL_HANDLE;
    }
}

VkPipelineShaderStageCreateInfo Shader::getPipelineStageInfo() const {
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = shaderTypeToStageFlag(type_);
    shaderStageInfo.module = shader_module_;
    shaderStageInfo.pName = "main";

    return shaderStageInfo;
}

} // namespace kera