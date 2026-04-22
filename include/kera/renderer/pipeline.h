#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace kera {

class Device;
class RenderPass;
class Shader;

class Pipeline {
public:
    Pipeline();
    ~Pipeline();

    // Delete copy operations
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    // Move operations
    Pipeline(Pipeline&& other) noexcept;
    Pipeline& operator=(Pipeline&& other) noexcept;

    bool initialize(const Device& device, const RenderPass& renderPass, const Shader& vertexShader, const Shader& fragmentShader);
    void shutdown();

    VkPipeline getVulkanPipeline() const { return pipeline_; }
    VkPipelineLayout getPipelineLayout() const { return pipeline_layout_; }

    bool isValid() const { return pipeline_ != VK_NULL_HANDLE; }

private:
    VkDevice device_;
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
};

} // namespace kera
