#include "basic_triangle_sample.h"
#include "kera/renderer/instance.h"
#include "kera/renderer/device.h"
#include "kera/renderer/surface.h"
#include "kera/renderer/swapchain.h"
#include "kera/renderer/slang_compiler.h"
#include "kera/utilities/logger.h"
#include "kera/utilities/file_utils.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace kera {

    // Simple vertex structure
    struct Vertex {
        glm::vec3 position;
        glm::vec3 color;
    };

    BasicTriangleSample::BasicTriangleSample(
        std::shared_ptr<Instance> instance,
        std::shared_ptr<Device> device,
        std::shared_ptr<Surface> surface,
        std::shared_ptr<SwapChain> swapchain)
        : Sample("Basic Triangle with Slang"),
        instance_(instance),
        device_(device),
        surface_(surface),
        swapchain_(swapchain),
        indexCount_(0),
        rotationAngle_(0.0f) {
    }

    void BasicTriangleSample::initialize() {
        Logger::getInstance().info("Initializing " + std::string(getName()));

        if (!compileShaders()) {
            Logger::getInstance().error("Failed to compile shaders");
            return;
        }

        if (!createGeometry()) {
            Logger::getInstance().error("Failed to create geometry");
            return;
        }

        if (!createPipeline()) {
            Logger::getInstance().error("Failed to create pipeline");
            return;
        }

        if (!createFramebuffers()) {
            Logger::getInstance().error("Failed to create framebuffers");
            return;
        }

        if (!createSyncObjects()) {
            Logger::getInstance().error("Failed to create frame synchronization objects");
            return;
        }

        Logger::getInstance().info("Triangle sample initialized successfully");
    }

    bool BasicTriangleSample::compileShaders() {
        Logger::getInstance().info("Compiling triangle shaders with Slang");

        // Compile vertex shader
        SlangCompileRequest vertexRequest;
        vertexRequest.shaderPath = "shaders/triangle.vert.slang";
        vertexRequest.entryPoint = "main";
        vertexRequest.shaderType = ShaderType::Vertex;

        std::vector<uint32_t> vertexSpirv;
        std::string vertexDiagnostics;
        if (!SlangCompiler::compileToSpirv(vertexRequest, vertexSpirv, &vertexDiagnostics)) {
            Logger::getInstance().error("Failed to compile vertex shader: " + vertexDiagnostics);
            return false;
        }

        // Compile fragment shader
        SlangCompileRequest fragmentRequest;
        fragmentRequest.shaderPath = "shaders/triangle.frag.slang";
        fragmentRequest.entryPoint = "main";
        fragmentRequest.shaderType = ShaderType::Fragment;

        std::vector<uint32_t> fragmentSpirv;
        std::string fragmentDiagnostics;
        if (!SlangCompiler::compileToSpirv(fragmentRequest, fragmentSpirv, &fragmentDiagnostics)) {
            Logger::getInstance().error("Failed to compile fragment shader: " + fragmentDiagnostics);
            return false;
        }

        // Create shader modules
        vertexShader_ = std::make_unique<Shader>();
        if (!vertexShader_->initialize(*device_, ShaderType::Vertex, vertexSpirv)) {
            Logger::getInstance().error("Failed to initialize vertex shader module");
            return false;
        }

        fragmentShader_ = std::make_unique<Shader>();
        if (!fragmentShader_->initialize(*device_, ShaderType::Fragment, fragmentSpirv)) {
            Logger::getInstance().error("Failed to initialize fragment shader module");
            return false;
        }

        Logger::getInstance().info("Shaders compiled and initialized successfully");
        return true;
    }

    bool BasicTriangleSample::createGeometry() {
        Logger::getInstance().info("Creating triangle geometry");

        // Define triangle vertices
        std::vector<Vertex> vertices = {
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},  // Red - bottom left
            {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},   // Green - bottom right
            {{0.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}     // Blue - top
        };

        // Define triangle indices
        std::vector<uint16_t> indices = { 0, 1, 2 };
        indexCount_ = static_cast<uint32_t>(indices.size());

        // Create vertex buffer
        vertexBuffer_ = std::make_unique<Buffer>();
        VkDeviceSize vertexDataSize = vertices.size() * sizeof(Vertex);
        if (!vertexBuffer_->initialize(*device_, vertexDataSize, BufferUsage::Vertex,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            Logger::getInstance().error("Failed to create vertex buffer");
            return false;
        }

        if (!vertexBuffer_->copyFrom(vertices.data(), vertexDataSize)) {
            Logger::getInstance().error("Failed to copy vertex data to buffer");
            return false;
        }

        // Create index buffer
        indexBuffer_ = std::make_unique<Buffer>();
        VkDeviceSize indexDataSize = indices.size() * sizeof(uint16_t);
        if (!indexBuffer_->initialize(*device_, indexDataSize, BufferUsage::Index,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            Logger::getInstance().error("Failed to create index buffer");
            return false;
        }

        if (!indexBuffer_->copyFrom(indices.data(), indexDataSize)) {
            Logger::getInstance().error("Failed to copy index data to buffer");
            return false;
        }

        Logger::getInstance().info("Triangle geometry created successfully");
        return true;
    }

    bool BasicTriangleSample::createPipeline() {
        Logger::getInstance().info("Creating graphics pipeline");

        // Create render pass
        renderPass_ = std::make_unique<RenderPass>();
        if (!renderPass_->initialize(*device_, *swapchain_)) {
            Logger::getInstance().error("Failed to create render pass");
            return false;
        }

        // Create pipeline
        pipeline_ = std::make_unique<Pipeline>();
        if (!pipeline_->initialize(*device_, *renderPass_, *vertexShader_, *fragmentShader_)) {
            Logger::getInstance().error("Failed to create pipeline");
            return false;
        }

        Logger::getInstance().info("Graphics pipeline created successfully");
        return true;
    }

    bool BasicTriangleSample::createFramebuffers() {
        Logger::getInstance().info("Creating framebuffers");

        // Create framebuffers from swapchain images
        framebuffer_ = std::make_unique<Framebuffer>();
        if (!framebuffer_->initialize(*device_, *renderPass_, *swapchain_)) {
            Logger::getInstance().error("Failed to create framebuffers");
            return false;
        }

        // Create command buffer
        commandBuffer_ = std::make_unique<CommandBuffer>();
        if (!commandBuffer_->initialize(*device_)) {
            Logger::getInstance().error("Failed to create command buffer");
            return false;
        }

        Logger::getInstance().info("Framebuffers and command buffer created successfully");
        return true;
    }

    bool BasicTriangleSample::createSyncObjects() {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkDevice vkDevice = device_->getVulkanDevice();
        if (vkCreateSemaphore(vkDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphore_) != VK_SUCCESS ||
            vkCreateSemaphore(vkDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphore_) != VK_SUCCESS ||
            vkCreateFence(vkDevice, &fenceInfo, nullptr, &inFlightFence_) != VK_SUCCESS) {
            Logger::getInstance().error("Failed to create synchronization objects for triangle sample");
            return false;
        }

        return true;
    }

    void BasicTriangleSample::update(float deltaTime) {
        // Update rotation
        rotationAngle_ += deltaTime * 45.0f;  // Rotate 45 degrees per second
        if (rotationAngle_ >= 360.0f) {
            rotationAngle_ -= 360.0f;
        }
    }

    void BasicTriangleSample::render() {
        if (!commandBuffer_ || !pipeline_ || !renderPass_ || !framebuffer_) {
            Logger::getInstance().warning("Render called before resources initialized");
            return;
        }

        VkDevice vkDevice = device_->getVulkanDevice();
        vkWaitForFences(vkDevice, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);
        vkResetFences(vkDevice, 1, &inFlightFence_);

        uint32_t imageIndex = 0;
        VkResult acquireResult = swapchain_->acquireNextImage(imageAvailableSemaphore_, VK_NULL_HANDLE, &imageIndex);
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
            Logger::getInstance().error("Failed to acquire swapchain image");
            return;
        }

        // Reset and begin command buffer
        commandBuffer_->reset();
        if (!commandBuffer_->begin()) {
            Logger::getInstance().error("Failed to begin command buffer");
            return;
        }

        // Get current swapchain extent and framebuffer
        VkExtent2D extent = swapchain_->getExtent();
        uint32_t framebufferCount = framebuffer_->getFramebufferCount();
        
        if (framebufferCount == 0) {
            Logger::getInstance().error("No framebuffers available");
            commandBuffer_->end();
            return;
        }

        VkFramebuffer currentFramebuffer = framebuffer_->getFramebuffer(imageIndex);
        if (currentFramebuffer == VK_NULL_HANDLE) {
            Logger::getInstance().error("Failed to get framebuffer for swapchain image");
            commandBuffer_->end();
            return;
        }

        // Begin render pass
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass_->getVulkanRenderPass();
        renderPassInfo.framebuffer = currentFramebuffer;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = extent;

        // Clear color (dark blue background)
        VkClearValue clearColor{};
        clearColor.color = {{0.0f, 0.0f, 0.1f, 1.0f}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffer_->getVulkanCommandBuffer(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Bind pipeline
        vkCmdBindPipeline(commandBuffer_->getVulkanCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_->getVulkanPipeline());

        // Set viewport
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer_->getVulkanCommandBuffer(), 0, 1, &viewport);

        // Set scissor
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = extent;
        vkCmdSetScissor(commandBuffer_->getVulkanCommandBuffer(), 0, 1, &scissor);

        // Bind vertex buffer
        VkBuffer vertexBuffers[] = {vertexBuffer_->getVulkanBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer_->getVulkanCommandBuffer(), 0, 1, vertexBuffers, offsets);

        // Bind index buffer
        vkCmdBindIndexBuffer(commandBuffer_->getVulkanCommandBuffer(), indexBuffer_->getVulkanBuffer(), 0, VK_INDEX_TYPE_UINT16);

        // Draw indexed
        vkCmdDrawIndexed(commandBuffer_->getVulkanCommandBuffer(), indexCount_, 1, 0, 0, 0);

        // End render pass
        vkCmdEndRenderPass(commandBuffer_->getVulkanCommandBuffer());

        // End command buffer
        if (!commandBuffer_->end()) {
            Logger::getInstance().error("Failed to end command buffer");
            return;
        }

        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imageAvailableSemaphore_;
        submitInfo.pWaitDstStageMask = waitStages;

        VkCommandBuffer commandBuffers[] = { commandBuffer_->getVulkanCommandBuffer() };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = commandBuffers;

        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderFinishedSemaphore_;

        if (vkQueueSubmit(device_->getGraphicsQueue(), 1, &submitInfo, inFlightFence_) != VK_SUCCESS) {
            Logger::getInstance().error("Failed to submit draw command buffer");
            return;
        }

        VkResult presentResult = swapchain_->present(imageIndex, renderFinishedSemaphore_, device_->getPresentQueue());
        if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR) {
            Logger::getInstance().error("Failed to present swapchain image");
            return;
        }

        Logger::getInstance().debug("Triangle rendered successfully");
    }

    void BasicTriangleSample::cleanup() {
        Logger::getInstance().info("Cleaning up " + std::string(getName()));

        if (commandBuffer_) {
            commandBuffer_->shutdown();
        }
        if (framebuffer_) {
            framebuffer_->shutdown();
        }
        if (renderPass_) {
            renderPass_->shutdown();
        }
        if (pipeline_) {
            pipeline_->shutdown();
        }
        if (indexBuffer_) {
            indexBuffer_->shutdown();
        }
        if (vertexBuffer_) {
            vertexBuffer_->shutdown();
        }
        if (fragmentShader_) {
            fragmentShader_->shutdown();
        }
        if (vertexShader_) {
            vertexShader_->shutdown();
        }

        VkDevice vkDevice = device_ ? device_->getVulkanDevice() : VK_NULL_HANDLE;
        if (vkDevice != VK_NULL_HANDLE) {
            if (inFlightFence_ != VK_NULL_HANDLE) {
                vkDestroyFence(vkDevice, inFlightFence_, nullptr);
                inFlightFence_ = VK_NULL_HANDLE;
            }
            if (renderFinishedSemaphore_ != VK_NULL_HANDLE) {
                vkDestroySemaphore(vkDevice, renderFinishedSemaphore_, nullptr);
                renderFinishedSemaphore_ = VK_NULL_HANDLE;
            }
            if (imageAvailableSemaphore_ != VK_NULL_HANDLE) {
                vkDestroySemaphore(vkDevice, imageAvailableSemaphore_, nullptr);
                imageAvailableSemaphore_ = VK_NULL_HANDLE;
            }
        }

        Logger::getInstance().info("Triangle sample cleaned up");
    }
} // namespace kera
