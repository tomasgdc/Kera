// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/renderer/buffer.h"
#include "kera/renderer/interfaces.h"
#include "kera/renderer/pipeline.h"
#include "kera/renderer/resource_registry.h"
#include "kera/renderer/shader.h"
#include "kera/renderer/slang_reflection.h"

#include <vulkan/vulkan.h>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct SDL_Window;

namespace kera
{

    class Window;
    class Instance;
    class PhysicalDevice;
    class Device;
    class Surface;
    class SwapChain;
    class CommandBuffer;

    struct VulkanShaderModuleResource
    {
        Shader m_shader;
        ShaderStage m_stage = ShaderStage::Vertex;
    };

    struct VulkanShaderProgramResource
    {
        std::vector<Shader> m_shaders;
        SlangReflectionMetadata m_reflection;
    };

    struct VulkanBufferResource
    {
        Buffer m_buffer;
        std::size_t m_ringSlotSize = 0;
        uint32_t m_ringSlotCount = 1;
    };

    struct VulkanTextureResource
    {
        VkDevice m_device = VK_NULL_HANDLE;
        VkImage m_image = VK_NULL_HANDLE;
        VkDeviceMemory m_memory = VK_NULL_HANDLE;
        VkImageView m_imageView = VK_NULL_HANDLE;
        VkFormat m_format = VK_FORMAT_R8G8B8A8_UNORM;
        TextureFormat m_textureFormat = TextureFormat::RGBA8;
        VkExtent2D m_extent{};
        uint32_t m_mipLevels = 1;
        bool m_generateMipmaps = false;
        VkImageAspectFlags m_aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageLayout m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout m_descriptorLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout m_renderTargetFinalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        bool m_sampled = false;
        bool m_renderTarget = false;
        bool m_depthStencil = false;

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
            , m_textureFormat(other.m_textureFormat)
            , m_extent(other.m_extent)
            , m_mipLevels(other.m_mipLevels)
            , m_generateMipmaps(other.m_generateMipmaps)
            , m_aspectMask(other.m_aspectMask)
            , m_currentLayout(other.m_currentLayout)
            , m_descriptorLayout(other.m_descriptorLayout)
            , m_renderTargetFinalLayout(other.m_renderTargetFinalLayout)
            , m_sampled(other.m_sampled)
            , m_renderTarget(other.m_renderTarget)
            , m_depthStencil(other.m_depthStencil)
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
                m_textureFormat = other.m_textureFormat;
                m_extent = other.m_extent;
                m_mipLevels = other.m_mipLevels;
                m_generateMipmaps = other.m_generateMipmaps;
                m_aspectMask = other.m_aspectMask;
                m_currentLayout = other.m_currentLayout;
                m_descriptorLayout = other.m_descriptorLayout;
                m_renderTargetFinalLayout = other.m_renderTargetFinalLayout;
                m_sampled = other.m_sampled;
                m_renderTarget = other.m_renderTarget;
                m_depthStencil = other.m_depthStencil;
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
            m_textureFormat = TextureFormat::RGBA8;
            m_extent = {};
            m_mipLevels = 1;
            m_generateMipmaps = false;
            m_aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            m_descriptorLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            m_renderTargetFinalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            m_sampled = false;
            m_renderTarget = false;
            m_depthStencil = false;
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
        TextureHandle m_depthTexture;
        VkExtent2D m_extent{};
    };

    struct VulkanGraphicsPipelineResource
    {
        Pipeline m_pipeline;
        GraphicsPipelineDesc m_desc;
        ShaderProgramHandle m_program;
    };

    template <typename HandleT>
    struct VulkanDescriptorBindingReference
    {
        uint32_t m_binding = 0;
        HandleT m_handle;
        std::size_t m_offset = 0;
        std::size_t m_range = 0;
    };

    struct VulkanDescriptorSetResource
    {
        VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
        VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
        GraphicsPipelineHandle m_pipeline;
        uint32_t m_set = 0;
        DescriptorSetLayoutDesc m_layout;
        std::string m_debugName;
        std::vector<VulkanDescriptorBindingReference<BufferHandle>> m_buffers;
        std::vector<VulkanDescriptorBindingReference<TextureHandle>> m_textures;
        std::vector<VulkanDescriptorBindingReference<SamplerHandle>> m_samplers;
    };

    struct VulkanFrameResourceUse
    {
        std::vector<BufferHandle> m_buffers;
        std::vector<TextureHandle> m_textures;
        std::vector<SamplerHandle> m_samplers;
        std::vector<DescriptorSetHandle> m_descriptorSets;
    };

    struct VulkanFrameResource
    {
        VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
        VkImageView m_colorImageView = VK_NULL_HANDLE;
        VkExtent2D m_extent{};
        uint32_t m_imageIndex = 0;
        uint32_t m_syncIndex = 0;
        bool m_renderPassActive = false;
        TextureHandle m_activeRenderTargetTexture;
        TextureHandle m_activeDepthTexture;
        VkImageLayout m_renderPassFinalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    struct VulkanFrameSyncResource
    {
        VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;
        VkSemaphore m_renderFinishedSemaphore = VK_NULL_HANDLE;
        uint64_t m_timelineValue = 0;
        VulkanFrameResourceUse m_resourceUse;
    };

    struct VulkanDeferredDeletion
    {
        uint64_t m_timelineValue = 0;
        std::vector<Buffer> m_buffers;
        std::vector<VulkanBufferResource> m_bufferResources;
        std::vector<VulkanTextureResource> m_textures;
        std::vector<VulkanSamplerResource> m_samplers;
        std::vector<VulkanGraphicsPipelineResource> m_graphicsPipelines;
        std::vector<VulkanDescriptorSetResource> m_descriptorSets;
        std::vector<VkCommandBuffer> m_commandBuffers;
    };

    struct VulkanUploadContext
    {
        std::vector<Buffer> m_availableStagingBuffers;
    };

    struct VulkanDescriptorPoolResource
    {
        VkDescriptorPool m_pool = VK_NULL_HANDLE;
        uint32_t m_allocatedSets = 0;
    };

    class VulkanRenderer : public IRenderer
    {
    public:
        VulkanRenderer();
        ~VulkanRenderer() override;

        VulkanRenderer(const VulkanRenderer&) = delete;
        VulkanRenderer& operator=(const VulkanRenderer&) = delete;

        bool initialize(Window& window);
        bool initialize(SDL_Window* window, uint32_t width, uint32_t height);
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
        ShaderProgramHandle createGraphicsShaderProgram(const GraphicsShaderProgramDesc& desc) override;
        const SlangReflectionMetadata* getShaderProgramReflection(ShaderProgramHandle program) const override;
        std::vector<SlangReflectionEntryPoint> getShaderProgramEntryPoints(ShaderProgramHandle program) const override;
        std::vector<SlangReflectionBinding> getShaderProgramDescriptorBindings(
            ShaderProgramHandle program) const override;
        std::vector<SlangReflectionInput> getShaderProgramVertexInputs(ShaderProgramHandle program,
                                                                       const std::string& entryPoint) const override;
        bool destroyShaderProgram(ShaderProgramHandle program) override;

        BufferHandle createBuffer(const BufferDesc& desc) override;
        bool destroyBuffer(BufferHandle buffer) override;
        bool mapBuffer(BufferHandle bufferHandle, void** data);
        void unmapBuffer(BufferHandle bufferHandle);
        bool uploadBuffer(BufferHandle buffer, const void* data, std::size_t size, std::size_t offset = 0) override;
        BufferHandle createUniformRingBuffer(std::size_t elementSize, uint32_t slotCount = 0) override;
        bool uploadUniformRingBuffer(BufferHandle buffer, FrameHandle frame, const void* data,
                                     std::size_t size) override;
        std::size_t getUniformRingBufferOffset(BufferHandle buffer, FrameHandle frame) const override;

        TextureHandle createTexture(const TextureDesc& desc) override;
        bool uploadTexture(TextureHandle texture, const void* data, std::size_t size) override;
        bool uploadTextureSubresource(TextureHandle texture, const TexturePrepareUpload& upload) override;
        bool destroyTexture(TextureHandle texture) override;
        SamplerHandle createSampler(const SamplerDesc& desc) override;
        bool destroySampler(SamplerHandle sampler) override;
        RenderTargetHandle createRenderTarget(const RenderTargetDesc& desc) override;
        bool destroyRenderTarget(RenderTargetHandle renderTarget) override;

        GraphicsPipelineHandle createGraphicsPipeline(const GraphicsPipelineDesc& desc,
                                                      ShaderProgramHandle program) override;
        GraphicsPipelineHandle createGraphicsPipeline(const GraphicsPipelineCreateDesc& desc) override;
        std::vector<DescriptorSetLayoutDesc> getGraphicsPipelineDescriptorSets(
            GraphicsPipelineHandle pipeline) const override;
        VertexLayoutDesc getGraphicsPipelineVertexLayout(GraphicsPipelineHandle pipeline) const override;
        bool destroyGraphicsPipeline(GraphicsPipelineHandle pipeline) override;
        DescriptorSetHandle createDescriptorSet(GraphicsPipelineHandle pipeline) override;
        DescriptorSetHandle createDescriptorSet(GraphicsPipelineHandle pipeline, uint32_t set) override;
        bool updateDescriptorSet(DescriptorSetHandle set, uint32_t binding, BufferHandle buffer, std::size_t offset = 0,
                                 std::size_t range = 0) override;
        bool updateDescriptorSet(DescriptorSetHandle set, uint32_t binding, TextureHandle texture) override;
        bool updateDescriptorSet(DescriptorSetHandle set, uint32_t binding, SamplerHandle sampler) override;
        bool updateDescriptorSet(DescriptorSetHandle set, const std::string& name, BufferHandle buffer,
                                 std::size_t offset = 0, std::size_t range = 0) override;
        bool updateDescriptorSet(DescriptorSetHandle set, const std::string& name, TextureHandle texture) override;
        bool updateDescriptorSet(DescriptorSetHandle set, const std::string& name, SamplerHandle sampler) override;
        bool validateDescriptorSet(DescriptorSetHandle set) const override;
        bool validateGraphicsPipelineDescriptorSets(GraphicsPipelineHandle pipeline) const override;
        RendererValidationReport validateDescriptorSetDetailed(DescriptorSetHandle set) const override;
        RendererValidationReport validateGraphicsPipelineDescriptorSetsDetailed(
            GraphicsPipelineHandle pipeline) const override;
        bool destroyDescriptorSet(DescriptorSetHandle set) override;

        bool setDebugName(BufferHandle buffer, const std::string& name) override;
        bool setDebugName(TextureHandle texture, const std::string& name) override;
        bool setDebugName(SamplerHandle sampler, const std::string& name) override;
        bool setDebugName(GraphicsPipelineHandle pipeline, const std::string& name) override;
        bool setDebugName(DescriptorSetHandle descriptorSet, const std::string& name) override;

        FrameHandle beginFrame() override;
        void beginRenderPass(FrameHandle frame, const RenderPassDesc& desc) override;
        void beginRenderPass(FrameHandle frame, RenderTargetHandle target, const RenderPassDesc& desc) override;
        void endRenderPass(FrameHandle frame) override;
        void bindPipeline(FrameHandle frame, GraphicsPipelineHandle pipeline) override;
        void bindVertexBuffer(FrameHandle frame, uint32_t slot, BufferHandle buffer, std::size_t offset = 0) override;
        void bindIndexBuffer(FrameHandle frame, BufferHandle buffer, IndexFormat format,
                             std::size_t offset = 0) override;
        void bindDescriptorSet(FrameHandle frame, GraphicsPipelineHandle pipeline,
                               DescriptorSetHandle descriptorSet) override;
        void bindDescriptorSet(FrameHandle frame, GraphicsPipelineHandle pipeline, uint32_t set,
                               DescriptorSetHandle descriptorSet) override;
        void drawIndexed(FrameHandle frame, uint32_t indexCount, uint32_t instanceCount = 1) override;
        bool endFrame(FrameHandle frame) override;

    private:
        bool recreateSwapchainResources(uint32_t width, uint32_t height);
        bool recreateLiveGraphicsPipelines();
        bool recreateSwapchainFromWindow();
        bool refreshSwapchainSupport();
        bool hasActiveFrames() const;
        void releaseFrame(FrameHandle frame, uint32_t syncIndex);
        bool waitForTimelineValue(uint64_t timelineValue);
        uint64_t reserveTimelineValue();
        uint64_t getLastSubmittedTimelineValue() const;
        bool submitImmediateCommandBuffer(VkCommandBuffer commandBuffer, uint64_t& timelineValue);
        Buffer acquireStagingBuffer(VkDeviceSize size);
        void releaseStagingBuffer(Buffer&& buffer);
        bool allocateDescriptorSet(const VulkanGraphicsPipelineResource& pipeline, uint32_t set,
                                   VkDescriptorSet& descriptorSet, VkDescriptorPool& descriptorPool);
        bool reallocateDescriptorSetsForPipeline(GraphicsPipelineHandle pipelineHandle,
                                                 VulkanGraphicsPipelineResource& pipeline);
        void queueDeferredDeletion(VulkanDeferredDeletion deletion);
        void collectDeferredDeletions();
        void flushDeferredDeletions();
        bool createDescriptorPoolBlock();
        void transitionTextureLayout(VkCommandBuffer commandBuffer, VulkanTextureResource& texture,
                                     VkImageLayout newLayout);
        bool copyBufferToTexture(Buffer& stagingBuffer, VulkanTextureResource& texture);
        bool copyBufferToTextureSubresources(Buffer& stagingBuffer, VulkanTextureResource& texture,
                                             const std::vector<VkBufferImageCopy>& regions);
        bool generateTextureMipmaps(VkCommandBuffer commandBuffer, VulkanTextureResource& texture);
        void clearCompletedFrameResourceUse(uint32_t syncIndex);
        void recordDescriptorSetUse(uint32_t syncIndex, DescriptorSetHandle descriptorSetHandle,
                                    const VulkanDescriptorSetResource& descriptorSet);
        bool frameResourceUses(BufferHandle buffer) const;
        bool frameResourceUses(TextureHandle texture) const;
        bool frameResourceUses(SamplerHandle sampler) const;
        bool frameResourceUses(DescriptorSetHandle descriptorSet) const;
        bool descriptorSetsReference(BufferHandle buffer);
        bool descriptorSetsReference(TextureHandle texture);
        bool descriptorSetsReference(SamplerHandle sampler);
        bool renderTargetsReference(TextureHandle texture);
        const DescriptorSetLayoutDesc* resolveDescriptorSetLayout(const VulkanGraphicsPipelineResource& pipeline,
                                                                  uint32_t set) const;
        bool validateDescriptorBinding(const VulkanDescriptorSetResource& descriptorSet, uint32_t binding,
                                       DescriptorType type) const;
        RendererValidationReport validateDescriptorSetResource(const VulkanDescriptorSetResource& descriptorSet) const;
        bool resolvePipelineRenderingFormats(RenderTargetHandle renderTarget, VkFormat& colorFormat,
                                             VkFormat& depthFormat) const;
        void transitionSwapchainImageLayout(VkCommandBuffer commandBuffer, uint32_t imageIndex,
                                            VkImageLayout newLayout);
        void waitForDeviceIdle();
        void destroySyncObjects();
        bool createSyncObjects();
        bool createDescriptorPool();
        void destroyDescriptorPool();

        Window* m_window;
        SDL_Window* m_sdlWindow = nullptr;
        Extent2D m_windowExtent;
        std::shared_ptr<Instance> m_instance;
        std::shared_ptr<PhysicalDevice> m_physicalDevice;
        std::shared_ptr<Device> m_device;
        std::shared_ptr<Surface> m_surface;
        std::shared_ptr<SwapChain> m_swapchain;
        std::vector<std::unique_ptr<CommandBuffer>> m_commandBuffers;

        std::vector<VulkanFrameSyncResource> m_frameSyncResources;
        std::vector<uint64_t> m_imagesInFlight;
        std::vector<VkImageLayout> m_swapchainImageLayouts;
        std::vector<VulkanDeferredDeletion> m_deferredDeletions;
        VulkanUploadContext m_uploadContext;
        std::vector<FrameHandle> m_activeFrameHandles;
        VkSemaphore m_frameTimelineSemaphore = VK_NULL_HANDLE;
        uint64_t m_nextFrameTimelineValue = 1;
        uint32_t m_currentFrameSyncIndex = 0;
        VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;
        std::vector<VulkanDescriptorPoolResource> m_descriptorPools;

        ResourceRegistry<VulkanShaderModuleResource, ShaderModuleHandle> m_shaderModules;
        ResourceRegistry<VulkanShaderProgramResource, ShaderProgramHandle> m_shaderPrograms;
        ResourceRegistry<VulkanBufferResource, BufferHandle> m_buffers;
        ResourceRegistry<VulkanTextureResource, TextureHandle> m_textures;
        ResourceRegistry<VulkanSamplerResource, SamplerHandle> m_samplers;
        ResourceRegistry<VulkanRenderTargetResource, RenderTargetHandle> m_renderTargets;
        ResourceRegistry<VulkanGraphicsPipelineResource, GraphicsPipelineHandle> m_graphicsPipelines;
        ResourceRegistry<VulkanDescriptorSetResource, DescriptorSetHandle> m_descriptorSets;
        ResourceRegistry<VulkanFrameResource, FrameHandle> m_frames;
        mutable RendererStats m_stats;
        Extent2D m_pendingResizeExtent{};
        bool m_uiInitialized = false;
        bool m_swapchainRecreateRequested = false;
        bool m_resizePending = false;
    };

}  // namespace kera
