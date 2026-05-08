#include "kera/renderer/backend/vulkan/vulkan_renderer.h"

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

#include <array>
#include <span>
#include <stdexcept>
#include <utility>

#ifdef KERA_HAS_IMGUI
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#endif

namespace kera
{

    namespace
    {

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
                    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                default:
                    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            }
        }

        VkFormat toVkTextureFormat(TextureFormat format)
        {
            switch (format)
            {
                case TextureFormat::RGBA8:
                default:
                    return VK_FORMAT_R8G8B8A8_UNORM;
            }
        }

        VkFilter toVkFilter(SamplerFilter filter)
        {
            switch (filter)
            {
                case SamplerFilter::Nearest:
                    return VK_FILTER_NEAREST;
                case SamplerFilter::Linear:
                default:
                    return VK_FILTER_LINEAR;
            }
        }

        VkSamplerAddressMode toVkAddressMode(SamplerAddressMode mode)
        {
            switch (mode)
            {
                case SamplerAddressMode::Repeat:
                    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
                case SamplerAddressMode::ClampToEdge:
                default:
                    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            }
        }

        uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
        {
            VkPhysicalDeviceMemoryProperties memoryProperties{};
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

            for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
            {
                if ((typeFilter & (1u << i)) &&
                    (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
                {
                    return i;
                }
            }

            throw std::runtime_error("Failed to find suitable Vulkan memory type.");
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

        const Shader* findShader(const VulkanShaderProgramResource& program, ShaderStage stage)
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

        std::array<const Shader*, 2> getGraphicsShaders(const VulkanShaderProgramResource& program)
        {
            return {
                findShader(program, ShaderStage::Vertex),
                findShader(program, ShaderStage::Fragment),
            };
        }

        bool initializeVulkanShaderFromDesc(const Device& device, const ShaderModuleDesc& desc, Shader& shader)
        {
            const ShaderType shaderType = toShaderType(desc.stage);
            bool initialized = false;

            switch (desc.source)
            {
                case ShaderSourceKind::SlangFile:
                    initialized = shader.initializeFromSlangFile(device, shaderType, desc.path, desc.entryPoint,
                                                                 desc.searchPaths);
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
                Logger::getInstance().error("Failed to create Vulkan shader module from requested source.");
                return false;
            }

            return true;
        }

    }  // namespace

    VulkanRenderer::VulkanRenderer()
        : m_window(nullptr)
        , m_imageAvailableSemaphore(VK_NULL_HANDLE)
        , m_inFlightFence(VK_NULL_HANDLE)
        , m_descriptorPool(VK_NULL_HANDLE)
        , m_uiInitialized(false)
    {
    }

    VulkanRenderer::~VulkanRenderer()
    {
        shutdown();
    }

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
        if (!m_physicalDevice->initialize(m_instance->getVulkanInstance(), m_surface->getVulkanSurface()))
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

        if (!createDescriptorPool())
        {
            Logger::getInstance().error("Failed to create Vulkan descriptor pool");
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
            Logger::getInstance().error("Failed to create Vulkan frame synchronization objects");
            return false;
        }

        return true;
    }

    void VulkanRenderer::shutdown()
    {
        waitForDeviceIdle();
        shutdownUi();

        m_frames.clear();
        m_descriptorSets.clear();
        m_graphicsPipelines.clear();
        m_renderTargets.clear();
        m_samplers.clear();
        m_textures.clear();
        m_buffers.clear();
        m_shaderPrograms.clear();
        m_shaderModules.clear();

        destroySyncObjects();
        destroyDescriptorPool();

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
        m_stats = {};
    }

    Extent2D VulkanRenderer::getDrawableExtent() const
    {
        if (!m_swapchain)
        {
            return {};
        }

        const VkExtent2D extent = m_swapchain->getExtent();
        return {extent.width, extent.height};
    }

    RendererStats VulkanRenderer::getStats() const
    {
        RendererStats stats = m_stats;
        stats.resources.shaderModules = m_shaderModules.activeCount();
        stats.resources.shaderPrograms = m_shaderPrograms.activeCount();
        stats.resources.buffers = m_buffers.activeCount();
        stats.resources.textures = m_textures.activeCount();
        stats.resources.samplers = m_samplers.activeCount();
        stats.resources.renderTargets = m_renderTargets.activeCount();
        stats.resources.graphicsPipelines = m_graphicsPipelines.activeCount();
        stats.resources.descriptorSets = m_descriptorSets.activeCount();
        stats.resources.frames = m_frames.activeCount();
        return stats;
    }

    bool VulkanRenderer::initializeUi()
    {
#ifdef KERA_HAS_IMGUI
        if (m_uiInitialized)
        {
            return true;
        }

        if (!m_window || !m_instance || !m_device || !m_renderPass || !m_swapchain)
        {
            Logger::getInstance().warning("Cannot initialize ImGui before Vulkan renderer resources are ready.");
            return false;
        }

        const uint32_t imageCount = m_swapchain->getImageCount();
        const uint32_t minImageCount = imageCount < 2 ? imageCount : 2;
        if (minImageCount < 2 || imageCount < minImageCount)
        {
            Logger::getInstance().warning("Cannot initialize ImGui with fewer than two swapchain images.");
            return false;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        if (!ImGui_ImplSDL3_InitForVulkan(m_window->getSDLWindow()))
        {
            Logger::getInstance().warning("Failed to initialize ImGui SDL3 backend.");
            ImGui::DestroyContext();
            return false;
        }

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.ApiVersion = VK_API_VERSION_1_3;
        initInfo.Instance = m_instance->getVulkanInstance();
        initInfo.PhysicalDevice = m_device->getVulkanPhysicalDevice();
        initInfo.Device = m_device->getVulkanDevice();
        initInfo.QueueFamily = m_device->getGraphicsQueueFamilyIndex();
        initInfo.Queue = m_device->getGraphicsQueue();
        initInfo.DescriptorPoolSize = 256;
        initInfo.MinImageCount = minImageCount;
        initInfo.ImageCount = imageCount;
        initInfo.PipelineInfoMain.RenderPass = m_renderPass->getVulkanRenderPass();
        initInfo.PipelineInfoMain.Subpass = 0;
        initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        if (!ImGui_ImplVulkan_Init(&initInfo))
        {
            Logger::getInstance().warning("Failed to initialize ImGui Vulkan backend.");
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            return false;
        }

        m_uiInitialized = true;
        return true;
#else
        Logger::getInstance().warning("Kera was built without ImGui support.");
        return false;
#endif
    }

    void VulkanRenderer::shutdownUi()
    {
#ifdef KERA_HAS_IMGUI
        if (!m_uiInitialized)
        {
            return;
        }

        waitForDeviceIdle();
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
#endif
        m_uiInitialized = false;
    }

    void VulkanRenderer::handleUiEvent(const SDL_Event& event)
    {
#ifdef KERA_HAS_IMGUI
        if (m_uiInitialized)
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
        }
#else
        (void)event;
#endif
    }

    void VulkanRenderer::beginUi()
    {
#ifdef KERA_HAS_IMGUI
        if (!m_uiInitialized)
        {
            return;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
#endif
    }

    void VulkanRenderer::endUi()
    {
#ifdef KERA_HAS_IMGUI
        if (m_uiInitialized)
        {
            ImGui::Render();
        }
#endif
    }

    void VulkanRenderer::renderUi(FrameHandle frameHandle)
    {
#ifdef KERA_HAS_IMGUI
        if (!m_uiInitialized)
        {
            return;
        }

        VulkanFrameResource* frame = m_frames.get(frameHandle);
        if (!frame)
        {
            Logger::getInstance().error("Invalid frame handle passed to renderUi.");
            return;
        }

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frame->m_commandBuffer);
#else
        (void)frameHandle;
#endif
    }

    bool VulkanRenderer::resize(Extent2D newExtent)
    {
        if (!m_device || !m_surface || !m_physicalDevice || !m_swapchain)
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return false;
        }

        if (newExtent.width == 0 || newExtent.height == 0)
        {
            return true;
        }

        const bool restoreUi = m_uiInitialized;
        if (restoreUi)
        {
            shutdownUi();
        }

        waitForDeviceIdle();

        if (!recreateSwapchainResources(newExtent.width, newExtent.height))
        {
            Logger::getInstance().error("Failed to recreate Vulkan swapchain resources.");
            return false;
        }

        if (!createSyncObjects())
        {
            Logger::getInstance().error("Failed to recreate Vulkan frame synchronization objects.");
            return false;
        }

        if (restoreUi && !initializeUi())
        {
            Logger::getInstance().warning("Failed to restore ImGui after resize.");
        }

        return true;
    }

    ShaderModuleHandle VulkanRenderer::createShaderModule(const ShaderModuleDesc& desc)
    {
        if (!m_device)
        {
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

    bool VulkanRenderer::destroyShaderModule(ShaderModuleHandle module)
    {
        waitForDeviceIdle();
        return m_shaderModules.remove(module);
    }

    ShaderProgramHandle VulkanRenderer::createShaderProgram(const ShaderProgramDesc& desc)
    {
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
        for (const ShaderModuleDesc& stageDesc : desc.stages)
        {
            if (stageDesc.stage == ShaderStage::Vertex)
            {
                if (hasVertexStage)
                {
                    Logger::getInstance().error("Shader program contains duplicate vertex stages.");
                    return {};
                }
                hasVertexStage = true;
            }
            else if (stageDesc.stage == ShaderStage::Fragment)
            {
                if (hasFragmentStage)
                {
                    Logger::getInstance().error("Shader program contains duplicate fragment stages.");
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

    bool VulkanRenderer::destroyShaderProgram(ShaderProgramHandle program)
    {
        waitForDeviceIdle();
        return m_shaderPrograms.remove(program);
    }

    BufferHandle VulkanRenderer::createBuffer(const BufferDesc& desc)
    {
        if (!m_device)
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return {};
        }

        VulkanBufferResource resource{};
        if (!resource.m_buffer.initialize(*m_device, static_cast<VkDeviceSize>(desc.size), toBufferUsage(desc.usage),
                                          toMemoryFlags(desc.memoryAccess)))
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

    bool VulkanRenderer::mapBuffer(BufferHandle bufferHandle, void** data)
    {
        VulkanBufferResource* resource = m_buffers.get(bufferHandle);
        if (!resource)
        {
            Logger::getInstance().error("Failed to map buffer: Invalid BufferHandle provided.");
            return false;
        }

        // Get the actual Buffer object managed by the resource.
        Buffer& buffer = resource->m_buffer;
        if (!buffer.map(data))
        {
            Logger::getInstance().error("Failed to map buffer: vkMapMemory failed.");
            return false;
        }
        return true;
    }

    void VulkanRenderer::unmapBuffer(BufferHandle bufferHandle)
    {
        VulkanBufferResource* resource = m_buffers.get(bufferHandle);
        if (!resource)
        {
            Logger::getInstance().error("Failed to unmap buffer: Invalid BufferHandle provided.");
            return;
        }

        Buffer& buffer = resource->m_buffer;
        buffer.unmap();
    }

    bool VulkanRenderer::uploadBuffer(BufferHandle buffer, const void* data, std::size_t size, std::size_t offset)
    {
        VulkanBufferResource* resource = m_buffers.get(buffer);
        if (!resource)
        {
            Logger::getInstance().error("Invalid buffer handle passed to uploadBuffer.");
            return false;
        }

        if (!resource->m_buffer.copyFrom(data, static_cast<VkDeviceSize>(size), static_cast<VkDeviceSize>(offset)))
        {
            Logger::getInstance().error("Failed to upload data to Vulkan buffer.");
            return false;
        }

        return true;
    }

    TextureHandle VulkanRenderer::createTexture(const TextureDesc& desc)
    {
        if (!m_device)
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return {};
        }

        if (desc.width == 0 || desc.height == 0)
        {
            Logger::getInstance().error("Texture dimensions must be non-zero.");
            return {};
        }

        VkImageUsageFlags usage = 0;
        if (desc.renderTarget)
        {
            usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        if (desc.sampled)
        {
            usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        }
        if (usage == 0)
        {
            Logger::getInstance().error("TextureDesc must request at least one usage.");
            return {};
        }

        VulkanTextureResource resource{};
        resource.m_device = m_device->getVulkanDevice();
        resource.m_format = toVkTextureFormat(desc.format);
        resource.m_extent = {desc.width, desc.height};

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {desc.width, desc.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = resource.m_format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        VkDevice vkDevice = m_device->getVulkanDevice();
        if (vkCreateImage(vkDevice, &imageInfo, nullptr, &resource.m_image) != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to create Vulkan texture image.");
            return {};
        }

        VkMemoryRequirements memoryRequirements{};
        vkGetImageMemoryRequirements(vkDevice, resource.m_image, &memoryRequirements);

        VkMemoryAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = memoryRequirements.size;
        try
        {
            allocateInfo.memoryTypeIndex =
                findMemoryType(m_device->getVulkanPhysicalDevice(), memoryRequirements.memoryTypeBits,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        }
        catch (const std::runtime_error& error)
        {
            Logger::getInstance().error(error.what());
            resource.shutdown();
            return {};
        }

        if (vkAllocateMemory(vkDevice, &allocateInfo, nullptr, &resource.m_memory) != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to allocate Vulkan texture memory.");
            resource.shutdown();
            return {};
        }

        vkBindImageMemory(vkDevice, resource.m_image, resource.m_memory, 0);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = resource.m_image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = resource.m_format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(vkDevice, &viewInfo, nullptr, &resource.m_imageView) != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to create Vulkan texture view.");
            resource.shutdown();
            return {};
        }

        return m_textures.emplace(std::move(resource));
    }

    bool VulkanRenderer::destroyTexture(TextureHandle texture)
    {
        waitForDeviceIdle();
        return m_textures.remove(texture);
    }

    SamplerHandle VulkanRenderer::createSampler(const SamplerDesc& desc)
    {
        if (!m_device)
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return {};
        }

        VulkanSamplerResource resource{};
        resource.m_device = m_device->getVulkanDevice();

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = toVkFilter(desc.magFilter);
        samplerInfo.minFilter = toVkFilter(desc.minFilter);
        samplerInfo.addressModeU = toVkAddressMode(desc.addressModeU);
        samplerInfo.addressModeV = toVkAddressMode(desc.addressModeV);
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        if (vkCreateSampler(resource.m_device, &samplerInfo, nullptr, &resource.m_sampler) != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to create Vulkan sampler.");
            return {};
        }

        return m_samplers.emplace(std::move(resource));
    }

    bool VulkanRenderer::destroySampler(SamplerHandle sampler)
    {
        waitForDeviceIdle();
        return m_samplers.remove(sampler);
    }

    RenderTargetHandle VulkanRenderer::createRenderTarget(const RenderTargetDesc& desc)
    {
        if (!m_device)
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return {};
        }

        VulkanTextureResource* texture = m_textures.get(desc.colorTexture);
        if (!texture)
        {
            Logger::getInstance().error("Invalid color texture passed to createRenderTarget.");
            return {};
        }

        VulkanRenderTargetResource resource{};
        resource.m_colorTexture = desc.colorTexture;
        resource.m_extent = texture->m_extent;
        resource.m_renderPass = std::make_unique<RenderPass>();
        if (!resource.m_renderPass->initializeColorTarget(*m_device, texture->m_format))
        {
            Logger::getInstance().error("Failed to create render target render pass.");
            return {};
        }

        resource.m_framebuffer = std::make_unique<Framebuffer>();
        if (!resource.m_framebuffer->initializeSingleColorTarget(*m_device, *resource.m_renderPass,
                                                                 texture->m_imageView, texture->m_extent))
        {
            Logger::getInstance().error("Failed to create render target framebuffer.");
            return {};
        }

        return m_renderTargets.emplace(std::move(resource));
    }

    bool VulkanRenderer::destroyRenderTarget(RenderTargetHandle renderTarget)
    {
        waitForDeviceIdle();
        return m_renderTargets.remove(renderTarget);
    }

    GraphicsPipelineHandle VulkanRenderer::createGraphicsPipeline(const GraphicsPipelineDesc& desc,
                                                                  ShaderProgramHandle program)
    {
        if (!m_device || !m_renderPass)
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return {};
        }

        RenderPass* renderPass = resolveRenderPass(desc.renderTarget);
        if (!renderPass)
        {
            Logger::getInstance().error("Invalid render target passed to createGraphicsPipeline.");
            return {};
        }

        VulkanShaderProgramResource* shaderProgram = m_shaderPrograms.get(program);
        if (!shaderProgram)
        {
            Logger::getInstance().error("Invalid shader program handle passed to createGraphicsPipeline.");
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
                *m_device, *renderPass, std::span<const Shader* const>(graphicsShaders.data(), graphicsShaders.size()),
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

    DescriptorSetHandle VulkanRenderer::createDescriptorSet(GraphicsPipelineHandle pipelineHandle, uint32_t set)
    {
        if (!m_device || m_descriptorPool == VK_NULL_HANDLE)
        {
            Logger::getInstance().error("Renderer descriptor resources are not initialized.");
            return {};
        }

        VulkanGraphicsPipelineResource* pipeline = m_graphicsPipelines.get(pipelineHandle);
        if (!pipeline)
        {
            Logger::getInstance().error("Invalid graphics pipeline handle passed to createDescriptorSet.");
            return {};
        }

        VkDescriptorSetLayout layout = pipeline->m_pipeline.getDescriptorSetLayout(set);
        if (layout == VK_NULL_HANDLE)
        {
            Logger::getInstance().error("Graphics pipeline does not declare the requested descriptor set.");
            return {};
        }

        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = m_descriptorPool;
        allocateInfo.descriptorSetCount = 1;
        allocateInfo.pSetLayouts = &layout;

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        if (vkAllocateDescriptorSets(m_device->getVulkanDevice(), &allocateInfo, &descriptorSet) != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to allocate Vulkan descriptor set.");
            return {};
        }

        VulkanDescriptorSetResource resource{};
        resource.m_descriptorSet = descriptorSet;
        resource.m_pipeline = pipelineHandle;
        resource.m_set = set;
        return m_descriptorSets.emplace(resource);
    }

    bool VulkanRenderer::updateDescriptorSet(DescriptorSetHandle setHandle, uint32_t binding, BufferHandle bufferHandle,
                                             std::size_t offset, std::size_t range)
    {
        if (!m_device)
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return false;
        }

        VulkanDescriptorSetResource* descriptorSet = m_descriptorSets.get(setHandle);
        VulkanBufferResource* buffer = m_buffers.get(bufferHandle);
        if (!descriptorSet || !buffer)
        {
            Logger::getInstance().error("Invalid handle passed to updateDescriptorSet.");
            return false;
        }

        const VkDeviceSize bufferSize = buffer->m_buffer.getSize();
        if (offset > bufferSize)
        {
            Logger::getInstance().error("Descriptor buffer offset exceeds buffer size.");
            return false;
        }

        const VkDeviceSize descriptorRange =
            range == 0 ? bufferSize - static_cast<VkDeviceSize>(offset) : static_cast<VkDeviceSize>(range);
        if (static_cast<VkDeviceSize>(offset) + descriptorRange > bufferSize)
        {
            Logger::getInstance().error("Descriptor buffer range exceeds buffer size.");
            return false;
        }

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = buffer->m_buffer.getVulkanBuffer();
        bufferInfo.offset = static_cast<VkDeviceSize>(offset);
        bufferInfo.range = descriptorRange;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet->m_descriptorSet;
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(m_device->getVulkanDevice(), 1, &write, 0, nullptr);
        return true;
    }

    bool VulkanRenderer::updateDescriptorSet(DescriptorSetHandle setHandle, uint32_t binding,
                                             TextureHandle textureHandle)
    {
        if (!m_device)
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return false;
        }

        VulkanDescriptorSetResource* descriptorSet = m_descriptorSets.get(setHandle);
        VulkanTextureResource* texture = m_textures.get(textureHandle);
        if (!descriptorSet || !texture)
        {
            Logger::getInstance().error("Invalid handle passed to texture updateDescriptorSet.");
            return false;
        }

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = texture->m_imageView;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet->m_descriptorSet;
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_device->getVulkanDevice(), 1, &write, 0, nullptr);
        return true;
    }

    bool VulkanRenderer::updateDescriptorSet(DescriptorSetHandle setHandle, uint32_t binding,
                                             SamplerHandle samplerHandle)
    {
        if (!m_device)
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return false;
        }

        VulkanDescriptorSetResource* descriptorSet = m_descriptorSets.get(setHandle);
        VulkanSamplerResource* sampler = m_samplers.get(samplerHandle);
        if (!descriptorSet || !sampler)
        {
            Logger::getInstance().error("Invalid handle passed to sampler updateDescriptorSet.");
            return false;
        }

        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = sampler->m_sampler;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet->m_descriptorSet;
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_device->getVulkanDevice(), 1, &write, 0, nullptr);
        return true;
    }

    bool VulkanRenderer::destroyDescriptorSet(DescriptorSetHandle setHandle)
    {
        VulkanDescriptorSetResource* descriptorSet = m_descriptorSets.get(setHandle);
        if (!descriptorSet)
        {
            return false;
        }

        waitForDeviceIdle();

        if (m_device && m_descriptorPool != VK_NULL_HANDLE && descriptorSet->m_descriptorSet != VK_NULL_HANDLE)
        {
            vkFreeDescriptorSets(m_device->getVulkanDevice(), m_descriptorPool, 1, &descriptorSet->m_descriptorSet);
        }

        return m_descriptorSets.remove(setHandle);
    }

    FrameHandle VulkanRenderer::beginFrame()
    {
        if (!m_device || !m_swapchain || !m_commandBuffer || !m_framebuffer || !m_renderPass)
        {
            Logger::getInstance().error("Renderer frame resources are not initialized.");
            return {};
        }

        VkDevice vkDevice = m_device->getVulkanDevice();
        vkWaitForFences(vkDevice, 1, &m_inFlightFence, VK_TRUE, UINT64_MAX);
        vkResetFences(vkDevice, 1, &m_inFlightFence);

        uint32_t imageIndex = 0;
        const VkResult acquireResult =
            m_swapchain->acquireNextImage(m_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

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
        m_stats.drawCallsThisFrame = 0;
        ++m_stats.frameIndex;
        return m_frames.emplace(frame);
    }

    void VulkanRenderer::beginRenderPass(FrameHandle frameHandle, const RenderPassDesc& desc)
    {
        VulkanFrameResource* frame = m_frames.get(frameHandle);
        if (!frame)
        {
            Logger::getInstance().error("Invalid frame handle passed to beginRenderPass.");
            return;
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = frame->m_renderPass;
        renderPassInfo.framebuffer = frame->m_framebuffer;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = frame->m_extent;

        VkClearValue clearColor{};
        clearColor.color = {{desc.clearColor.r, desc.clearColor.g, desc.clearColor.b, desc.clearColor.a}};

        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(frame->m_commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(frame->m_extent.width);
        viewport.height = static_cast<float>(frame->m_extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(frame->m_commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = frame->m_extent;
        vkCmdSetScissor(frame->m_commandBuffer, 0, 1, &scissor);
    }

    void VulkanRenderer::beginRenderPass(FrameHandle frameHandle, RenderTargetHandle renderTargetHandle,
                                         const RenderPassDesc& desc)
    {
        VulkanFrameResource* frame = m_frames.get(frameHandle);
        VulkanRenderTargetResource* renderTarget = m_renderTargets.get(renderTargetHandle);
        if (!frame || !renderTarget || !renderTarget->m_renderPass || !renderTarget->m_framebuffer)
        {
            Logger::getInstance().error("Invalid handle passed to render target beginRenderPass.");
            return;
        }

        VkFramebuffer framebuffer = renderTarget->m_framebuffer->getFramebuffer(0);
        if (framebuffer == VK_NULL_HANDLE)
        {
            Logger::getInstance().error("Render target framebuffer is invalid.");
            return;
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderTarget->m_renderPass->getVulkanRenderPass();
        renderPassInfo.framebuffer = framebuffer;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = renderTarget->m_extent;

        VkClearValue clearColor{};
        clearColor.color = {{desc.clearColor.r, desc.clearColor.g, desc.clearColor.b, desc.clearColor.a}};

        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(frame->m_commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(renderTarget->m_extent.width);
        viewport.height = static_cast<float>(renderTarget->m_extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(frame->m_commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = renderTarget->m_extent;
        vkCmdSetScissor(frame->m_commandBuffer, 0, 1, &scissor);
    }

    void VulkanRenderer::endRenderPass(FrameHandle frameHandle)
    {
        VulkanFrameResource* frame = m_frames.get(frameHandle);
        if (!frame)
        {
            Logger::getInstance().error("Invalid frame handle passed to endRenderPass.");
            return;
        }

        vkCmdEndRenderPass(frame->m_commandBuffer);
    }

    void VulkanRenderer::bindPipeline(FrameHandle frameHandle, GraphicsPipelineHandle pipelineHandle)
    {
        VulkanFrameResource* frame = m_frames.get(frameHandle);
        VulkanGraphicsPipelineResource* pipeline = m_graphicsPipelines.get(pipelineHandle);

        if (!frame || !pipeline)
        {
            Logger::getInstance().error("Invalid handle passed to bindPipeline.");
            return;
        }

        vkCmdBindPipeline(frame->m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline->m_pipeline.getVulkanPipeline());
    }

    void VulkanRenderer::bindVertexBuffer(FrameHandle frameHandle, uint32_t slot, BufferHandle bufferHandle,
                                          std::size_t offset)
    {
        VulkanFrameResource* frame = m_frames.get(frameHandle);
        VulkanBufferResource* buffer = m_buffers.get(bufferHandle);
        if (!frame || !buffer)
        {
            Logger::getInstance().error("Invalid handle passed to bindVertexBuffer.");
            return;
        }

        VkBuffer vertexBuffers[] = {buffer->m_buffer.getVulkanBuffer()};
        VkDeviceSize offsets[] = {static_cast<VkDeviceSize>(offset)};
        vkCmdBindVertexBuffers(frame->m_commandBuffer, slot, 1, vertexBuffers, offsets);
    }

    void VulkanRenderer::bindIndexBuffer(FrameHandle frameHandle, BufferHandle bufferHandle, IndexFormat format,
                                         std::size_t offset)
    {
        VulkanFrameResource* frame = m_frames.get(frameHandle);
        VulkanBufferResource* buffer = m_buffers.get(bufferHandle);
        if (!frame || !buffer)
        {
            Logger::getInstance().error("Invalid handle passed to bindIndexBuffer.");
            return;
        }

        const VkIndexType indexType = format == IndexFormat::UInt32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
        vkCmdBindIndexBuffer(frame->m_commandBuffer, buffer->m_buffer.getVulkanBuffer(),
                             static_cast<VkDeviceSize>(offset), indexType);
    }

    void VulkanRenderer::bindDescriptorSet(FrameHandle frameHandle, GraphicsPipelineHandle pipelineHandle, uint32_t set,
                                           DescriptorSetHandle descriptorSetHandle)
    {
        VulkanFrameResource* frame = m_frames.get(frameHandle);
        VulkanGraphicsPipelineResource* pipeline = m_graphicsPipelines.get(pipelineHandle);
        VulkanDescriptorSetResource* descriptorSet = m_descriptorSets.get(descriptorSetHandle);
        if (!frame || !pipeline || !descriptorSet)
        {
            Logger::getInstance().error("Invalid handle passed to bindDescriptorSet.");
            return;
        }

        if (descriptorSet->m_set != set || descriptorSet->m_pipeline != pipelineHandle)
        {
            Logger::getInstance().error("Descriptor set is not compatible with the requested pipeline set.");
            return;
        }

        vkCmdBindDescriptorSets(frame->m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline->m_pipeline.getPipelineLayout(), set, 1, &descriptorSet->m_descriptorSet, 0,
                                nullptr);
    }

    void VulkanRenderer::drawIndexed(FrameHandle frameHandle, uint32_t indexCount, uint32_t instanceCount)
    {
        VulkanFrameResource* frame = m_frames.get(frameHandle);
        if (!frame)
        {
            Logger::getInstance().error("Invalid frame handle passed to drawIndexed.");
            return;
        }

        vkCmdDrawIndexed(frame->m_commandBuffer, indexCount, instanceCount, 0, 0, 0);
        ++m_stats.drawCallsThisFrame;
    }

    bool VulkanRenderer::endFrame(FrameHandle frameHandle)
    {
        if (!m_device || !m_commandBuffer || !m_swapchain)
        {
            Logger::getInstance().error("Renderer frame resources are not initialized.");
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
            Logger::getInstance().error("Swapchain image index exceeded available render-finished semaphores.");
            return false;
        }

        VkSemaphore renderFinishedSemaphore = m_renderFinishedSemaphores[imageIndex];
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        VkCommandBuffer commandBuffers[] = {m_commandBuffer->getVulkanCommandBuffer()};

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_imageAvailableSemaphore;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = commandBuffers;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderFinishedSemaphore;

        if (vkQueueSubmit(m_device->getGraphicsQueue(), 1, &submitInfo, m_inFlightFence) != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to submit Vulkan draw command buffer.");
            return false;
        }

        const VkResult presentResult =
            m_swapchain->present(imageIndex, renderFinishedSemaphore, m_device->getPresentQueue());

        if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR)
        {
            Logger::getInstance().error("Failed to present Vulkan swapchain image.");
            return false;
        }

        m_frames.remove(frameHandle);
        return true;
    }

    bool VulkanRenderer::recreateSwapchainResources(uint32_t width, uint32_t height)
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

        if (!m_physicalDevice->initialize(m_instance->getVulkanInstance(), m_surface->getVulkanSurface()))
        {
            return false;
        }

        if (!m_swapchain->initialize(*m_physicalDevice, *m_device, m_surface->getVulkanSurface(), width, height))
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
                VulkanShaderProgramResource* shaderProgram = m_shaderPrograms.get(resource.m_program);
                if (!shaderProgram)
                {
                    Logger::getInstance().error("Live graphics pipeline references an invalid shader program.");
                    return false;
                }

                const auto graphicsShaders = getGraphicsShaders(*shaderProgram);
                if (!graphicsShaders[0] || !graphicsShaders[1])
                {
                    Logger::getInstance().error("Live graphics pipeline is missing vertex or fragment shaders.");
                    return false;
                }

                RenderPass* renderPass = resolveRenderPass(resource.m_desc.renderTarget);
                if (!renderPass)
                {
                    Logger::getInstance().error("Live graphics pipeline references an invalid render target.");
                    return false;
                }

                if (!resource.m_pipeline.initialize(
                        *m_device, *renderPass,
                        std::span<const Shader* const>(graphicsShaders.data(), graphicsShaders.size()),
                        resource.m_desc))
                {
                    Logger::getInstance().error("Failed to recreate live graphics pipeline.");
                    return false;
                }

                return true;
            });
    }

    RenderPass* VulkanRenderer::resolveRenderPass(RenderTargetHandle renderTarget)
    {
        if (!renderTarget.isValid())
        {
            return m_renderPass.get();
        }

        VulkanRenderTargetResource* resource = m_renderTargets.get(renderTarget);
        if (!resource || !resource->m_renderPass)
        {
            return nullptr;
        }

        return resource->m_renderPass.get();
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

    bool VulkanRenderer::createDescriptorPool()
    {
        destroyDescriptorPool();
        if (!m_device)
        {
            return false;
        }

        std::array<VkDescriptorPoolSize, 3> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 128;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        poolSizes[1].descriptorCount = 64;
        poolSizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
        poolSizes[2].descriptorCount = 64;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 128;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();

        return vkCreateDescriptorPool(m_device->getVulkanDevice(), &poolInfo, nullptr, &m_descriptorPool) == VK_SUCCESS;
    }

    void VulkanRenderer::destroyDescriptorPool()
    {
        if (!m_device || m_descriptorPool == VK_NULL_HANDLE)
        {
            return;
        }

        vkDestroyDescriptorPool(m_device->getVulkanDevice(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
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
        if (vkCreateSemaphore(vkDevice, &semaphoreInfo, nullptr, &m_imageAvailableSemaphore) != VK_SUCCESS)
        {
            return false;
        }

        m_renderFinishedSemaphores.resize(m_swapchain->getImageCount(), VK_NULL_HANDLE);

        for (VkSemaphore& semaphore : m_renderFinishedSemaphores)
        {
            if (vkCreateSemaphore(vkDevice, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS)
            {
                destroySyncObjects();
                return false;
            }
        }

        if (vkCreateFence(vkDevice, &fenceInfo, nullptr, &m_inFlightFence) != VK_SUCCESS)
        {
            destroySyncObjects();
            return false;
        }

        return true;
    }
}  // namespace kera
