#pragma once

#include "samples.h"
#include "kera/renderer/shader.h"
#include "kera/renderer/buffer.h"
#include "kera/renderer/pipeline.h"
#include "kera/renderer/render_pass.h"
#include "kera/renderer/framebuffer.h"
#include "kera/renderer/command_buffer.h"
#include <memory>

namespace kera {

// Forward declarations
class Instance;
class Device;
class Surface;
class SwapChain;

class BasicTriangleSample : public Sample {
public:
    BasicTriangleSample(
        std::shared_ptr<Instance> instance,
        std::shared_ptr<Device> device,
        std::shared_ptr<Surface> surface,
        std::shared_ptr<SwapChain> swapchain);

    void initialize() override;
    void update(float deltaTime) override;
    void render() override;
    void cleanup() override;

private:
    bool compileShaders();
    bool createGeometry();
    bool createPipeline();
    bool createFramebuffers();
    bool createSyncObjects();

    // Renderer resources
    std::shared_ptr<Instance> instance_;
    std::shared_ptr<Device> device_;
    std::shared_ptr<Surface> surface_;
    std::shared_ptr<SwapChain> swapchain_;

    // Shaders
    std::unique_ptr<Shader> vertexShader_;
    std::unique_ptr<Shader> fragmentShader_;

    // Geometry
    std::unique_ptr<Buffer> vertexBuffer_;
    std::unique_ptr<Buffer> indexBuffer_;
    uint32_t indexCount_;

    // Pipeline
    std::unique_ptr<Pipeline> pipeline_;
    std::unique_ptr<RenderPass> renderPass_;
    std::unique_ptr<Framebuffer> framebuffer_;
    std::unique_ptr<CommandBuffer> commandBuffer_;

    VkSemaphore imageAvailableSemaphore_ = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore_ = VK_NULL_HANDLE;
    VkFence inFlightFence_ = VK_NULL_HANDLE;

    // Animation
    float rotationAngle_;
};

} // namespace kera
