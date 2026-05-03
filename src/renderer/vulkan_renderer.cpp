#include "kera/renderer/backend/vulkan/vulkan_renderer.h"

#include <array>
#include <span>
#include <utility>

#include "kera/core/window.h"
#include "kera/renderer/command_buffer.h"
#include "kera/renderer/device.h"
#include "kera/renderer/framebuffer.h"
#include "kera/renderer/instance.h"
#include "kera/renderer/physical_device.h"
#include "kera/renderer/render_pass.h"
#include "kera/renderer/surface.h"
#include "kera/renderer/swapchain.h"
#include "kera/utilities/logger.h"

namespace kera {

    namespace {

        ShaderType toShaderType(ShaderStage stage) 
        {
            switch (stage) 
            {
                case ShaderStage::Vertex:
                    return ShaderType::Vertex;
                case ShaderStage::Fragment:
                    return ShaderType::Fragment;
                case ShaderStage::Compute:
                    return ShaderType::Compute;
                default:
                    return ShaderType::Vertex;
            }
        }

        BufferUsage toBufferUsage(BufferUsageKind usage) 
        {
            switch (usage) 
            {
                case BufferUsageKind::Vertex:
                    return BufferUsage::Vertex;
                case BufferUsageKind::Index:
                    return BufferUsage::Index;
                case BufferUsageKind::Uniform:
                    return BufferUsage::Uniform;
                case BufferUsageKind::Storage:
                    return BufferUsage::Storage;
                default:
                    return BufferUsage::Vertex;
            }
        }

        VkMemoryPropertyFlags toMemoryFlags(MemoryAccess access) 
        {
            switch (access)
            {
                case MemoryAccess::GpuOnly:
                    return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                case MemoryAccess::CpuWrite:
                    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                default:
                    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            }
        }

        ShaderStage shaderTypeToStage(ShaderType type) 
        {
            switch (type) 
            {
                case ShaderType::Vertex:
                    return ShaderStage::Vertex;
                case ShaderType::Fragment:
                    return ShaderStage::Fragment;
                case ShaderType::Compute:
                    return ShaderStage::Compute;
                default:
                    return ShaderStage::Vertex;
            }
        }

        const Shader* findShader(const VulkanShaderProgramResource& program,
            ShaderStage stage) 
        {
            for (const Shader& shader : program.m_shaders) 
            {
                if (shaderTypeToStage(shader.getType()) == stage) 
                {
                    return &shader;
                }
            }
            return nullptr;
        }

        std::array<const Shader*, 2> getGraphicsShaders(
            const VulkanShaderProgramResource& program) 
        {
            return 
            {
                findShader(program, ShaderStage::Vertex),
                findShader(program, ShaderStage::Fragment),
            };
        }

        bool initializeVulkanShaderFromDesc(const Device& device,
            const ShaderModuleDesc& desc,
            Shader& shader) 
        {
            const ShaderType shaderType = toShaderType(desc.stage);
            bool initialized = false;

            switch (desc.source) 
            {
            case ShaderSourceKind::SlangFile:
                initialized = shader.initializeFromSlangFile(
                    device, shaderType, desc.path, desc.entryPoint, desc.searchPaths);
                break;
            case ShaderSourceKind::SpirvFile:
                initialized = shader.initializeFromFile(device, shaderType, desc.path);
                break;
            case ShaderSourceKind::SpirvBinary:
                if (desc.spirvCode.empty()) 
                {
                    Logger::getInstance().error(
                        "ShaderModuleDesc.spirvCode must not be empty for SpirvBinary "
                        "source.");
                    return false;
                }
                initialized = shader.initialize(device, shaderType, desc.spirvCode);
                break;
            default:
                Logger::getInstance().error("Unsupported shader source kind.");
                return false;
            }

            if (!initialized) 
            {
                Logger::getInstance().error(
                    "Failed to create Vulkan shader module from requested source.");
                return false;
            }

            return true;
        }

    }  // namespace

    VulkanRenderer::VulkanRenderer()
        : m_window(nullptr),
        m_imageAvailableSemaphore(VK_NULL_HANDLE),
        m_inFlightFence(VK_NULL_HANDLE) {
    }

    VulkanRenderer::~VulkanRenderer() { shutdown(); }

    bool VulkanRenderer::initialize(Window& window) 
    {
        m_window = &window;

        m_instance = std::make_shared<Instance>();
        if (!m_instance->initialize("Kera Sample", VK_MAKE_VERSION(0, 1, 0), true)) 
        {
            Logger::getInstance().error("Failed to create Vulkan instance");
            return false;
        }

        m_surface = std::make_shared<Surface>();
        if (!m_surface->create(m_instance->getVulkanInstance(), window)) 
        {
            Logger::getInstance().error("Failed to create Vulkan surface");
            return false;
        }

        m_physicalDevice = std::make_shared<PhysicalDevice>();
        if (!m_physicalDevice->initialize(m_instance->getVulkanInstance(),
            m_surface->getVulkanSurface())) 
        {
            Logger::getInstance().error("Failed to select physical device");
            return false;
        }

        m_device = std::make_shared<Device>();
        if (!m_device->initialize(*m_physicalDevice)) 
        {
            Logger::getInstance().error("Failed to create logical device");
            return false;
        }

        if (!recreateSwapchainResources(static_cast<uint32_t>(window.getWidth()),
            static_cast<uint32_t>(window.getHeight()))) 
        {
            Logger::getInstance().error("Failed to create Vulkan swapchain resources");
            return false;
        }

        if (!createSyncObjects()) 
        {
            Logger::getInstance().error(
                "Failed to create Vulkan frame synchronization objects");
            return false;
        }

        return true;
    }

    void VulkanRenderer::shutdown() 
    {
        waitForDeviceIdle();

        m_frames.clear();
        m_graphicsPipelines.clear();
        m_buffers.clear();
        m_shaderPrograms.clear();
        m_shaderModules.clear();

        destroySyncObjects();

        if (m_commandBuffer) 
        {
            m_commandBuffer->shutdown();
            m_commandBuffer.reset();
        }

        if (m_framebuffer) 
        {
            m_framebuffer->shutdown();
            m_framebuffer.reset();
        }

        if (m_renderPass) 
        {
            m_renderPass->shutdown();
            m_renderPass.reset();
        }

        if (m_swapchain) 
        {
            m_swapchain->shutdown();
            m_swapchain.reset();
        }
        if (m_surface) 
        {
            m_surface->destroy();
            m_surface.reset();
        }
        if (m_device) 
        {
            m_device->shutdown();
            m_device.reset();
        }
        m_physicalDevice.reset();
        if (m_instance) 
        {
            m_instance->shutdown();
            m_instance.reset();
        }
        m_window = nullptr;
    }

    Extent2D VulkanRenderer::getDrawableExtent() const 
    {
        if (!m_swapchain) 
        {
            return {};
        }

        const VkExtent2D extent = m_swapchain->getExtent();
        return { extent.width, extent.height };
    }

    bool VulkanRenderer::resize(Extent2D newExtent) 
    {
        if (!m_device || !m_surface || !m_physicalDevice || !m_swapchain) 
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return false;
        }

        if (newExtent.width == 0 || newExtent.height == 0) {
            return true;
        }

        waitForDeviceIdle();

        if (!recreateSwapchainResources(newExtent.width, newExtent.height)) {
            Logger::getInstance().error(
                "Failed to recreate Vulkan swapchain resources.");
            return false;
        }

        if (!createSyncObjects()) 
        {
            Logger::getInstance().error(
                "Failed to recreate Vulkan frame synchronization objects.");
            return false;
        }

        return true;
    }

    ShaderModuleHandle VulkanRenderer::createShaderModule(
        const ShaderModuleDesc& desc) 
    {
        if (!m_device) {
            Logger::getInstance().error("Renderer is not initialized.");
            return {};
        }

        VulkanShaderModuleResource resource{};
        resource.m_stage = desc.stage;
        if (!initializeVulkanShaderFromDesc(*m_device, desc, resource.m_shader)) 
        {
            return {};
        }

        return m_shaderModules.emplace(std::move(resource));
    }

    bool VulkanRenderer::destroyShaderModule(ShaderModuleHandle module) {
        waitForDeviceIdle();
        return m_shaderModules.remove(module);
    }

    ShaderProgramHandle VulkanRenderer::createShaderProgram(
        const ShaderProgramDesc& desc) {
        if (!m_device) 
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return {};
        }

        if (desc.stages.empty()) 
        {
            Logger::getInstance().error("ShaderProgramDesc.stages must not be empty.");
            return {};
        }

        VulkanShaderProgramResource resource{};
        resource.m_shaders.reserve(desc.stages.size());

        bool hasVertexStage = false;
        bool hasFragmentStage = false;
        for (const ShaderModuleDesc& stageDesc : desc.stages) {
            if (stageDesc.stage == ShaderStage::Vertex) 
            {
                if (hasVertexStage) 
                {
                    Logger::getInstance().error(
                        "Shader program contains duplicate vertex stages.");
                    return {};
                }
                hasVertexStage = true;
            }
            else if (stageDesc.stage == ShaderStage::Fragment) {
                if (hasFragmentStage) 
                {
                    Logger::getInstance().error(
                        "Shader program contains duplicate fragment stages.");
                    return {};
                }
                hasFragmentStage = true;
            }

            Shader shader;
            if (!initializeVulkanShaderFromDesc(*m_device, stageDesc, shader)) 
            {
                return {};
            }
            resource.m_shaders.push_back(std::move(shader));
        }

        return m_shaderPrograms.emplace(std::move(resource));
    }

    bool VulkanRenderer::destroyShaderProgram(ShaderProgramHandle program) {
        waitForDeviceIdle();
        return m_shaderPrograms.remove(program);
    }

    BufferHandle VulkanRenderer::createBuffer(const BufferDesc& desc) {
        if (!m_device) 
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return {};
        }

        VulkanBufferResource resource{};
        if (!resource.m_buffer.initialize(
            *m_device, static_cast<VkDeviceSize>(desc.size),
            toBufferUsage(desc.usage), toMemoryFlags(desc.memoryAccess))) 
        {
            Logger::getInstance().error("Failed to create Vulkan buffer.");
            return {};
        }

        return m_buffers.emplace(std::move(resource));
    }

    bool VulkanRenderer::destroyBuffer(BufferHandle buffer) 
    {
        waitForDeviceIdle();
        return m_buffers.remove(buffer);
    }

    bool VulkanRenderer::uploadBuffer(BufferHandle buffer, const void* data,
        std::size_t size, std::size_t offset) 
    {
        VulkanBufferResource* resource = m_buffers.get(buffer);
        if (!resource) {
            Logger::getInstance().error(
                "Invalid buffer handle passed to uploadBuffer.");
            return false;
        }

        if (!resource->m_buffer.copyFrom(data, static_cast<VkDeviceSize>(size),
            static_cast<VkDeviceSize>(offset)))
        {
            Logger::getInstance().error("Failed to upload data to Vulkan buffer.");
            return false;
        }

        return true;
    }

    GraphicsPipelineHandle VulkanRenderer::createGraphicsPipeline(
        const GraphicsPipelineDesc& desc, ShaderProgramHandle program) {
        if (!m_device || !m_renderPass) 
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return {};
        }

        VulkanShaderProgramResource* shaderProgram = m_shaderPrograms.get(program);
        if (!shaderProgram) {
            Logger::getInstance().error(
                "Invalid shader program handle passed to createGraphicsPipeline.");
            return {};
        }

        const auto graphicsShaders = getGraphicsShaders(*shaderProgram);
        if (!graphicsShaders[0] || !graphicsShaders[1]) 
        {
            Logger::getInstance().error(
                "Graphics pipeline creation requires shader program stages for both "
                "vertex and fragment shaders.");
            return {};
        }

        VulkanGraphicsPipelineResource resource{};
        resource.m_desc = desc;
        resource.m_program = program;
        if (!resource.m_pipeline.initialize(
            *m_device, *m_renderPass,
            std::span<const Shader* const>(graphicsShaders.data(),
                graphicsShaders.size()),
            desc)) 
        {
            Logger::getInstance().error("Failed to create Vulkan graphics pipeline.");
            return {};
        }

        return m_graphicsPipelines.emplace(std::move(resource));
    }

    bool VulkanRenderer::destroyGraphicsPipeline(GraphicsPipelineHandle pipeline) 
    {
        waitForDeviceIdle();
        return m_graphicsPipelines.remove(pipeline);
    }

    FrameHandle VulkanRenderer::beginFrame() 
    {
        if (!m_device || !m_swapchain || !m_commandBuffer || !m_framebuffer ||
            !m_renderPass) 
        {
            Logger::getInstance().error(
                "Renderer frame resources are not initialized.");
            return {};
        }

        VkDevice vkDevice = m_device->getVulkanDevice();
        vkWaitForFences(vkDevice, 1, &m_inFlightFence, VK_TRUE, UINT64_MAX);
        vkResetFences(vkDevice, 1, &m_inFlightFence);

        uint32_t imageIndex = 0;
        const VkResult acquireResult = m_swapchain->acquireNextImage(
            m_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) 
        {
            Logger::getInstance().error("Failed to acquire Vulkan swapchain image.");
            return {};
        }

        m_commandBuffer->reset();
        if (!m_commandBuffer->begin()) 
        {
            Logger::getInstance().error("Failed to begin Vulkan command buffer.");
            return {};
        }

        VkFramebuffer framebuffer = m_framebuffer->getFramebuffer(imageIndex);
        if (framebuffer == VK_NULL_HANDLE) 
        {
            Logger::getInstance().error("Failed to get Vulkan framebuffer.");
            return {};
        }

        VulkanFrameResource frame{};
        frame.m_commandBuffer = m_commandBuffer->getVulkanCommandBuffer();
        frame.m_renderPass = m_renderPass->getVulkanRenderPass();
        frame.m_framebuffer = framebuffer;
        frame.m_extent = m_swapchain->getExtent();
        frame.m_imageIndex = imageIndex;
        return m_frames.emplace(frame);
    }

    void VulkanRenderer::beginRenderPass(FrameHandle frameHandle,
        const RenderPassDesc& desc) 
    {
        VulkanFrameResource* frame = m_frames.get(frameHandle);
        if (!frame) 
        {
            Logger::getInstance().error(
                "Invalid frame handle passed to beginRenderPass.");
            return;
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = frame->m_renderPass;
        renderPassInfo.framebuffer = frame->m_framebuffer;
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = frame->m_extent;

        VkClearValue clearColor{};
        clearColor.color = 
        { 
            {desc.clearColor.r, desc.clearColor.g, desc.clearColor.b, desc.clearColor.a} 
        };

        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(frame->m_commandBuffer, &renderPassInfo,
            VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(frame->m_extent.width);
        viewport.height = static_cast<float>(frame->m_extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(frame->m_commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = frame->m_extent;
        vkCmdSetScissor(frame->m_commandBuffer, 0, 1, &scissor);
    }

    void VulkanRenderer::endRenderPass(FrameHandle frameHandle) 
    {
        VulkanFrameResource* frame = m_frames.get(frameHandle);
        if (!frame) 
        {
            Logger::getInstance().error(
                "Invalid frame handle passed to endRenderPass.");
            return;
        }

        vkCmdEndRenderPass(frame->m_commandBuffer);
    }

    void VulkanRenderer::bindPipeline(FrameHandle frameHandle,
        GraphicsPipelineHandle pipelineHandle) 
    {
        VulkanFrameResource* frame = m_frames.get(frameHandle);
        VulkanGraphicsPipelineResource* pipeline =
            m_graphicsPipelines.get(pipelineHandle);

        if (!frame || !pipeline) 
        {
            Logger::getInstance().error("Invalid handle passed to bindPipeline.");
            return;
        }

        vkCmdBindPipeline(frame->m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline->m_pipeline.getVulkanPipeline());
    }

    void VulkanRenderer::bindVertexBuffer(FrameHandle frameHandle, uint32_t slot,
        BufferHandle bufferHandle,
        std::size_t offset) 
    {
        VulkanFrameResource* frame = m_frames.get(frameHandle);
        VulkanBufferResource* buffer = m_buffers.get(bufferHandle);
        if (!frame || !buffer) 
        {
            Logger::getInstance().error("Invalid handle passed to bindVertexBuffer.");
            return;
        }

        VkBuffer vertexBuffers[] = { buffer->m_buffer.getVulkanBuffer() };
        VkDeviceSize offsets[] = { static_cast<VkDeviceSize>(offset) };
        vkCmdBindVertexBuffers(frame->m_commandBuffer, slot, 1, vertexBuffers,
            offsets);
    }

    void VulkanRenderer::bindIndexBuffer(FrameHandle frameHandle,
        BufferHandle bufferHandle,
        IndexFormat format, std::size_t offset) 
    {
        VulkanFrameResource* frame = m_frames.get(frameHandle);
        VulkanBufferResource* buffer = m_buffers.get(bufferHandle);
        if (!frame || !buffer) 
        {
            Logger::getInstance().error("Invalid handle passed to bindIndexBuffer.");
            return;
        }

        const VkIndexType indexType = format == IndexFormat::UInt32
            ? VK_INDEX_TYPE_UINT32
            : VK_INDEX_TYPE_UINT16;
        vkCmdBindIndexBuffer(frame->m_commandBuffer,
            buffer->m_buffer.getVulkanBuffer(),
            static_cast<VkDeviceSize>(offset), indexType);
    }

    void VulkanRenderer::drawIndexed(FrameHandle frameHandle, uint32_t indexCount,
        uint32_t instanceCount) 
    {
        VulkanFrameResource* frame = m_frames.get(frameHandle);
        if (!frame) 
        {
            Logger::getInstance().error("Invalid frame handle passed to drawIndexed.");
            return;
        }

        vkCmdDrawIndexed(frame->m_commandBuffer, indexCount, instanceCount, 0, 0, 0);
    }

    bool VulkanRenderer::endFrame(FrameHandle frameHandle) 
    {
        if (!m_device || !m_commandBuffer || !m_swapchain) 
        {
            Logger::getInstance().error(
                "Renderer frame resources are not initialized.");
            return false;
        }

        VulkanFrameResource* frame = m_frames.get(frameHandle);
        if (!frame) 
        {
            Logger::getInstance().error("Invalid frame handle passed to endFrame.");
            return false;
        }

        if (!m_commandBuffer->end()) 
        {
            Logger::getInstance().error("Failed to end Vulkan command buffer.");
            return false;
        }

        const uint32_t imageIndex = frame->m_imageIndex;
        if (imageIndex >= m_renderFinishedSemaphores.size()) 
        {
            Logger::getInstance().error(
                "Swapchain image index exceeded available render-finished semaphores.");
            return false;
        }

        VkSemaphore renderFinishedSemaphore = m_renderFinishedSemaphores[imageIndex];
        VkPipelineStageFlags waitStages[] = 
        {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        };

        VkCommandBuffer commandBuffers[] = 
        {
            m_commandBuffer->getVulkanCommandBuffer()
        };

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_imageAvailableSemaphore;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = commandBuffers;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderFinishedSemaphore;

        if (vkQueueSubmit(m_device->getGraphicsQueue(), 1, &submitInfo,
            m_inFlightFence) != VK_SUCCESS) 
        {
            Logger::getInstance().error("Failed to submit Vulkan draw command buffer.");
            return false;
        }

        const VkResult presentResult = m_swapchain->present(
            imageIndex, renderFinishedSemaphore, m_device->getPresentQueue());

        if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR) 
        {
            Logger::getInstance().error("Failed to present Vulkan swapchain image.");
            return false;
        }

        m_frames.remove(frameHandle);
        return true;
    }

    bool VulkanRenderer::recreateSwapchainResources(uint32_t width,
        uint32_t height) 
    {
        if (!m_physicalDevice || !m_device || !m_surface) 
        {
            return false;
        }

        m_graphicsPipelines.forEach(
            [](GraphicsPipelineHandle, VulkanGraphicsPipelineResource& resource) 
            {
                resource.m_pipeline.shutdown();
                return true;
            });

        if (m_commandBuffer) 
        {
            m_commandBuffer->shutdown();
            m_commandBuffer.reset();
        }

        if (m_framebuffer) 
        {
            m_framebuffer->shutdown();
            m_framebuffer.reset();
        }

        if (m_renderPass) 
        {
            m_renderPass->shutdown();
            m_renderPass.reset();
        }

        if (m_swapchain) 
        {
            m_swapchain->shutdown();
        }
        else 
        {
            m_swapchain = std::make_shared<SwapChain>();
        }

        if (!m_physicalDevice->initialize(m_instance->getVulkanInstance(),
            m_surface->getVulkanSurface())) 
        {
            return false;
        }

        if (!m_swapchain->initialize(*m_physicalDevice, *m_device,
            m_surface->getVulkanSurface(), width, height)) 
        {
            return false;
        }

        m_renderPass = std::make_unique<RenderPass>();
        if (!m_renderPass->initialize(*m_device, *m_swapchain)) 
        {
            return false;
        }

        m_framebuffer = std::make_unique<Framebuffer>();
        if (!m_framebuffer->initialize(*m_device, *m_renderPass, *m_swapchain)) 
        {
            return false;
        }

        m_commandBuffer = std::make_unique<CommandBuffer>();
        if (!m_commandBuffer->initialize(*m_device)) 
        {
            return false;
        }

        return recreateLiveGraphicsPipelines();
    }

    bool VulkanRenderer::recreateLiveGraphicsPipelines() 
    {
        if (!m_device || !m_renderPass) 
        {
            return false;
        }

        return m_graphicsPipelines.forEach(
            [this](GraphicsPipelineHandle, VulkanGraphicsPipelineResource& resource) 
            {
                VulkanShaderProgramResource* shaderProgram =
                    m_shaderPrograms.get(resource.m_program);
                if (!shaderProgram) 
                {
                    Logger::getInstance().error(
                        "Live graphics pipeline references an invalid shader program.");
                    return false;
                }

                const auto graphicsShaders = getGraphicsShaders(*shaderProgram);
                if (!graphicsShaders[0] || !graphicsShaders[1]) 
                {
                    Logger::getInstance().error(
                        "Live graphics pipeline is missing vertex or fragment shaders.");
                    return false;
                }

                if (!resource.m_pipeline.initialize(
                    *m_device, *m_renderPass,
                    std::span<const Shader* const>(graphicsShaders.data(),
                        graphicsShaders.size()),
                    resource.m_desc)) {
                    Logger::getInstance().error(
                        "Failed to recreate live graphics pipeline.");
                    return false;
                }

                return true;
            });
    }

    void VulkanRenderer::waitForDeviceIdle() 
    {
        if (m_device) 
        {
            vkDeviceWaitIdle(m_device->getVulkanDevice());
        }
    }

    void VulkanRenderer::destroySyncObjects() 
    {
        if (!m_device) 
        {
            return;
        }

        VkDevice vkDevice = m_device->getVulkanDevice();
        if (m_inFlightFence != VK_NULL_HANDLE) 
        {
            vkDestroyFence(vkDevice, m_inFlightFence, nullptr);
            m_inFlightFence = VK_NULL_HANDLE;
        }

        for (VkSemaphore semaphore : m_renderFinishedSemaphores) 
        {
            if (semaphore != VK_NULL_HANDLE) 
            {
                vkDestroySemaphore(vkDevice, semaphore, nullptr);
            }
        }
        m_renderFinishedSemaphores.clear();
        if (m_imageAvailableSemaphore != VK_NULL_HANDLE) 
        {
            vkDestroySemaphore(vkDevice, m_imageAvailableSemaphore, nullptr);
            m_imageAvailableSemaphore = VK_NULL_HANDLE;
        }
    }

    bool VulkanRenderer::createSyncObjects() 
    {
        destroySyncObjects();
        if (!m_swapchain) 
        {
            return false;
        }

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkDevice vkDevice = m_device->getVulkanDevice();
        if (vkCreateSemaphore(vkDevice, &semaphoreInfo, nullptr,
            &m_imageAvailableSemaphore) != VK_SUCCESS) 
        {
            return false;
        }

        m_renderFinishedSemaphores.resize(m_swapchain->getImageCount(),
            VK_NULL_HANDLE);

        for (VkSemaphore& semaphore : m_renderFinishedSemaphores) 
        {
            if (vkCreateSemaphore(vkDevice, &semaphoreInfo, nullptr, &semaphore) !=
                VK_SUCCESS) 
            {
                destroySyncObjects();
                return false;
            }
        }

        if (vkCreateFence(vkDevice, &fenceInfo, nullptr, &m_inFlightFence) !=
            VK_SUCCESS) 
        {
            destroySyncObjects();
            return false;
        }

        return true;
    }
} // namespace kera
