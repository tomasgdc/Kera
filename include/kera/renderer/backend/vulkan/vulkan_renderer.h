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
        EShaderStage m_stage = EShaderStage::VERTEX;
    };

    struct VulkanShaderProgramResource
    {
        std::vector<Shader> m_shaders;
        SlangReflectionMetadata m_reflection;
    };

    struct VulkanBufferResource
    {
        Buffer m_buffer;
        std::size_t m_ring_slot_size = 0;
        std::size_t m_ring_slot_stride = 0;
        uint32_t m_ring_slot_count = 1;
    };

    struct VulkanTextureResource
    {
        VkDevice m_device = VK_NULL_HANDLE;
        VkImage m_image = VK_NULL_HANDLE;
        VkDeviceMemory m_memory = VK_NULL_HANDLE;
        VkImageView m_image_view = VK_NULL_HANDLE;
        VkFormat m_format = VK_FORMAT_R8G8B8A8_UNORM;
        ETextureFormat m_texture_format = ETextureFormat::RGBA8;
        VkExtent2D m_extent{};
        uint32_t m_mip_levels = 1;
        uint32_t m_array_layers = 1;
        bool m_generate_mipmaps = false;
        VkImageAspectFlags m_aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageLayout m_current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout m_descriptor_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout m_render_target_final_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        bool m_sampled = false;
        bool m_render_target = false;
        bool m_depth_stencil = false;

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
            , m_image_view(std::exchange(other.m_image_view, VK_NULL_HANDLE))
            , m_format(other.m_format)
            , m_texture_format(other.m_texture_format)
            , m_extent(other.m_extent)
            , m_mip_levels(other.m_mip_levels)
            , m_array_layers(other.m_array_layers)
            , m_generate_mipmaps(other.m_generate_mipmaps)
            , m_aspect_mask(other.m_aspect_mask)
            , m_current_layout(other.m_current_layout)
            , m_descriptor_layout(other.m_descriptor_layout)
            , m_render_target_final_layout(other.m_render_target_final_layout)
            , m_sampled(other.m_sampled)
            , m_render_target(other.m_render_target)
            , m_depth_stencil(other.m_depth_stencil)
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
                m_image_view = std::exchange(other.m_image_view, VK_NULL_HANDLE);
                m_format = other.m_format;
                m_texture_format = other.m_texture_format;
                m_extent = other.m_extent;
                m_mip_levels = other.m_mip_levels;
                m_array_layers = other.m_array_layers;
                m_generate_mipmaps = other.m_generate_mipmaps;
                m_aspect_mask = other.m_aspect_mask;
                m_current_layout = other.m_current_layout;
                m_descriptor_layout = other.m_descriptor_layout;
                m_render_target_final_layout = other.m_render_target_final_layout;
                m_sampled = other.m_sampled;
                m_render_target = other.m_render_target;
                m_depth_stencil = other.m_depth_stencil;
            }
            return *this;
        }

        void shutdown()
        {
            if (m_image_view != VK_NULL_HANDLE)
            {
                vkDestroyImageView(m_device, m_image_view, nullptr);
                m_image_view = VK_NULL_HANDLE;
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
            m_texture_format = ETextureFormat::RGBA8;
            m_extent = {};
            m_mip_levels = 1;
            m_array_layers = 1;
            m_generate_mipmaps = false;
            m_aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
            m_current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
            m_descriptor_layout = VK_IMAGE_LAYOUT_UNDEFINED;
            m_render_target_final_layout = VK_IMAGE_LAYOUT_UNDEFINED;
            m_sampled = false;
            m_render_target = false;
            m_depth_stencil = false;
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
        TextureHandle m_color_texture;
        TextureHandle m_depth_texture;
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
        VkDescriptorSet m_descriptor_set = VK_NULL_HANDLE;
        VkDescriptorPool m_descriptor_pool = VK_NULL_HANDLE;
        GraphicsPipelineHandle m_pipeline;
        uint32_t m_set = 0;
        DescriptorSetLayoutDesc m_layout;
        std::string m_debug_name;
        std::vector<VulkanDescriptorBindingReference<BufferHandle>> m_buffers;
        std::vector<VulkanDescriptorBindingReference<TextureHandle>> m_textures;
        std::vector<VulkanDescriptorBindingReference<SamplerHandle>> m_samplers;
    };

    struct VulkanFrameResourceUse
    {
        std::vector<BufferHandle> m_buffers;
        std::vector<TextureHandle> m_textures;
        std::vector<SamplerHandle> m_samplers;
        std::vector<DescriptorSetHandle> m_descriptor_sets;
    };

    struct VulkanFrameResource
    {
        VkCommandBuffer m_command_buffer = VK_NULL_HANDLE;
        VkImageView m_color_image_view = VK_NULL_HANDLE;
        VkExtent2D m_extent{};
        uint32_t m_image_index = 0;
        uint32_t m_sync_index = 0;
        bool m_render_pass_active = false;
        TextureHandle m_active_render_target_texture;
        TextureHandle m_active_depth_texture;
        VkImageLayout m_render_pass_final_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    struct VulkanFrameSyncResource
    {
        VkSemaphore m_image_available_semaphore = VK_NULL_HANDLE;
        VkSemaphore m_render_finished_semaphore = VK_NULL_HANDLE;
        uint64_t m_timeline_value = 0;
        VulkanFrameResourceUse m_resource_use;
    };

    struct VulkanDeferredDeletion
    {
        uint64_t m_timeline_value = 0;
        std::vector<Buffer> m_buffers;
        std::vector<VulkanBufferResource> m_buffer_resources;
        std::vector<VulkanTextureResource> m_textures;
        std::vector<VulkanSamplerResource> m_samplers;
        std::vector<VulkanGraphicsPipelineResource> m_graphics_pipelines;
        std::vector<VulkanDescriptorSetResource> m_descriptor_sets;
        std::vector<VkCommandBuffer> m_command_buffers;
    };

    struct PendingTextureUpload
    {
        TextureHandle texture;
        Buffer staging_buffer;
        std::vector<VkBufferImageCopy> regions;
        bool generate_mipmaps = false;
    };

    struct VulkanUploadContext
    {
        std::vector<Buffer> available_staging_buffers;
        std::vector<PendingTextureUpload> pending_texture_uploads;
        bool batch_active = false;
    };

    struct VulkanDescriptorPoolResource
    {
        VkDescriptorPool m_pool = VK_NULL_HANDLE;
        uint32_t m_allocated_sets = 0;
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

        EGraphicsBackend getBackend() const override
        {
            return EGraphicsBackend::VULKAN;
        }
        Extent2D getDrawableExtent() const override;
        RendererStats getStats() const override;
        bool resize(Extent2D new_extent) override;
        bool initializeUi() override;
        void shutdownUi() override;
        void handleUiEvent(const SDL_Event& event) override;
        void beginUi() override;
        void endUi() override;
        void renderUi(FrameHandle frame) override;
        bool isUiAvailable() const override
        {
            return m_ui_initialized;
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
                                                                       const std::string& entry_point) const override;
        bool destroyShaderProgram(ShaderProgramHandle program) override;

        BufferHandle createBuffer(const BufferDesc& desc) override;
        bool destroyBuffer(BufferHandle buffer) override;
        bool mapBuffer(BufferHandle buffer_handle, void** data) override;
        void unmapBuffer(BufferHandle buffer_handle) override;
        bool uploadBuffer(BufferHandle buffer, const void* data, std::size_t size, std::size_t offset = 0) override;
        BufferHandle createUniformRingBuffer(std::size_t element_size, uint32_t slot_count = 0) override;
        bool uploadUniformRingBuffer(BufferHandle buffer, FrameHandle frame, const void* data,
                                     std::size_t size) override;
        virtual UniformRingBufferInfo getUniformRingBufferInfo(BufferHandle buffer) const override;
        virtual uint32_t getUniformRingBufferSlot(BufferHandle buffer, FrameHandle frame) const override;
        std::size_t getUniformRingBufferSlotOffset(BufferHandle buffer, uint32_t slot) const override;

        TextureHandle createTexture(const TextureDesc& desc) override;
        bool beginUploadBatch() override;
        bool endUploadBatch() override;
        void cancelUploadBatch() override;
        bool uploadTexture(TextureHandle texture, const void* data, std::size_t size) override;
        bool uploadTextureSubresource(TextureHandle texture, const TexturePrepareUpload& upload) override;
        bool destroyTexture(TextureHandle texture) override;
        SamplerHandle createSampler(const SamplerDesc& desc) override;
        bool destroySampler(SamplerHandle sampler) override;
        RenderTargetHandle createRenderTarget(const RenderTargetDesc& desc) override;
        bool destroyRenderTarget(RenderTargetHandle render_target) override;

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
        bool setDebugName(DescriptorSetHandle descriptor_set, const std::string& name) override;

        FrameHandle beginFrame() override;
        void beginRenderPass(FrameHandle frame, const RenderPassDesc& desc) override;
        void beginRenderPass(FrameHandle frame, RenderTargetHandle target, const RenderPassDesc& desc) override;
        void endRenderPass(FrameHandle frame) override;
        void bindPipeline(FrameHandle frame, GraphicsPipelineHandle pipeline) override;
        void bindVertexBuffer(FrameHandle frame, uint32_t slot, BufferHandle buffer, std::size_t offset = 0) override;
        void bindIndexBuffer(FrameHandle frame, BufferHandle buffer, EIndexFormat format,
                             std::size_t offset = 0) override;
        void bindDescriptorSet(FrameHandle frame, GraphicsPipelineHandle pipeline,
                               DescriptorSetHandle descriptor_set) override;
        void bindDescriptorSet(FrameHandle frame, GraphicsPipelineHandle pipeline, uint32_t set,
                               DescriptorSetHandle descriptor_set) override;
        void drawIndexed(FrameHandle frame, uint32_t index_count, uint32_t instance_count = 1) override;
        bool endFrame(FrameHandle frame) override;

    private:
        bool recreateSwapchainResources(uint32_t width, uint32_t height);
        bool recreateLiveGraphicsPipelines();
        bool recreateSwapchainFromWindow();
        bool refreshSwapchainSupport();
        bool hasActiveFrames() const;
        void releaseFrame(FrameHandle frame, uint32_t sync_index);
        bool waitForTimelineValue(uint64_t timeline_value);
        uint64_t reserveTimelineValue();
        uint64_t getLastSubmittedTimelineValue() const;
        bool submitImmediateCommandBuffer(VkCommandBuffer command_buffer, uint64_t& timeline_value);
        Buffer acquireStagingBuffer(VkDeviceSize size);
        void releaseStagingBuffer(Buffer&& buffer);
        bool allocateDescriptorSet(const VulkanGraphicsPipelineResource& pipeline, uint32_t set,
                                   VkDescriptorSet& descriptor_set, VkDescriptorPool& descriptor_pool);
        bool reallocateDescriptorSetsForPipeline(GraphicsPipelineHandle pipeline_handle,
                                                 VulkanGraphicsPipelineResource& pipeline);
        void queueDeferredDeletion(VulkanDeferredDeletion deletion);
        void collectDeferredDeletions();
        void flushDeferredDeletions();
        bool createDescriptorPoolBlock();
        void transitionTextureLayout(VkCommandBuffer command_buffer, VulkanTextureResource& texture,
                                     VkImageLayout new_layout);
        bool recordTextureUpload(VkCommandBuffer command_buffer, PendingTextureUpload& pending_upload,
                                 VulkanTextureResource& texture);
        bool generateTextureMipmaps(VkCommandBuffer command_buffer, VulkanTextureResource& texture);
        void clearCompletedFrameResourceUse(uint32_t sync_index);
        void recordDescriptorSetUse(uint32_t sync_index, DescriptorSetHandle descriptor_set_handle,
                                    const VulkanDescriptorSetResource& descriptor_set);
        bool frameResourceUses(BufferHandle buffer) const;
        bool frameResourceUses(TextureHandle texture) const;
        bool frameResourceUses(SamplerHandle sampler) const;
        bool frameResourceUses(DescriptorSetHandle descriptor_set) const;
        bool descriptorSetsReference(BufferHandle buffer);
        bool descriptorSetsReference(TextureHandle texture);
        bool descriptorSetsReference(SamplerHandle sampler);
        bool renderTargetsReference(TextureHandle texture);
        const DescriptorSetLayoutDesc* resolveDescriptorSetLayout(const VulkanGraphicsPipelineResource& pipeline,
                                                                  uint32_t set) const;
        bool validateDescriptorBinding(const VulkanDescriptorSetResource& descriptor_set, uint32_t binding,
                                        EDescriptorType type) const;
        RendererValidationReport validateDescriptorSetResource(const VulkanDescriptorSetResource& descriptor_set) const;
        bool resolvePipelineRenderingFormats(RenderTargetHandle render_target, VkFormat& color_format,
                                             VkFormat& depth_format) const;
        void transitionSwapchainImageLayout(VkCommandBuffer command_buffer, uint32_t image_index,
                                            VkImageLayout new_layout);
        void waitForDeviceIdle();
        void destroySyncObjects();
        bool createSyncObjects();
        bool createDescriptorPool();
        void destroyDescriptorPool();
        bool flushUploads();
        void discardPendingUploads();

        Window* m_window;
        SDL_Window* m_sdl_window = nullptr;
        Extent2D m_window_extent;
        std::shared_ptr<Instance> m_instance;
        std::shared_ptr<PhysicalDevice> m_physical_device;
        std::shared_ptr<Device> m_device;
        std::shared_ptr<Surface> m_surface;
        std::shared_ptr<SwapChain> m_swapchain;
        std::vector<std::unique_ptr<CommandBuffer>> m_command_buffers;

        std::vector<VulkanFrameSyncResource> m_frame_sync_resources;
        std::vector<uint64_t> m_images_in_flight;
        std::vector<VkImageLayout> m_swapchain_image_layouts;
        std::vector<VulkanDeferredDeletion> m_deferred_deletions;
        VulkanUploadContext m_upload_context;
        std::vector<FrameHandle> m_active_frame_handles;
        VkSemaphore m_frame_timeline_semaphore = VK_NULL_HANDLE;
        uint64_t m_next_frame_timeline_value = 1;
        uint32_t m_current_frame_sync_index = 0;
        VkPipelineCache m_pipeline_cache = VK_NULL_HANDLE;
        std::vector<VulkanDescriptorPoolResource> m_descriptor_pools;

        ResourceRegistry<VulkanShaderModuleResource, ShaderModuleHandle> m_shader_modules;
        ResourceRegistry<VulkanShaderProgramResource, ShaderProgramHandle> m_shader_programs;
        ResourceRegistry<VulkanBufferResource, BufferHandle> m_buffers;
        ResourceRegistry<VulkanTextureResource, TextureHandle> m_textures;
        ResourceRegistry<VulkanSamplerResource, SamplerHandle> m_samplers;
        ResourceRegistry<VulkanRenderTargetResource, RenderTargetHandle> m_render_targets;
        ResourceRegistry<VulkanGraphicsPipelineResource, GraphicsPipelineHandle> m_graphics_pipelines;
        ResourceRegistry<VulkanDescriptorSetResource, DescriptorSetHandle> m_descriptor_sets;
        ResourceRegistry<VulkanFrameResource, FrameHandle> m_frames;
        mutable RendererStats m_stats;
        Extent2D m_pending_resize_extent{};
        bool m_ui_initialized = false;
        bool m_swapchain_recreate_requested = false;
        bool m_resize_pending = false;
    };

}  // namespace kera
