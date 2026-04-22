#include "kera/renderer/backend/vulkan/vulkan_renderer.h"

#include "kera/core/window.h"
#include "kera/renderer/buffer.h"
#include "kera/renderer/command_buffer.h"
#include "kera/renderer/device.h"
#include "kera/renderer/framebuffer.h"
#include "kera/renderer/instance.h"
#include "kera/renderer/physical_device.h"
#include "kera/renderer/pipeline.h"
#include "kera/renderer/render_pass.h"
#include "kera/renderer/shader.h"
#include "kera/renderer/surface.h"
#include "kera/renderer/swapchain.h"
#include "kera/utilities/logger.h"

#include <array>
#include <utility>

namespace kera {

namespace {

ShaderType toShaderType(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::Vertex: return ShaderType::Vertex;
        case ShaderStage::Fragment: return ShaderType::Fragment;
        case ShaderStage::Compute: return ShaderType::Compute;
        default: return ShaderType::Vertex;
    }
}

BufferUsage toBufferUsage(BufferUsageKind usage) {
    switch (usage) {
        case BufferUsageKind::Vertex: return BufferUsage::Vertex;
        case BufferUsageKind::Index: return BufferUsage::Index;
        case BufferUsageKind::Uniform: return BufferUsage::Uniform;
        case BufferUsageKind::Storage: return BufferUsage::Storage;
        default: return BufferUsage::Vertex;
    }
}

VkMemoryPropertyFlags toMemoryFlags(MemoryAccess access) {
    switch (access) {
        case MemoryAccess::GpuOnly:
            return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        case MemoryAccess::CpuWrite:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        default:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
}

class VulkanShaderModule final : public IShaderModule {
public:
    VulkanShaderModule(std::shared_ptr<Shader> shader, ShaderStage stage)
        : shader_(std::move(shader)), stage_(stage) {}

    ShaderStage getStage() const override { return stage_; }

    Shader& shader() const { return *shader_; }

private:
    std::shared_ptr<Shader> shader_;
    ShaderStage stage_;
};

class VulkanBuffer final : public IBuffer {
public:
    explicit VulkanBuffer(std::shared_ptr<Buffer> buffer)
        : buffer_(std::move(buffer)) {}

    std::size_t getSize() const override {
        return static_cast<std::size_t>(buffer_->getSize());
    }

    Result<void> upload(const void* data, std::size_t size, std::size_t offset = 0) override {
        if (!buffer_->copyFrom(data, size, offset)) {
            return failure("Failed to upload data to Vulkan buffer.");
        }
        return success();
    }

    Buffer& buffer() const { return *buffer_; }

private:
    std::shared_ptr<Buffer> buffer_;
};

class VulkanShaderProgram final : public IShaderProgram {
public:
    explicit VulkanShaderProgram(std::vector<std::shared_ptr<Shader>> shaders)
        : shaders_(std::move(shaders)) {}

    const Shader* findShader(ShaderStage stage) const {
        for (const auto& shader : shaders_) {
            if (shader && shaderTypeToStage(shader->getType()) == stage) {
                return shader.get();
            }
        }
        return nullptr;
    }

    std::array<const Shader*, 2> getGraphicsShaders() const {
        return {
            findShader(ShaderStage::Vertex),
            findShader(ShaderStage::Fragment),
        };
    }

private:
    static ShaderStage shaderTypeToStage(ShaderType type) {
        switch (type) {
            case ShaderType::Vertex: return ShaderStage::Vertex;
            case ShaderType::Fragment: return ShaderStage::Fragment;
            case ShaderType::Compute: return ShaderStage::Compute;
            default: return ShaderStage::Vertex;
        }
    }

    std::vector<std::shared_ptr<Shader>> shaders_;
};

class VulkanGraphicsPipeline final : public IGraphicsPipeline {
public:
    explicit VulkanGraphicsPipeline(std::shared_ptr<Pipeline> pipeline)
        : pipeline_(std::move(pipeline)) {}

    Pipeline& pipeline() const { return *pipeline_; }

private:
    std::shared_ptr<Pipeline> pipeline_;
};

class VulkanFrame final : public IFrame {
public:
    VulkanFrame(
        VulkanRenderer& renderer,
        VkCommandBuffer commandBuffer,
        VkRenderPass renderPass,
        VkFramebuffer framebuffer,
        VkExtent2D extent,
        uint32_t imageIndex)
        : renderer_(renderer)
        , commandBuffer_(commandBuffer)
        , renderPass_(renderPass)
        , framebuffer_(framebuffer)
        , extent_(extent)
        , imageIndex_(imageIndex) {}

    void beginRenderPass(const RenderPassDesc& desc) override {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass_;
        renderPassInfo.framebuffer = framebuffer_;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = extent_;

        VkClearValue clearColor{};
        clearColor.color = {{desc.clearColor.r, desc.clearColor.g, desc.clearColor.b, desc.clearColor.a}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffer_, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent_.width);
        viewport.height = static_cast<float>(extent_.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer_, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = extent_;
        vkCmdSetScissor(commandBuffer_, 0, 1, &scissor);
    }

    void endRenderPass() override {
        vkCmdEndRenderPass(commandBuffer_);
    }

    void bindPipeline(IGraphicsPipeline& pipeline) override {
        auto& vulkanPipeline = dynamic_cast<VulkanGraphicsPipeline&>(pipeline);
        vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanPipeline.pipeline().getVulkanPipeline());
    }

    void bindVertexBuffer(uint32_t slot, IBuffer& buffer, std::size_t offset = 0) override {
        auto& vulkanBuffer = dynamic_cast<VulkanBuffer&>(buffer);
        VkBuffer vertexBuffers[] = { vulkanBuffer.buffer().getVulkanBuffer() };
        VkDeviceSize offsets[] = { static_cast<VkDeviceSize>(offset) };
        vkCmdBindVertexBuffers(commandBuffer_, slot, 1, vertexBuffers, offsets);
    }

    void bindIndexBuffer(IBuffer& buffer, IndexFormat format, std::size_t offset = 0) override {
        auto& vulkanBuffer = dynamic_cast<VulkanBuffer&>(buffer);
        const VkIndexType indexType = format == IndexFormat::UInt32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
        vkCmdBindIndexBuffer(
            commandBuffer_,
            vulkanBuffer.buffer().getVulkanBuffer(),
            static_cast<VkDeviceSize>(offset),
            indexType);
    }

    void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1) override {
        vkCmdDrawIndexed(commandBuffer_, indexCount, instanceCount, 0, 0, 0);
    }

    uint32_t imageIndex() const { return imageIndex_; }

private:
    VulkanRenderer& renderer_;
    VkCommandBuffer commandBuffer_;
    VkRenderPass renderPass_;
    VkFramebuffer framebuffer_;
    VkExtent2D extent_;
    uint32_t imageIndex_;
};

} // namespace

VulkanRenderer::VulkanRenderer()
    : window_(nullptr)
    , imageAvailableSemaphore_(VK_NULL_HANDLE)
    , inFlightFence_(VK_NULL_HANDLE) {}

VulkanRenderer::~VulkanRenderer() {
    shutdown();
}

bool VulkanRenderer::initialize(Window& window) {
    window_ = &window;

    instance_ = std::make_shared<Instance>();
    if (!instance_->initialize("Kera Sample", VK_MAKE_VERSION(0, 1, 0), true)) {
        Logger::getInstance().error("Failed to create Vulkan instance");
        return false;
    }

    surface_ = std::make_shared<Surface>();
    if (!surface_->create(instance_->getVulkanInstance(), window)) {
        Logger::getInstance().error("Failed to create Vulkan surface");
        return false;
    }

    physicalDevice_ = std::make_shared<PhysicalDevice>();
    if (!physicalDevice_->initialize(instance_->getVulkanInstance(), surface_->getVulkanSurface())) {
        Logger::getInstance().error("Failed to select physical device");
        return false;
    }

    device_ = std::make_shared<Device>();
    if (!device_->initialize(*physicalDevice_)) {
        Logger::getInstance().error("Failed to create logical device");
        return false;
    }

    if (!recreateSwapchainResources(static_cast<uint32_t>(window.getWidth()), static_cast<uint32_t>(window.getHeight()))) {
        Logger::getInstance().error("Failed to create Vulkan swapchain resources");
        return false;
    }

    if (!createSyncObjects()) {
        Logger::getInstance().error("Failed to create Vulkan frame synchronization objects");
        return false;
    }

    return true;
}

void VulkanRenderer::shutdown() {
    if (device_) {
        vkDeviceWaitIdle(device_->getVulkanDevice());
    }

    destroySyncObjects();

    if (commandBuffer_) {
        commandBuffer_->shutdown();
        commandBuffer_.reset();
    }
    if (framebuffer_) {
        framebuffer_->shutdown();
        framebuffer_.reset();
    }
    if (renderPass_) {
        renderPass_->shutdown();
        renderPass_.reset();
    }
    if (swapchain_) {
        swapchain_->shutdown();
        swapchain_.reset();
    }
    if (surface_) {
        surface_->destroy();
        surface_.reset();
    }
    if (device_) {
        device_->shutdown();
        device_.reset();
    }
    physicalDevice_.reset();
    if (instance_) {
        instance_->shutdown();
        instance_.reset();
    }
    window_ = nullptr;
}

Extent2D VulkanRenderer::getDrawableExtent() const {
    if (!swapchain_) {
        return {};
    }

    const VkExtent2D extent = swapchain_->getExtent();
    return { extent.width, extent.height };
}

Result<void> VulkanRenderer::resize(Extent2D newExtent) {
    if (!device_ || !surface_ || !physicalDevice_ || !swapchain_) {
        return failure("Renderer is not initialized.");
    }

    if (newExtent.width == 0 || newExtent.height == 0) {
        return success();
    }

    vkDeviceWaitIdle(device_->getVulkanDevice());

    if (!recreateSwapchainResources(newExtent.width, newExtent.height)) {
        return failure("Failed to recreate Vulkan swapchain resources.");
    }

    if (!createSyncObjects()) {
        return failure("Failed to recreate Vulkan frame synchronization objects.");
    }

    return success();
}

namespace {

Result<std::shared_ptr<Shader>> createVulkanShaderFromDesc(
    const Device& device,
    const ShaderModuleDesc& desc) {
    auto shader = std::make_shared<Shader>();
    const ShaderType shaderType = toShaderType(desc.stage);
    bool initialized = false;

    switch (desc.source) {
        case ShaderSourceKind::SlangFile:
            initialized = shader->initializeFromSlangFile(
                device,
                shaderType,
                desc.path,
                desc.entryPoint,
                desc.searchPaths);
            break;
        case ShaderSourceKind::SpirvFile:
            initialized = shader->initializeFromFile(device, shaderType, desc.path);
            break;
        case ShaderSourceKind::SpirvBinary:
            if (desc.spirvCode.empty()) {
                return failure<std::shared_ptr<Shader>>(
                    "ShaderModuleDesc.spirvCode must not be empty for SpirvBinary source.");
            }
            initialized = shader->initialize(device, shaderType, desc.spirvCode);
            break;
        default:
            return failure<std::shared_ptr<Shader>>("Unsupported shader source kind.");
    }

    if (!initialized) {
        return failure<std::shared_ptr<Shader>>("Failed to create Vulkan shader module from requested source.");
    }

    return success<std::shared_ptr<Shader>>(std::move(shader));
}

} // namespace

Result<std::shared_ptr<IShaderModule>> VulkanRenderer::createShaderModule(const ShaderModuleDesc& desc) {
    if (!device_) {
        return failure<std::shared_ptr<IShaderModule>>("Renderer is not initialized.");
    }

    auto shaderResult = createVulkanShaderFromDesc(*device_, desc);
    if (shaderResult.hasError()) {
        return failure<std::shared_ptr<IShaderModule>>(shaderResult.error());
    }

    return success<std::shared_ptr<IShaderModule>>(
        std::make_shared<VulkanShaderModule>(shaderResult.value(), desc.stage));
}

Result<std::shared_ptr<IShaderProgram>> VulkanRenderer::createShaderProgram(const ShaderProgramDesc& desc) {
    if (!device_) {
        return failure<std::shared_ptr<IShaderProgram>>("Renderer is not initialized.");
    }

    if (desc.stages.empty()) {
        return failure<std::shared_ptr<IShaderProgram>>("ShaderProgramDesc.stages must not be empty.");
    }

    std::vector<std::shared_ptr<Shader>> shaders;
    shaders.reserve(desc.stages.size());

    bool hasVertexStage = false;
    bool hasFragmentStage = false;
    for (const ShaderModuleDesc& stageDesc : desc.stages) {
        if (stageDesc.stage == ShaderStage::Vertex) {
            if (hasVertexStage) {
                return failure<std::shared_ptr<IShaderProgram>>("Shader program contains duplicate vertex stages.");
            }
            hasVertexStage = true;
        } else if (stageDesc.stage == ShaderStage::Fragment) {
            if (hasFragmentStage) {
                return failure<std::shared_ptr<IShaderProgram>>("Shader program contains duplicate fragment stages.");
            }
            hasFragmentStage = true;
        }

        auto shaderResult = createVulkanShaderFromDesc(*device_, stageDesc);
        if (shaderResult.hasError()) {
            return failure<std::shared_ptr<IShaderProgram>>(shaderResult.error());
        }
        shaders.push_back(shaderResult.value());
    }

    return success<std::shared_ptr<IShaderProgram>>(std::make_shared<VulkanShaderProgram>(std::move(shaders)));
}

Result<std::shared_ptr<IBuffer>> VulkanRenderer::createBuffer(const BufferDesc& desc) {
    if (!device_) {
        return failure<std::shared_ptr<IBuffer>>("Renderer is not initialized.");
    }

    auto buffer = std::make_shared<Buffer>();
    if (!buffer->initialize(
            *device_,
            static_cast<VkDeviceSize>(desc.size),
            toBufferUsage(desc.usage),
            toMemoryFlags(desc.memoryAccess))) {
        return failure<std::shared_ptr<IBuffer>>("Failed to create Vulkan buffer.");
    }

    return success<std::shared_ptr<IBuffer>>(std::make_shared<VulkanBuffer>(buffer));
}

Result<std::shared_ptr<IGraphicsPipeline>> VulkanRenderer::createGraphicsPipeline(
    const GraphicsPipelineDesc& desc,
    IShaderProgram& program) {
    if (!device_ || !renderPass_) {
        return failure<std::shared_ptr<IGraphicsPipeline>>("Renderer is not initialized.");
    }

    auto* vkProgram = dynamic_cast<VulkanShaderProgram*>(&program);
    if (!vkProgram) {
        return failure<std::shared_ptr<IGraphicsPipeline>>(
            "Unexpected shader program implementation passed to VulkanRenderer::createGraphicsPipeline.");
    }

    const auto graphicsShaders = vkProgram->getGraphicsShaders();
    if (!graphicsShaders[0] || !graphicsShaders[1]) {
        return failure<std::shared_ptr<IGraphicsPipeline>>(
            "Graphics pipeline creation requires shader program stages for both vertex and fragment shaders.");
    }

    auto pipeline = std::make_shared<Pipeline>();
    if (!pipeline->initialize(
            *device_,
            *renderPass_,
            graphicsShaders,
            desc)) {
        return failure<std::shared_ptr<IGraphicsPipeline>>("Failed to create Vulkan graphics pipeline.");
    }

    return success<std::shared_ptr<IGraphicsPipeline>>(std::make_shared<VulkanGraphicsPipeline>(pipeline));
}

Result<std::unique_ptr<IFrame>> VulkanRenderer::beginFrame() {
    if (!device_ || !swapchain_ || !commandBuffer_ || !framebuffer_ || !renderPass_) {
        return failure<std::unique_ptr<IFrame>>("Renderer frame resources are not initialized.");
    }

    VkDevice vkDevice = device_->getVulkanDevice();
    vkWaitForFences(vkDevice, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);
    vkResetFences(vkDevice, 1, &inFlightFence_);

    uint32_t imageIndex = 0;
    const VkResult acquireResult = swapchain_->acquireNextImage(imageAvailableSemaphore_, VK_NULL_HANDLE, &imageIndex);
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        return failure<std::unique_ptr<IFrame>>("Failed to acquire Vulkan swapchain image.");
    }

    commandBuffer_->reset();
    if (!commandBuffer_->begin()) {
        return failure<std::unique_ptr<IFrame>>("Failed to begin Vulkan command buffer.");
    }

    VkFramebuffer framebuffer = framebuffer_->getFramebuffer(imageIndex);
    if (framebuffer == VK_NULL_HANDLE) {
        return failure<std::unique_ptr<IFrame>>("Failed to get Vulkan framebuffer.");
    }

    return success<std::unique_ptr<IFrame>>(std::make_unique<VulkanFrame>(
        *this,
        commandBuffer_->getVulkanCommandBuffer(),
        renderPass_->getVulkanRenderPass(),
        framebuffer,
        swapchain_->getExtent(),
        imageIndex));
}

Result<void> VulkanRenderer::endFrame(std::unique_ptr<IFrame> frame) {
    if (!device_ || !commandBuffer_ || !swapchain_) {
        return failure("Renderer frame resources are not initialized.");
    }

    auto* vulkanFrame = dynamic_cast<VulkanFrame*>(frame.get());
    if (!vulkanFrame) {
        return failure("Unexpected frame implementation passed to VulkanRenderer::endFrame.");
    }

    if (!commandBuffer_->end()) {
        return failure("Failed to end Vulkan command buffer.");
    }

    const uint32_t imageIndex = vulkanFrame->imageIndex();
    if (imageIndex >= renderFinishedSemaphores_.size()) {
        return failure("Swapchain image index exceeded available render-finished semaphores.");
    }

    VkSemaphore renderFinishedSemaphore = renderFinishedSemaphores_[imageIndex];

    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkCommandBuffer commandBuffers[] = { commandBuffer_->getVulkanCommandBuffer() };

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imageAvailableSemaphore_;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = commandBuffers;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderFinishedSemaphore;

    if (vkQueueSubmit(device_->getGraphicsQueue(), 1, &submitInfo, inFlightFence_) != VK_SUCCESS) {
        return failure("Failed to submit Vulkan draw command buffer.");
    }

    const VkResult presentResult = swapchain_->present(
        imageIndex,
        renderFinishedSemaphore,
        device_->getPresentQueue());
    if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR) {
        return failure("Failed to present Vulkan swapchain image.");
    }

    return success();
}

bool VulkanRenderer::recreateSwapchainResources(uint32_t width, uint32_t height) {
    if (!physicalDevice_ || !device_ || !surface_) {
        return false;
    }

    if (commandBuffer_) {
        commandBuffer_->shutdown();
        commandBuffer_.reset();
    }
    if (framebuffer_) {
        framebuffer_->shutdown();
        framebuffer_.reset();
    }
    if (renderPass_) {
        renderPass_->shutdown();
        renderPass_.reset();
    }
    if (swapchain_) {
        swapchain_->shutdown();
    } else {
        swapchain_ = std::make_shared<SwapChain>();
    }

    if (!physicalDevice_->initialize(instance_->getVulkanInstance(), surface_->getVulkanSurface())) {
        return false;
    }

    if (!swapchain_->initialize(*physicalDevice_, *device_, surface_->getVulkanSurface(), width, height)) {
        return false;
    }

    renderPass_ = std::make_unique<RenderPass>();
    if (!renderPass_->initialize(*device_, *swapchain_)) {
        return false;
    }

    framebuffer_ = std::make_unique<Framebuffer>();
    if (!framebuffer_->initialize(*device_, *renderPass_, *swapchain_)) {
        return false;
    }

    commandBuffer_ = std::make_unique<CommandBuffer>();
    if (!commandBuffer_->initialize(*device_)) {
        return false;
    }

    return true;
}

void VulkanRenderer::destroySyncObjects() {
    if (!device_) {
        return;
    }

    VkDevice vkDevice = device_->getVulkanDevice();
    if (inFlightFence_ != VK_NULL_HANDLE) {
        vkDestroyFence(vkDevice, inFlightFence_, nullptr);
        inFlightFence_ = VK_NULL_HANDLE;
    }
    for (VkSemaphore semaphore : renderFinishedSemaphores_) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(vkDevice, semaphore, nullptr);
        }
    }
    renderFinishedSemaphores_.clear();
    if (imageAvailableSemaphore_ != VK_NULL_HANDLE) {
        vkDestroySemaphore(vkDevice, imageAvailableSemaphore_, nullptr);
        imageAvailableSemaphore_ = VK_NULL_HANDLE;
    }
}

bool VulkanRenderer::createSyncObjects() {
    destroySyncObjects();

    if (!swapchain_) {
        return false;
    }

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkDevice vkDevice = device_->getVulkanDevice();
    if (vkCreateSemaphore(vkDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphore_) != VK_SUCCESS) {
        return false;
    }

    renderFinishedSemaphores_.resize(swapchain_->getImageCount(), VK_NULL_HANDLE);
    for (VkSemaphore& semaphore : renderFinishedSemaphores_) {
        if (vkCreateSemaphore(vkDevice, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
            destroySyncObjects();
            return false;
        }
    }

    if (vkCreateFence(vkDevice, &fenceInfo, nullptr, &inFlightFence_) != VK_SUCCESS) {
        destroySyncObjects();
        return false;
    }

    return true;
}

} // namespace kera
