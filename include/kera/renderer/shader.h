#pragma once

#include <memory>
#include <vector>
#include <string>
#include <vulkan/vulkan.h>

namespace kera {

enum class ShaderType {
    Vertex,
    Fragment,
    Compute,
    Geometry,
    TessellationControl,
    TessellationEvaluation
};

class Device;

class Shader {
public:
    Shader();
    ~Shader();

    // Delete copy operations
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // Move operations
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    bool initialize(const Device& device, ShaderType type, const std::vector<uint32_t>& spirvCode);
    bool initializeFromFile(const Device& device, ShaderType type, const std::string& filepath);
    void shutdown();

    VkShaderModule getVulkanShaderModule() const { return shader_module_; }
    ShaderType getType() const { return type_; }

    bool isValid() const { return shader_module_ != VK_NULL_HANDLE; }

    // Shader stage info for pipeline creation
    VkPipelineShaderStageCreateInfo getPipelineStageInfo() const;

private:
    VkShaderModule shader_module_;
    ShaderType type_;
};

} // namespace kera