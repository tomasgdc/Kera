#pragma once

#include "kera/renderer/buffer.h"
#include "kera/renderer/framebuffer.h"
#include "kera/renderer/interfaces.h"
#include "kera/renderer/pipeline.h"
#include "kera/renderer/render_pass.h"
#include "kera/renderer/resource_registry.h"
#include "kera/renderer/shader.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <utility>
#include <vector>

namespace kera
{

    class Window;
    class Instance;
    class PhysicalDevice;
    class Device;
    class Surface;
    class SwapChain;
    class RenderPass;
    class Framebuffer;
    class CommandBuffer;

    struct VulkanShaderModuleResource
    {
        Shader m_shader;
        ShaderStage m_stage = ShaderStage::Vertex;
    };

    struct VulkanShaderProgramResource
    {
        std::vector<Shader> m_shaders;
    };

    struct VulkanBufferResource
    {
        Buffer m_buffer;
    };

    struct VulkanTextureResource
    {
        VkDevice m_device = VK_NULL_HANDLE;
        VkImage m_image = VK_NULL_HANDLE;
        VkDeviceMemory m_memory = VK_NULL_HANDLE;
        VkImageView m_imageView = VK_NULL_HANDLE;
        VkFormat m_format = VK_FORMAT_R8G8B8A8_UNORM;
        VkExtent2D m_extent{};

        VulkanTextureResource() = default;
        ~VulkanTextureResource()
        {
            shutdown();
        }

        VulkanTextureResource(const VulkanTextureResource&) = delete;
        VulkanTextureResource& operator=(const VulkanTextureResource&) = delete;

        VulkanTextureResource(VulkanTextureResource&& other) noexcept
            : m_device(std::exchange(other.m_device, VK_NULL_HANDLE))
            , m_image(std::exchange(other.m_image, VK_NULL_HANDLE))
            , m_memory(std::exchange(other.m_memory, VK_NULL_HANDLE))
            , m_imageView(std::exchange(other.m_imageView, VK_NULL_HANDLE))
            , m_format(other.m_format)
            , m_extent(other.m_extent)
        {
        }

        VulkanTextureResource& operator=(VulkanTextureResource&& other) noexcept
        {
            if (this != &other)
            {
                shutdown();
                m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
                m_image = std::exchange(other.m_image, VK_NULL_HANDLE);
                m_memory = std::exchange(other.m_memory, VK_NULL_HANDLE);
                m_imageView = std::exchange(other.m_imageView, VK_NULL_HANDLE);
                m_format = other.m_format;
                m_extent = other.m_extent;
            }
            return *this;
        }

        void shutdown()
        {
            if (m_imageView != VK_NULL_HANDLE)
            {
                vkDestroyImageView(m_device, m_imageView, nullptr);
                m_imageView = VK_NULL_HANDLE;
            }
            if (m_image != VK_NULL_HANDLE)
            {
                vkDestroyImage(m_device, m_image, nullptr);
                m_image = VK_NULL_HANDLE;
            }
            if (m_memory != VK_NULL_HANDLE)
            {
                vkFreeMemory(m_device, m_memory, nullptr);
                m_memory = VK_NULL_HANDLE;
            }
            m_device = VK_NULL_HANDLE;
            m_extent = {};
        }
    };

    struct VulkanSamplerResource
    {
        VkDevice m_device = VK_NULL_HANDLE;
        VkSampler m_sampler = VK_NULL_HANDLE;

        VulkanSamplerResource() = default;
        ~VulkanSamplerResource()
        {
            shutdown();
        }

        VulkanSamplerResource(const VulkanSamplerResource&) = delete;
        VulkanSamplerResource& operator=(const VulkanSamplerResource&) = delete;

        VulkanSamplerResource(VulkanSamplerResource&& other) noexcept
            : m_device(std::exchange(other.m_device, VK_NULL_HANDLE))
            , m_sampler(std::exchange(other.m_sampler, VK_NULL_HANDLE))
        {
        }

        VulkanSamplerResource& operator=(VulkanSamplerResource&& other) noexcept
        {
            if (this != &other)
            {
                shutdown();
                m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
                m_sampler = std::exchange(other.m_sampler, VK_NULL_HANDLE);
            }
            return *this;
        }

        void shutdown()
        {
            if (m_sampler != VK_NULL_HANDLE)
            {
                vkDestroySampler(m_device, m_sampler, nullptr);
                m_sampler = VK_NULL_HANDLE;
            }
            m_device = VK_NULL_HANDLE;
        }
    };

    struct VulkanRenderTargetResource
    {
        TextureHandle m_colorTexture;
        std::unique_ptr<RenderPass> m_renderPass;
        std::unique_ptr<Framebuffer> m_framebuffer;
        VkExtent2D m_extent{};
    };

    struct VulkanGraphicsPipelineResource
    {
        Pipeline m_pipeline;
        GraphicsPipelineDesc m_desc;
        ShaderProgramHandle m_program;
    };

    struct VulkanDescriptorSetResource
    {
        VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
        GraphicsPipelineHandle m_pipeline;
        uint32_t m_set = 0;
    };

    struct VulkanFrameResource
    {
        VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
        VkRenderPass m_renderPass = VK_NULL_HANDLE;
        VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
        VkExtent2D m_extent{};
        uint32_t m_imageIndex = 0;
    };

    class VulkanRenderer : public IRenderer
    {
    public:
        VulkanRenderer();
        ~VulkanRenderer() override;

        VulkanRenderer(const VulkanRenderer&) = delete;
        VulkanRenderer& operator=(const VulkanRenderer&) = delete;

        bool initialize(Window& window);
        void shutdown() override;

        GraphicsBackend getBackend() const override
        {
            return GraphicsBackend::Vulkan;
        }
        Extent2D getDrawableExtent() const override;
        RendererStats getStats() const override;
        bool resize(Extent2D newExtent) override;
        bool initializeUi() override;
        void shutdownUi() override;
        void handleUiEvent(const SDL_Event& event) override;
        void beginUi() override;
        void endUi() override;
        void renderUi(FrameHandle frame) override;
        bool isUiAvailable() const override
        {
            return m_uiInitialized;
        }

        ShaderModuleHandle createShaderModule(const ShaderModuleDesc& desc) override;
        bool destroyShaderModule(ShaderModuleHandle module) override;

        ShaderProgramHandle createShaderProgram(const ShaderProgramDesc& desc) override;
        bool destroyShaderProgram(ShaderProgramHandle program) override;

        BufferHandle createBuffer(const BufferDesc& desc) override;
        bool destroyBuffer(BufferHandle buffer) override;
        bool mapBuffer(BufferHandle bufferHandle, void** data);
        void unmapBuffer(BufferHandle bufferHandle);
        bool uploadBuffer(BufferHandle buffer, const void* data, std::size_t size, std::size_t offset = 0) override;

        TextureHandle createTexture(const TextureDesc& desc) override;
        bool destroyTexture(TextureHandle texture) override;
        SamplerHandle createSampler(const SamplerDesc& desc) override;
        bool destroySampler(SamplerHandle sampler) override;
        RenderTargetHandle createRenderTarget(const RenderTargetDesc& desc) override;
        bool destroyRenderTarget(RenderTargetHandle renderTarget) override;

        GraphicsPipelineHandle createGraphicsPipeline(const GraphicsPipelineDesc& desc,
                                                      ShaderProgramHandle program) override;
        bool destroyGraphicsPipeline(GraphicsPipelineHandle pipeline) override;
        DescriptorSetHandle createDescriptorSet(GraphicsPipelineHandle pipeline, uint32_t set) override;
        bool updateDescriptorSet(DescriptorSetHandle set, uint32_t binding, BufferHandle buffer, std::size_t offset = 0,
                                 std::size_t range = 0) override;
        bool updateDescriptorSet(DescriptorSetHandle set, uint32_t binding, TextureHandle texture) override;
        bool updateDescriptorSet(DescriptorSetHandle set, uint32_t binding, SamplerHandle sampler) override;
        bool destroyDescriptorSet(DescriptorSetHandle set) override;

        FrameHandle beginFrame() override;
        void beginRenderPass(FrameHandle frame, const RenderPassDesc& desc) override;
        void beginRenderPass(FrameHandle frame, RenderTargetHandle target, const RenderPassDesc& desc) override;
        void endRenderPass(FrameHandle frame) override;
        void bindPipeline(FrameHandle frame, GraphicsPipelineHandle pipeline) override;
        void bindVertexBuffer(FrameHandle frame, uint32_t slot, BufferHandle buffer, std::size_t offset = 0) override;
        void bindIndexBuffer(FrameHandle frame, BufferHandle buffer, IndexFormat format,
                             std::size_t offset = 0) override;
        void bindDescriptorSet(FrameHandle frame, GraphicsPipelineHandle pipeline, uint32_t set,
                               DescriptorSetHandle descriptorSet) override;
        void drawIndexed(FrameHandle frame, uint32_t indexCount, uint32_t instanceCount = 1) override;
        bool endFrame(FrameHandle frame) override;

    private:
        bool recreateSwapchainResources(uint32_t width, uint32_t height);
        bool recreateLiveGraphicsPipelines();
        RenderPass* resolveRenderPass(RenderTargetHandle renderTarget);
        void waitForDeviceIdle();
        void destroySyncObjects();
        bool createSyncObjects();
        bool createDescriptorPool();
        void destroyDescriptorPool();

        Window* m_window;
        std::shared_ptr<Instance> m_instance;
        std::shared_ptr<PhysicalDevice> m_physicalDevice;
        std::shared_ptr<Device> m_device;
        std::shared_ptr<Surface> m_surface;
        std::shared_ptr<SwapChain> m_swapchain;
        std::unique_ptr<RenderPass> m_renderPass;
        std::unique_ptr<Framebuffer> m_framebuffer;
        std::unique_ptr<CommandBuffer> m_commandBuffer;

        VkSemaphore m_imageAvailableSemaphore;
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        VkFence m_inFlightFence;
        VkDescriptorPool m_descriptorPool;

        ResourceRegistry<VulkanShaderModuleResource, ShaderModuleHandle> m_shaderModules;
        ResourceRegistry<VulkanShaderProgramResource, ShaderProgramHandle> m_shaderPrograms;
        ResourceRegistry<VulkanBufferResource, BufferHandle> m_buffers;
        ResourceRegistry<VulkanTextureResource, TextureHandle> m_textures;
        ResourceRegistry<VulkanSamplerResource, SamplerHandle> m_samplers;
        ResourceRegistry<VulkanRenderTargetResource, RenderTargetHandle> m_renderTargets;
        ResourceRegistry<VulkanGraphicsPipelineResource, GraphicsPipelineHandle> m_graphicsPipelines;
        ResourceRegistry<VulkanDescriptorSetResource, DescriptorSetHandle> m_descriptorSets;
        ResourceRegistry<VulkanFrameResource, FrameHandle> m_frames;
        RendererStats m_stats;
        bool m_uiInitialized = false;
    };

}  // namespace kera
