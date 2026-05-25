#include "kera/renderer/backend/vulkan/vulkan_renderer.h"

#include "kera/core/window.h"
#include "kera/renderer/command_buffer.h"
#include "kera/renderer/descriptor_contracts.h"
#include "kera/renderer/device.h"
#include "kera/renderer/instance.h"
#include "kera/renderer/physical_device.h"
#include "kera/renderer/surface.h"
#include "kera/renderer/swapchain.h"
#include "kera/utilities/logger.h"

#include <array>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
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
        constexpr uint32_t kMaxFramesInFlight = 2;

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
                case TextureFormat::Depth32:
                    return VK_FORMAT_D32_SFLOAT;
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

        VkAccessFlags2 accessMask2ForLayout(VkImageLayout layout)
        {
            switch (layout)
            {
                case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                    return VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                    return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                    return VK_ACCESS_2_TRANSFER_WRITE_BIT;
                case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                    return VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
                case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                case VK_IMAGE_LAYOUT_UNDEFINED:
                default:
                    return 0;
            }
        }

        VkPipelineStageFlags2 stageMask2ForLayout(VkImageLayout layout)
        {
            switch (layout)
            {
                case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                    return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                    return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
                case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                    return VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                    return VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                case VK_IMAGE_LAYOUT_UNDEFINED:
                default:
                    return VK_PIPELINE_STAGE_2_NONE;
            }
        }

        const char* layoutName(VkImageLayout layout)
        {
            switch (layout)
            {
                case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                    return "ColorAttachment";
                case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                    return "DepthAttachment";
                case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                    return "TransferDst";
                case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                    return "ShaderRead";
                case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                    return "Present";
                case VK_IMAGE_LAYOUT_UNDEFINED:
                default:
                    return "Undefined";
            }
        }

        VkPipelineStageFlags2 signalStageForTimelineSubmit(VkPipelineStageFlags2 producerStage)
        {
            return producerStage == 0 ? VK_PIPELINE_STAGE_2_NONE : producerStage;
        }

        VkPipelineStageFlags2 renderCompleteStageMask()
        {
            return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        }

        void setDebugObjectName(VkDevice device, VkObjectType objectType, uint64_t objectHandle, const std::string& name)
        {
            if (device == VK_NULL_HANDLE || objectHandle == 0 || name.empty())
            {
                return;
            }

            auto setName =
                reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetDeviceProcAddr(device,
                                                                                       "vkSetDebugUtilsObjectNameEXT"));
            if (!setName)
            {
                return;
            }

            VkDebugUtilsObjectNameInfoEXT nameInfo{};
            nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            nameInfo.objectType = objectType;
            nameInfo.objectHandle = objectHandle;
            nameInfo.pObjectName = name.c_str();
            setName(device, &nameInfo);
        }

        void beginDebugLabel(VkDevice device, VkCommandBuffer commandBuffer, const char* name, float r, float g, float b)
        {
            if (device == VK_NULL_HANDLE || commandBuffer == VK_NULL_HANDLE || !name)
            {
                return;
            }

            auto beginLabel = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
                vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT"));
            if (!beginLabel)
            {
                return;
            }

            VkDebugUtilsLabelEXT label{};
            label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
            label.pLabelName = name;
            label.color[0] = r;
            label.color[1] = g;
            label.color[2] = b;
            label.color[3] = 1.0f;
            beginLabel(commandBuffer, &label);
        }

        void endDebugLabel(VkDevice device, VkCommandBuffer commandBuffer)
        {
            if (device == VK_NULL_HANDLE || commandBuffer == VK_NULL_HANDLE)
            {
                return;
            }

            auto endLabel =
                reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetDeviceProcAddr(device,
                                                                                     "vkCmdEndDebugUtilsLabelEXT"));
            if (endLabel)
            {
                endLabel(commandBuffer);
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
        , m_frameTimelineSemaphore(VK_NULL_HANDLE)
        , m_nextFrameTimelineValue(1)
        , m_pipelineCache(VK_NULL_HANDLE)
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
        Logger::getInstance().info(m_instance->isDebugMessengerActive()
                                       ? "Vulkan debug messenger is active."
                                       : "Vulkan debug messenger is not active.");

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

        VkPipelineCacheCreateInfo pipelineCacheInfo{};
        pipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        if (vkCreatePipelineCache(m_device->getVulkanDevice(), &pipelineCacheInfo, nullptr, &m_pipelineCache) !=
            VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to create Vulkan pipeline cache");
            return false;
        }
        setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_PIPELINE_CACHE,
                           (uint64_t)m_pipelineCache, "Kera Pipeline Cache");

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
        flushDeferredDeletions();
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
        if (m_device && m_pipelineCache != VK_NULL_HANDLE)
        {
            vkDestroyPipelineCache(m_device->getVulkanDevice(), m_pipelineCache, nullptr);
            m_pipelineCache = VK_NULL_HANDLE;
        }

        for (std::unique_ptr<CommandBuffer>& commandBuffer : m_commandBuffers)
        {
            if (commandBuffer)
            {
                commandBuffer->shutdown();
            }
        }
        m_commandBuffers.clear();
        m_swapchainImageLayouts.clear();

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

        if (!m_window || !m_instance || !m_device || !m_swapchain)
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

        const VkFormat uiColorAttachmentFormat = m_swapchain->getImageFormat();
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
        initInfo.UseDynamicRendering = true;
        initInfo.PipelineInfoMain.RenderPass = VK_NULL_HANDLE;
        initInfo.PipelineInfoMain.Subpass = 0;
        initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
#ifdef IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType =
            VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &uiColorAttachmentFormat;
#endif

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
        if (!frame->m_renderPassActive)
        {
            Logger::getInstance().error("Cannot render ImGui draw data outside an active Vulkan render pass.");
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
            m_swapchainRecreateRequested = true;
            return true;
        }

        if (hasActiveFrames())
        {
            Logger::getInstance().warning("Deferring Vulkan resize while a frame is active.");
            m_swapchainRecreateRequested = true;
            return false;
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

        m_swapchainRecreateRequested = false;
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
        bool referencedByPipeline = false;
        m_graphicsPipelines.forEach(
            [&referencedByPipeline, program](GraphicsPipelineHandle, VulkanGraphicsPipelineResource& pipeline)
            {
                if (pipeline.m_program == program)
                {
                    referencedByPipeline = true;
                    return false;
                }
                return true;
            });
        if (referencedByPipeline)
        {
            Logger::getInstance().error("Cannot destroy a Vulkan shader program referenced by a graphics pipeline.");
            return false;
        }

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

        BufferHandle handle = m_buffers.emplace(std::move(resource));
        if (VulkanBufferResource* namedResource = m_buffers.get(handle))
        {
            setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_BUFFER,
                               (uint64_t)namedResource->m_buffer.getVulkanBuffer(), "Kera Buffer");
        }
        return handle;
    }

    bool VulkanRenderer::destroyBuffer(BufferHandle buffer)
    {
        if (descriptorSetsReference(buffer))
        {
            Logger::getInstance().error("Cannot destroy a Vulkan buffer that is still referenced by a descriptor set.");
            return false;
        }

        std::optional<VulkanBufferResource> resource = m_buffers.take(buffer);
        if (!resource)
        {
            return false;
        }

        VulkanDeferredDeletion deletion{};
        deletion.m_timelineValue = getLastSubmittedTimelineValue();
        deletion.m_bufferResources.push_back(std::move(*resource));
        queueDeferredDeletion(std::move(deletion));
        return true;
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

    BufferHandle VulkanRenderer::createUniformRingBuffer(std::size_t elementSize, uint32_t slotCount)
    {
        if (elementSize == 0)
        {
            Logger::getInstance().error("Uniform ring buffer element size must be non-zero.");
            return {};
        }

        const uint32_t resolvedSlotCount =
            slotCount == 0 ? static_cast<uint32_t>(m_frameSyncResources.empty() ? kMaxFramesInFlight
                                                                                : m_frameSyncResources.size())
                           : slotCount;
        VulkanBufferResource resource{};
        resource.m_ringSlotSize = elementSize;
        resource.m_ringSlotCount = resolvedSlotCount;
        if (!resource.m_buffer.initialize(*m_device, static_cast<VkDeviceSize>(elementSize * resolvedSlotCount),
                                          BufferUsage::Uniform, toMemoryFlags(MemoryAccess::CpuWrite)))
        {
            Logger::getInstance().error("Failed to create Vulkan uniform ring buffer.");
            return {};
        }

        BufferHandle handle = m_buffers.emplace(std::move(resource));
        if (VulkanBufferResource* namedResource = m_buffers.get(handle))
        {
            setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_BUFFER,
                               (uint64_t)namedResource->m_buffer.getVulkanBuffer(),
                               "Kera Uniform Ring Buffer");
        }
        return handle;
    }

    bool VulkanRenderer::uploadUniformRingBuffer(BufferHandle bufferHandle, FrameHandle frameHandle, const void* data,
                                                 std::size_t size)
    {
        VulkanBufferResource* buffer = m_buffers.get(bufferHandle);
        VulkanFrameResource* frame = m_frames.get(frameHandle);
        if (!buffer || !frame)
        {
            Logger::getInstance().error("Invalid handle passed to uploadUniformRingBuffer.");
            return false;
        }
        if (buffer->m_ringSlotSize == 0 || buffer->m_ringSlotCount == 0)
        {
            Logger::getInstance().error("Buffer is not a Vulkan uniform ring buffer.");
            return false;
        }
        if (size > buffer->m_ringSlotSize)
        {
            Logger::getInstance().error("Uniform ring buffer upload exceeds slot size.");
            return false;
        }

        const std::size_t offset = getUniformRingBufferOffset(bufferHandle, frameHandle);
        return uploadBuffer(bufferHandle, data, size, offset);
    }

    std::size_t VulkanRenderer::getUniformRingBufferOffset(BufferHandle bufferHandle, FrameHandle frameHandle) const
    {
        const VulkanBufferResource* buffer = m_buffers.get(bufferHandle);
        const VulkanFrameResource* frame = m_frames.get(frameHandle);
        if (!buffer || !frame || buffer->m_ringSlotSize == 0 || buffer->m_ringSlotCount == 0)
        {
            return 0;
        }

        return buffer->m_ringSlotSize * (frame->m_syncIndex % buffer->m_ringSlotCount);
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
            usage |= desc.depthStencil ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        if (desc.sampled)
        {
            usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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
        resource.m_aspectMask = desc.depthStencil ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        resource.m_sampled = desc.sampled;
        resource.m_renderTarget = desc.renderTarget;
        resource.m_depthStencil = desc.depthStencil;
        resource.m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        resource.m_descriptorLayout = desc.sampled ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
        resource.m_renderTargetFinalLayout = desc.renderTarget
                                                 ? (desc.depthStencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                                                                      : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                                 : VK_IMAGE_LAYOUT_UNDEFINED;

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
        viewInfo.subresourceRange.aspectMask = resource.m_aspectMask;
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

        TextureHandle handle = m_textures.emplace(std::move(resource));
        if (VulkanTextureResource* namedResource = m_textures.get(handle))
        {
            setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_IMAGE,
                               (uint64_t)namedResource->m_image, "Kera Texture Image");
            setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_IMAGE_VIEW,
                               (uint64_t)namedResource->m_imageView, "Kera Texture View");
        }
        return handle;
    }

    bool VulkanRenderer::uploadTexture(TextureHandle textureHandle, const void* data, std::size_t size)
    {
        if (!m_device)
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return false;
        }
        if (!data || size == 0)
        {
            Logger::getInstance().error("Texture upload data must be non-empty.");
            return false;
        }

        VulkanTextureResource* texture = m_textures.get(textureHandle);
        if (!texture)
        {
            Logger::getInstance().error("Invalid texture handle passed to uploadTexture.");
            return false;
        }
        if (!texture->m_sampled || texture->m_descriptorLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
            Logger::getInstance().error("Texture upload requires a sampled Vulkan texture.");
            return false;
        }
        if (texture->m_depthStencil)
        {
            Logger::getInstance().error("Depth texture upload is not supported by uploadTexture.");
            return false;
        }

        const std::size_t expectedSize =
            static_cast<std::size_t>(texture->m_extent.width) * static_cast<std::size_t>(texture->m_extent.height) * 4u;
        if (size < expectedSize)
        {
            Logger::getInstance().error("Texture upload data is smaller than the texture extent requires.");
            return false;
        }

        Buffer stagingBuffer = acquireStagingBuffer(static_cast<VkDeviceSize>(expectedSize));
        if (!stagingBuffer.isValid())
        {
            Logger::getInstance().error("Failed to create Vulkan texture staging buffer.");
            return false;
        }
        if (!stagingBuffer.copyFrom(data, static_cast<VkDeviceSize>(expectedSize)))
        {
            Logger::getInstance().error("Failed to upload data to Vulkan texture staging buffer.");
            return false;
        }

        if (!copyBufferToTexture(stagingBuffer, *texture))
        {
            Logger::getInstance().error("Failed to copy Vulkan staging buffer to texture.");
            return false;
        }

        return true;
    }

    bool VulkanRenderer::destroyTexture(TextureHandle texture)
    {
        if (descriptorSetsReference(texture))
        {
            Logger::getInstance().error("Cannot destroy a Vulkan texture that is still referenced by a descriptor set.");
            return false;
        }

        if (renderTargetsReference(texture))
        {
            Logger::getInstance().error("Cannot destroy a Vulkan texture that is still referenced by a render target.");
            return false;
        }

        std::optional<VulkanTextureResource> resource = m_textures.take(texture);
        if (!resource)
        {
            return false;
        }

        VulkanDeferredDeletion deletion{};
        deletion.m_timelineValue = getLastSubmittedTimelineValue();
        deletion.m_textures.push_back(std::move(*resource));
        queueDeferredDeletion(std::move(deletion));
        return true;
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

        SamplerHandle handle = m_samplers.emplace(std::move(resource));
        if (VulkanSamplerResource* namedResource = m_samplers.get(handle))
        {
            setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_SAMPLER,
                               (uint64_t)namedResource->m_sampler, "Kera Sampler");
        }
        return handle;
    }

    bool VulkanRenderer::destroySampler(SamplerHandle sampler)
    {
        if (descriptorSetsReference(sampler))
        {
            Logger::getInstance().error("Cannot destroy a Vulkan sampler that is still referenced by a descriptor set.");
            return false;
        }

        std::optional<VulkanSamplerResource> resource = m_samplers.take(sampler);
        if (!resource)
        {
            return false;
        }

        VulkanDeferredDeletion deletion{};
        deletion.m_timelineValue = getLastSubmittedTimelineValue();
        deletion.m_samplers.push_back(std::move(*resource));
        queueDeferredDeletion(std::move(deletion));
        return true;
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
        if (!texture->m_renderTarget)
        {
            Logger::getInstance().error("Texture passed to createRenderTarget was not created for render-target usage.");
            return {};
        }
        if (texture->m_depthStencil)
        {
            Logger::getInstance().error("Color texture passed to createRenderTarget must not be a depth texture.");
            return {};
        }

        VulkanTextureResource* depthTexture = nullptr;
        if (desc.depthTexture.isValid())
        {
            depthTexture = m_textures.get(desc.depthTexture);
            if (!depthTexture)
            {
                Logger::getInstance().error("Invalid depth texture passed to createRenderTarget.");
                return {};
            }
            if (!depthTexture->m_renderTarget || !depthTexture->m_depthStencil)
            {
                Logger::getInstance().error("Depth texture passed to createRenderTarget is not depth render-target usage.");
                return {};
            }
            if (depthTexture->m_extent.width != texture->m_extent.width ||
                depthTexture->m_extent.height != texture->m_extent.height)
            {
                Logger::getInstance().error("Render target color and depth textures must have matching extents.");
                return {};
            }
        }

        VulkanRenderTargetResource resource{};
        resource.m_colorTexture = desc.colorTexture;
        resource.m_depthTexture = desc.depthTexture;
        resource.m_extent = texture->m_extent;
        return m_renderTargets.emplace(std::move(resource));
    }

    bool VulkanRenderer::destroyRenderTarget(RenderTargetHandle renderTarget)
    {
        return m_renderTargets.remove(renderTarget);
    }

    GraphicsPipelineHandle VulkanRenderer::createGraphicsPipeline(const GraphicsPipelineDesc& desc,
                                                                  ShaderProgramHandle program)
    {
        if (!m_device || !m_swapchain)
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return {};
        }

        VkFormat colorFormat = VK_FORMAT_UNDEFINED;
        VkFormat depthFormat = VK_FORMAT_UNDEFINED;
        if (!resolvePipelineRenderingFormats(desc.renderTarget, colorFormat, depthFormat))
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
        if (!resource.m_pipeline.initialize(*m_device, m_pipelineCache, colorFormat, depthFormat,
                                            std::span<const Shader* const>(graphicsShaders.data(),
                                                                          graphicsShaders.size()),
                                            desc))
        {
            Logger::getInstance().error("Failed to create Vulkan graphics pipeline.");
            return {};
        }

        GraphicsPipelineHandle handle = m_graphicsPipelines.emplace(std::move(resource));
        if (VulkanGraphicsPipelineResource* namedResource = m_graphicsPipelines.get(handle))
        {
            setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_PIPELINE,
                               (uint64_t)namedResource->m_pipeline.getVulkanPipeline(),
                               "Kera Graphics Pipeline");
        }
        return handle;
    }

    bool VulkanRenderer::destroyGraphicsPipeline(GraphicsPipelineHandle pipeline)
    {
        std::optional<VulkanGraphicsPipelineResource> resource = m_graphicsPipelines.take(pipeline);
        if (!resource)
        {
            return false;
        }

        VulkanDeferredDeletion deletion{};
        deletion.m_timelineValue = getLastSubmittedTimelineValue();
        deletion.m_graphicsPipelines.push_back(std::move(*resource));
        queueDeferredDeletion(std::move(deletion));
        return true;
    }

    DescriptorSetHandle VulkanRenderer::createDescriptorSet(GraphicsPipelineHandle pipelineHandle, uint32_t set)
    {
        if (!m_device || m_descriptorPools.empty())
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

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        if (!allocateDescriptorSet(*pipeline, set, descriptorSet, descriptorPool))
        {
            Logger::getInstance().error("Failed to allocate Vulkan descriptor set.");
            return {};
        }

        VulkanDescriptorSetResource resource{};
        resource.m_descriptorSet = descriptorSet;
        resource.m_descriptorPool = descriptorPool;
        resource.m_pipeline = pipelineHandle;
        resource.m_set = set;
        const DescriptorSetLayoutDesc* layoutDesc = resolveDescriptorSetLayout(*pipeline, set);
        if (!layoutDesc)
        {
            Logger::getInstance().error("Graphics pipeline descriptor set metadata is missing.");
            vkFreeDescriptorSets(m_device->getVulkanDevice(), descriptorPool, 1, &descriptorSet);
            return {};
        }
        resource.m_layout = *layoutDesc;
        DescriptorSetHandle descriptorSetHandle = m_descriptorSets.emplace(resource);
        setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_DESCRIPTOR_SET,
                           (uint64_t)descriptorSet, "Kera Descriptor Set");
        return descriptorSetHandle;
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
        if (!validateDescriptorBinding(*descriptorSet, binding, DescriptorType::UniformBuffer))
        {
            Logger::getInstance().error("Descriptor binding does not accept a uniform buffer.");
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
        bool updatedBinding = false;
        for (auto& reference : descriptorSet->m_buffers)
        {
            if (reference.m_binding == binding)
            {
                reference.m_handle = bufferHandle;
                reference.m_offset = offset;
                reference.m_range = static_cast<std::size_t>(descriptorRange);
                updatedBinding = true;
                break;
            }
        }
        if (!updatedBinding)
        {
            descriptorSet->m_buffers.push_back({binding, bufferHandle, offset, static_cast<std::size_t>(descriptorRange)});
        }
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
        if (!validateDescriptorBinding(*descriptorSet, binding, DescriptorType::SampledImage))
        {
            Logger::getInstance().error("Descriptor binding does not accept a sampled image.");
            return false;
        }
        if (!texture->m_sampled || texture->m_descriptorLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            Logger::getInstance().error("Texture is not compatible with sampled-image descriptor usage.");
            return false;
        }

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = texture->m_descriptorLayout;
        imageInfo.imageView = texture->m_imageView;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet->m_descriptorSet;
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_device->getVulkanDevice(), 1, &write, 0, nullptr);
        bool updatedBinding = false;
        for (auto& reference : descriptorSet->m_textures)
        {
            if (reference.m_binding == binding)
            {
                reference.m_handle = textureHandle;
                updatedBinding = true;
                break;
            }
        }
        if (!updatedBinding)
        {
            descriptorSet->m_textures.push_back({binding, textureHandle, 0, 0});
        }
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
        if (!validateDescriptorBinding(*descriptorSet, binding, DescriptorType::Sampler))
        {
            Logger::getInstance().error("Descriptor binding does not accept a sampler.");
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
        bool updatedBinding = false;
        for (auto& reference : descriptorSet->m_samplers)
        {
            if (reference.m_binding == binding)
            {
                reference.m_handle = samplerHandle;
                updatedBinding = true;
                break;
            }
        }
        if (!updatedBinding)
        {
            descriptorSet->m_samplers.push_back({binding, samplerHandle, 0, 0});
        }
        return true;
    }

    bool VulkanRenderer::destroyDescriptorSet(DescriptorSetHandle setHandle)
    {
        VulkanDescriptorSetResource* descriptorSet = m_descriptorSets.get(setHandle);
        if (!descriptorSet)
        {
            return false;
        }

        std::optional<VulkanDescriptorSetResource> resource = m_descriptorSets.take(setHandle);
        if (!resource)
        {
            return false;
        }

        VulkanDeferredDeletion deletion{};
        deletion.m_timelineValue = getLastSubmittedTimelineValue();
        deletion.m_descriptorSets.push_back(std::move(*resource));
        queueDeferredDeletion(std::move(deletion));
        return true;
    }

    FrameHandle VulkanRenderer::beginFrame()
    {
        if (!m_device || !m_swapchain || m_commandBuffers.empty() || m_frameSyncResources.empty())
        {
            Logger::getInstance().error("Renderer frame resources are not initialized.");
            return {};
        }

        const uint32_t syncIndex = m_currentFrameSyncIndex;
        VulkanFrameSyncResource& frameSync = m_frameSyncResources[syncIndex];
        CommandBuffer& commandBuffer = *m_commandBuffers[syncIndex];
        if (syncIndex >= m_activeFrameHandles.size())
        {
            Logger::getInstance().error("Frame sync slot is missing active-frame tracking.");
            return {};
        }

        FrameHandle& activeFrameHandle = m_activeFrameHandles[syncIndex];
        if (activeFrameHandle.isValid())
        {
            if (m_frames.get(activeFrameHandle))
            {
                Logger::getInstance().error("Cannot begin a new Vulkan frame while the current sync slot is active.");
                return {};
            }
            activeFrameHandle = {};
        }

        if (!waitForTimelineValue(frameSync.m_timelineValue))
        {
            Logger::getInstance().error("Failed to wait for Vulkan frame timeline.");
            return {};
        }
        commandBuffer.markCompleted();
        clearCompletedFrameResourceUse(syncIndex);
        collectDeferredDeletions();

        const uint32_t swapchainImageCount = m_swapchain->getImageCount();
        if (m_imagesInFlight.size() != swapchainImageCount || m_swapchainImageLayouts.size() != swapchainImageCount ||
            m_swapchain->getImageViews().size() < swapchainImageCount)
        {
            Logger::getInstance().error("Swapchain frame resources do not match swapchain image count.");
            return {};
        }

        if (!commandBuffer.reset() || !commandBuffer.begin())
        {
            Logger::getInstance().error("Failed to begin Vulkan command buffer.");
            return {};
        }

        uint32_t imageIndex = 0;
        const VkResult acquireResult =
            m_swapchain->acquireNextImage(frameSync.m_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            Logger::getInstance().info("Vulkan swapchain is out of date during image acquire; recreating.");
            m_swapchainRecreateRequested = true;
            commandBuffer.reset();
            if (!recreateSwapchainFromWindow())
            {
                Logger::getInstance().error("Failed to recreate Vulkan swapchain after image acquire.");
            }
            return {};
        }

        if (acquireResult == VK_SUBOPTIMAL_KHR)
        {
            m_swapchainRecreateRequested = true;
        }
        else if (acquireResult != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to acquire Vulkan swapchain image.");
            commandBuffer.reset();
            return {};
        }

        if (imageIndex >= m_imagesInFlight.size())
        {
            Logger::getInstance().error("Swapchain image index exceeded in-flight image tracking.");
            commandBuffer.reset();
            return {};
        }

        if (m_imagesInFlight[imageIndex] != 0)
        {
            if (!waitForTimelineValue(m_imagesInFlight[imageIndex]))
            {
                Logger::getInstance().error("Failed to wait for in-flight Vulkan swapchain image timeline.");
                commandBuffer.reset();
                return {};
            }
            collectDeferredDeletions();
        }

        VkImageView imageView = m_swapchain->getImageViews()[imageIndex];
        if (imageView == VK_NULL_HANDLE)
        {
            Logger::getInstance().error("Failed to get Vulkan swapchain image view.");
            commandBuffer.reset();
            return {};
        }

        VulkanFrameResource frame{};
        frame.m_commandBuffer = commandBuffer.getVulkanCommandBuffer();
        frame.m_colorImageView = imageView;
        frame.m_extent = m_swapchain->getExtent();
        frame.m_imageIndex = imageIndex;
        frame.m_syncIndex = syncIndex;
        m_stats.drawCallsThisFrame = 0;
        ++m_stats.frameIndex;
        FrameHandle frameHandle = m_frames.emplace(frame);
        activeFrameHandle = frameHandle;
        return frameHandle;
    }

    void VulkanRenderer::beginRenderPass(FrameHandle frameHandle, const RenderPassDesc& desc)
    {
        VulkanFrameResource* frame = m_frames.get(frameHandle);
        if (!frame)
        {
            Logger::getInstance().error("Invalid frame handle passed to beginRenderPass.");
            return;
        }
        if (frame->m_renderPassActive)
        {
            Logger::getInstance().error("Cannot begin a Vulkan render pass while another render pass is active.");
            return;
        }

        transitionSwapchainImageLayout(frame->m_commandBuffer, frame->m_imageIndex,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = frame->m_colorImageView;
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color = {{desc.clearColor.r, desc.clearColor.g, desc.clearColor.b,
                                             desc.clearColor.a}};

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.offset = {0, 0};
        renderingInfo.renderArea.extent = frame->m_extent;
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;

        beginDebugLabel(m_device->getVulkanDevice(), frame->m_commandBuffer, "Kera Backbuffer Pass", 0.2f, 0.4f, 0.9f);
        vkCmdBeginRendering(frame->m_commandBuffer, &renderingInfo);
        frame->m_renderPassActive = true;
        frame->m_activeRenderTargetTexture = {};
        frame->m_activeDepthTexture = {};
        frame->m_renderPassFinalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

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
        if (!frame || !renderTarget)
        {
            Logger::getInstance().error("Invalid handle passed to render target beginRenderPass.");
            return;
        }
        if (frame->m_renderPassActive)
        {
            Logger::getInstance().error("Cannot begin a Vulkan render target pass while another render pass is active.");
            return;
        }
        VulkanTextureResource* texture = m_textures.get(renderTarget->m_colorTexture);
        if (!texture)
        {
            Logger::getInstance().error("Render target color texture is invalid.");
            return;
        }

        transitionTextureLayout(frame->m_commandBuffer, *texture, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VulkanTextureResource* depthTexture =
            renderTarget->m_depthTexture.isValid() ? m_textures.get(renderTarget->m_depthTexture) : nullptr;
        if (renderTarget->m_depthTexture.isValid() && !depthTexture)
        {
            Logger::getInstance().error("Render target depth texture is invalid.");
            return;
        }
        if (depthTexture)
        {
            transitionTextureLayout(frame->m_commandBuffer, *depthTexture, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = texture->m_imageView;
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color = {{desc.clearColor.r, desc.clearColor.g, desc.clearColor.b,
                                             desc.clearColor.a}};

        VkRenderingAttachmentInfo depthAttachment{};
        if (depthTexture)
        {
            depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment.imageView = depthTexture->m_imageView;
            depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.clearValue.depthStencil = {desc.clearDepth, 0};
        }

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.offset = {0, 0};
        renderingInfo.renderArea.extent = renderTarget->m_extent;
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.pDepthAttachment = depthTexture ? &depthAttachment : nullptr;

        beginDebugLabel(m_device->getVulkanDevice(), frame->m_commandBuffer, "Kera Render Target Pass", 0.4f, 0.8f, 0.4f);
        vkCmdBeginRendering(frame->m_commandBuffer, &renderingInfo);
        frame->m_renderPassActive = true;
        frame->m_activeRenderTargetTexture = renderTarget->m_colorTexture;
        frame->m_activeDepthTexture = renderTarget->m_depthTexture;
        frame->m_renderPassFinalLayout = texture->m_renderTargetFinalLayout;

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
        if (!frame->m_renderPassActive)
        {
            Logger::getInstance().error("Cannot end a Vulkan render pass when no render pass is active.");
            return;
        }

        vkCmdEndRendering(frame->m_commandBuffer);
        endDebugLabel(m_device->getVulkanDevice(), frame->m_commandBuffer);
        frame->m_renderPassActive = false;
        if (frame->m_activeRenderTargetTexture.isValid())
        {
            VulkanTextureResource* texture = m_textures.get(frame->m_activeRenderTargetTexture);
            if (texture)
            {
                transitionTextureLayout(frame->m_commandBuffer, *texture, frame->m_renderPassFinalLayout);
            }
            frame->m_activeRenderTargetTexture = {};
        }
        else
        {
            transitionSwapchainImageLayout(frame->m_commandBuffer, frame->m_imageIndex, frame->m_renderPassFinalLayout);
        }
        if (frame->m_activeDepthTexture.isValid())
        {
            VulkanTextureResource* texture = m_textures.get(frame->m_activeDepthTexture);
            if (texture)
            {
                texture->m_currentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }
            frame->m_activeDepthTexture = {};
        }
        frame->m_renderPassFinalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
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
        if (!frame->m_renderPassActive)
        {
            Logger::getInstance().error("Cannot bind a Vulkan graphics pipeline outside an active render pass.");
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
        if (!frame->m_renderPassActive)
        {
            Logger::getInstance().error("Cannot bind a Vulkan vertex buffer outside an active render pass.");
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
        if (!frame->m_renderPassActive)
        {
            Logger::getInstance().error("Cannot bind a Vulkan index buffer outside an active render pass.");
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
        if (!frame->m_renderPassActive)
        {
            Logger::getInstance().error("Cannot bind a Vulkan descriptor set outside an active render pass.");
            return;
        }

        if (descriptorSet->m_set != set || descriptorSet->m_pipeline != pipelineHandle)
        {
            Logger::getInstance().error("Descriptor set is not compatible with the requested pipeline set.");
            return;
        }

        const DescriptorSetLayoutDesc* expectedLayout = resolveDescriptorSetLayout(*pipeline, set);
        if (!expectedLayout || !descriptorSetLayoutsCompatible(descriptorSet->m_layout, *expectedLayout))
        {
            Logger::getInstance().error("Descriptor set layout is not compatible with the requested pipeline.");
            return;
        }

        vkCmdBindDescriptorSets(frame->m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline->m_pipeline.getPipelineLayout(), set, 1, &descriptorSet->m_descriptorSet, 0,
                                nullptr);
        recordDescriptorSetUse(frame->m_syncIndex, descriptorSetHandle, *descriptorSet);
    }

    void VulkanRenderer::drawIndexed(FrameHandle frameHandle, uint32_t indexCount, uint32_t instanceCount)
    {
        VulkanFrameResource* frame = m_frames.get(frameHandle);
        if (!frame)
        {
            Logger::getInstance().error("Invalid frame handle passed to drawIndexed.");
            return;
        }
        if (!frame->m_renderPassActive)
        {
            Logger::getInstance().error("Cannot draw Vulkan indexed geometry outside an active render pass.");
            return;
        }

        vkCmdDrawIndexed(frame->m_commandBuffer, indexCount, instanceCount, 0, 0, 0);
        ++m_stats.drawCallsThisFrame;
    }

    bool VulkanRenderer::endFrame(FrameHandle frameHandle)
    {
        if (!m_device || m_commandBuffers.empty() || m_frameSyncResources.empty() || !m_swapchain)
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

        if (frame->m_syncIndex >= m_frameSyncResources.size() || frame->m_syncIndex >= m_commandBuffers.size())
        {
            Logger::getInstance().error("Frame references an invalid sync resource.");
            releaseFrame(frameHandle, frame->m_syncIndex);
            return false;
        }

        const uint32_t syncIndex = frame->m_syncIndex;

        if (syncIndex >= m_activeFrameHandles.size() || m_activeFrameHandles[syncIndex] != frameHandle)
        {
            Logger::getInstance().error("Frame handle does not match the active Vulkan frame slot.");
            releaseFrame(frameHandle, syncIndex);
            return false;
        }

        if (frame->m_renderPassActive)
        {
            Logger::getInstance().error("Cannot end a Vulkan frame while a render pass is still active.");
            releaseFrame(frameHandle, syncIndex);
            return false;
        }

        VulkanFrameSyncResource& frameSync = m_frameSyncResources[syncIndex];
        CommandBuffer& commandBuffer = *m_commandBuffers[syncIndex];

        if (!commandBuffer.end())
        {
            Logger::getInstance().error("Failed to end Vulkan command buffer.");
            releaseFrame(frameHandle, syncIndex);
            return false;
        }

        const uint32_t imageIndex = frame->m_imageIndex;
        if (imageIndex >= m_swapchain->getImageCount() || imageIndex >= m_imagesInFlight.size())
        {
            Logger::getInstance().error("Swapchain image index exceeded available swapchain images.");
            releaseFrame(frameHandle, syncIndex);
            return false;
        }

        const uint64_t signalTimelineValue = reserveTimelineValue();

        VkSemaphoreSubmitInfo waitSemaphoreInfo{};
        waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waitSemaphoreInfo.semaphore = frameSync.m_imageAvailableSemaphore;
        waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkCommandBufferSubmitInfo commandBufferInfo{};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferInfo.commandBuffer = frame->m_commandBuffer;

        std::array<VkSemaphoreSubmitInfo, 2> signalSemaphoreInfos{};
        signalSemaphoreInfos[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalSemaphoreInfos[0].semaphore = frameSync.m_renderFinishedSemaphore;
        signalSemaphoreInfos[0].stageMask = renderCompleteStageMask();
        signalSemaphoreInfos[1].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalSemaphoreInfos[1].semaphore = m_frameTimelineSemaphore;
        signalSemaphoreInfos[1].value = signalTimelineValue;
        signalSemaphoreInfos[1].stageMask = renderCompleteStageMask();

        VkSubmitInfo2 submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfo.waitSemaphoreInfoCount = 1;
        submitInfo.pWaitSemaphoreInfos = &waitSemaphoreInfo;
        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos = &commandBufferInfo;
        submitInfo.signalSemaphoreInfoCount = static_cast<uint32_t>(signalSemaphoreInfos.size());
        submitInfo.pSignalSemaphoreInfos = signalSemaphoreInfos.data();
        VkResult submitResult =
            vkQueueSubmit2(m_device->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);

        if (submitResult != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to submit Vulkan draw command buffer.");
            releaseFrame(frameHandle, syncIndex);
            return false;
        }
        commandBuffer.markSubmitted();
        frameSync.m_timelineValue = signalTimelineValue;
        m_imagesInFlight[imageIndex] = signalTimelineValue;

        const VkResult presentResult =
            m_swapchain->present(imageIndex, frameSync.m_renderFinishedSemaphore, m_device->getPresentQueue());

        const bool shouldRecreateSwapchain = m_swapchainRecreateRequested ||
                                             presentResult == VK_ERROR_OUT_OF_DATE_KHR ||
                                             presentResult == VK_SUBOPTIMAL_KHR;

        if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR &&
            presentResult != VK_ERROR_OUT_OF_DATE_KHR)
        {
            Logger::getInstance().error("Failed to present Vulkan swapchain image.");
            releaseFrame(frameHandle, syncIndex);
            return false;
        }

        releaseFrame(frameHandle, syncIndex);
        m_currentFrameSyncIndex = (m_currentFrameSyncIndex + 1) % static_cast<uint32_t>(m_frameSyncResources.size());
        if (shouldRecreateSwapchain)
        {
            Logger::getInstance().info("Vulkan swapchain needs recreation after present.");
            if (!recreateSwapchainFromWindow())
            {
                Logger::getInstance().error("Failed to recreate Vulkan swapchain after present.");
                return false;
            }
        }
        collectDeferredDeletions();
        return true;
    }

    bool VulkanRenderer::recreateSwapchainResources(uint32_t width, uint32_t height)
    {
        if (!m_physicalDevice || !m_device || !m_surface)
        {
            return false;
        }

        if (hasActiveFrames())
        {
            Logger::getInstance().error("Cannot recreate Vulkan swapchain while a frame is active.");
            return false;
        }

        m_graphicsPipelines.forEach(
            [](GraphicsPipelineHandle, VulkanGraphicsPipelineResource& resource)
            {
                resource.m_pipeline.shutdown();
                return true;
            });
        m_frames.clear();
        m_activeFrameHandles.clear();

        for (std::unique_ptr<CommandBuffer>& commandBuffer : m_commandBuffers)
        {
            if (commandBuffer)
            {
                commandBuffer->shutdown();
            }
        }
        m_commandBuffers.clear();

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
        m_swapchainImageLayouts.assign(m_swapchain->getImageCount(), VK_IMAGE_LAYOUT_UNDEFINED);

        const uint32_t frameResourceCount =
            m_swapchain->getImageCount() < kMaxFramesInFlight ? m_swapchain->getImageCount() : kMaxFramesInFlight;
        if (frameResourceCount == 0)
        {
            return false;
        }

        m_commandBuffers.reserve(frameResourceCount);
        for (uint32_t index = 0; index < frameResourceCount; ++index)
        {
            auto commandBuffer = std::make_unique<CommandBuffer>();
            if (!commandBuffer->initialize(*m_device))
            {
                return false;
            }
            m_commandBuffers.push_back(std::move(commandBuffer));
        }

        return recreateLiveGraphicsPipelines();
    }

    bool VulkanRenderer::recreateLiveGraphicsPipelines()
    {
        if (!m_device || !m_swapchain)
        {
            return false;
        }

        return m_graphicsPipelines.forEach(
            [this](GraphicsPipelineHandle pipelineHandle, VulkanGraphicsPipelineResource& resource)
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

                VkFormat colorFormat = VK_FORMAT_UNDEFINED;
                VkFormat depthFormat = VK_FORMAT_UNDEFINED;
                if (!resolvePipelineRenderingFormats(resource.m_desc.renderTarget, colorFormat, depthFormat))
                {
                    Logger::getInstance().error("Live graphics pipeline references an invalid render target.");
                    return false;
                }

                if (!resource.m_pipeline.initialize(*m_device, m_pipelineCache, colorFormat, depthFormat,
                                                    std::span<const Shader* const>(graphicsShaders.data(),
                                                                                  graphicsShaders.size()),
                                                    resource.m_desc))
                {
                    Logger::getInstance().error("Failed to recreate live graphics pipeline.");
                    return false;
                }

                if (!reallocateDescriptorSetsForPipeline(pipelineHandle, resource))
                {
                    Logger::getInstance().error("Failed to refresh descriptor sets for recreated live pipeline.");
                    return false;
                }

                return true;
            });
    }

    bool VulkanRenderer::recreateSwapchainFromWindow()
    {
        if (!m_window)
        {
            Logger::getInstance().error("Cannot recreate Vulkan swapchain without a window.");
            return false;
        }

        const int width = m_window->getWidth();
        const int height = m_window->getHeight();
        if (width <= 0 || height <= 0)
        {
            m_swapchainRecreateRequested = true;
            return true;
        }

        return resize({
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
        });
    }

    bool VulkanRenderer::hasActiveFrames() const
    {
        return m_frames.activeCount() > 0;
    }

    void VulkanRenderer::releaseFrame(FrameHandle frameHandle, uint32_t syncIndex)
    {
        if (syncIndex < m_activeFrameHandles.size() && m_activeFrameHandles[syncIndex] == frameHandle)
        {
            m_activeFrameHandles[syncIndex] = {};
        }
        else
        {
            for (FrameHandle& activeFrameHandle : m_activeFrameHandles)
            {
                if (activeFrameHandle == frameHandle)
                {
                    activeFrameHandle = {};
                    break;
                }
            }
        }

        m_frames.remove(frameHandle);
    }

    bool VulkanRenderer::waitForTimelineValue(uint64_t timelineValue)
    {
        if (timelineValue == 0)
        {
            return true;
        }
        if (!m_device || m_frameTimelineSemaphore == VK_NULL_HANDLE)
        {
            return false;
        }

        VkSemaphoreWaitInfo waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.semaphoreCount = 1;
        waitInfo.pSemaphores = &m_frameTimelineSemaphore;
        waitInfo.pValues = &timelineValue;
        return vkWaitSemaphores(m_device->getVulkanDevice(), &waitInfo, UINT64_MAX) == VK_SUCCESS;
    }

    uint64_t VulkanRenderer::reserveTimelineValue()
    {
        return m_nextFrameTimelineValue++;
    }

    uint64_t VulkanRenderer::getLastSubmittedTimelineValue() const
    {
        uint64_t timelineValue = 0;
        for (const VulkanFrameSyncResource& frameSync : m_frameSyncResources)
        {
            timelineValue = timelineValue < frameSync.m_timelineValue ? frameSync.m_timelineValue : timelineValue;
        }
        for (const uint64_t imageTimelineValue : m_imagesInFlight)
        {
            timelineValue = timelineValue < imageTimelineValue ? imageTimelineValue : timelineValue;
        }
        return timelineValue;
    }

    bool VulkanRenderer::submitImmediateCommandBuffer(VkCommandBuffer commandBuffer, uint64_t& timelineValue)
    {
        if (!m_device || m_frameTimelineSemaphore == VK_NULL_HANDLE || commandBuffer == VK_NULL_HANDLE)
        {
            return false;
        }

        timelineValue = reserveTimelineValue();

        VkCommandBufferSubmitInfo commandBufferInfo{};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferInfo.commandBuffer = commandBuffer;

        VkSemaphoreSubmitInfo signalSemaphoreInfo{};
        signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalSemaphoreInfo.semaphore = m_frameTimelineSemaphore;
        signalSemaphoreInfo.value = timelineValue;
        signalSemaphoreInfo.stageMask = signalStageForTimelineSubmit(VK_PIPELINE_STAGE_2_TRANSFER_BIT);

        VkSubmitInfo2 submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos = &commandBufferInfo;
        submitInfo.signalSemaphoreInfoCount = 1;
        submitInfo.pSignalSemaphoreInfos = &signalSemaphoreInfo;
        return vkQueueSubmit2(m_device->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE) == VK_SUCCESS;
    }

    Buffer VulkanRenderer::acquireStagingBuffer(VkDeviceSize size)
    {
        for (auto bufferIt = m_uploadContext.m_availableStagingBuffers.begin();
             bufferIt != m_uploadContext.m_availableStagingBuffers.end(); ++bufferIt)
        {
            if (bufferIt->getSize() >= size)
            {
                Buffer buffer = std::move(*bufferIt);
                m_uploadContext.m_availableStagingBuffers.erase(bufferIt);
                return buffer;
            }
        }

        Buffer buffer;
        buffer.initialize(*m_device, size, BufferUsage::TransferSrc,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        return buffer;
    }

    void VulkanRenderer::releaseStagingBuffer(Buffer&& buffer)
    {
        if (buffer.isValid())
        {
            m_uploadContext.m_availableStagingBuffers.push_back(std::move(buffer));
        }
    }

    bool VulkanRenderer::allocateDescriptorSet(const VulkanGraphicsPipelineResource& pipeline, uint32_t set,
                                               VkDescriptorSet& descriptorSet, VkDescriptorPool& descriptorPool)
    {
        if (!m_device || m_descriptorPools.empty())
        {
            return false;
        }

        VkDescriptorSetLayout layout = pipeline.m_pipeline.getDescriptorSetLayout(set);
        if (layout == VK_NULL_HANDLE)
        {
            Logger::getInstance().error("Graphics pipeline does not declare the requested descriptor set.");
            return false;
        }

        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorSetCount = 1;
        allocateInfo.pSetLayouts = &layout;

        VkResult allocateResult = VK_ERROR_OUT_OF_POOL_MEMORY;
        for (VulkanDescriptorPoolResource& pool : m_descriptorPools)
        {
            allocateInfo.descriptorPool = pool.m_pool;
            allocateResult = vkAllocateDescriptorSets(m_device->getVulkanDevice(), &allocateInfo, &descriptorSet);
            if (allocateResult == VK_SUCCESS)
            {
                descriptorPool = pool.m_pool;
                ++pool.m_allocatedSets;
                break;
            }
        }

        if (allocateResult == VK_ERROR_OUT_OF_POOL_MEMORY || allocateResult == VK_ERROR_FRAGMENTED_POOL)
        {
            if (createDescriptorPoolBlock())
            {
                VulkanDescriptorPoolResource& pool = m_descriptorPools.back();
                allocateInfo.descriptorPool = pool.m_pool;
                allocateResult = vkAllocateDescriptorSets(m_device->getVulkanDevice(), &allocateInfo, &descriptorSet);
                if (allocateResult == VK_SUCCESS)
                {
                    descriptorPool = pool.m_pool;
                    ++pool.m_allocatedSets;
                }
            }
        }

        return allocateResult == VK_SUCCESS;
    }

    bool VulkanRenderer::reallocateDescriptorSetsForPipeline(GraphicsPipelineHandle pipelineHandle,
                                                             VulkanGraphicsPipelineResource& pipeline)
    {
        bool success = true;
        m_descriptorSets.forEach(
            [this, pipelineHandle, &pipeline, &success](DescriptorSetHandle descriptorSetHandle,
                                                        VulkanDescriptorSetResource& descriptorSet)
            {
                if (descriptorSet.m_pipeline != pipelineHandle)
                {
                    return true;
                }

                const std::vector<VulkanDescriptorBindingReference<BufferHandle>> buffers = descriptorSet.m_buffers;
                const std::vector<VulkanDescriptorBindingReference<TextureHandle>> textures = descriptorSet.m_textures;
                const std::vector<VulkanDescriptorBindingReference<SamplerHandle>> samplers = descriptorSet.m_samplers;

                VkDescriptorSet newDescriptorSet = VK_NULL_HANDLE;
                VkDescriptorPool newDescriptorPool = VK_NULL_HANDLE;
                if (!allocateDescriptorSet(pipeline, descriptorSet.m_set, newDescriptorSet, newDescriptorPool))
                {
                    Logger::getInstance().error("Failed to reallocate Vulkan descriptor set for recreated pipeline.");
                    success = false;
                    return false;
                }

                VulkanDeferredDeletion deletion{};
                deletion.m_timelineValue = getLastSubmittedTimelineValue();
                VulkanDescriptorSetResource oldDescriptorSet{};
                oldDescriptorSet.m_descriptorSet = descriptorSet.m_descriptorSet;
                oldDescriptorSet.m_descriptorPool = descriptorSet.m_descriptorPool;
                deletion.m_descriptorSets.push_back(std::move(oldDescriptorSet));

                descriptorSet.m_descriptorSet = newDescriptorSet;
                descriptorSet.m_descriptorPool = newDescriptorPool;
                descriptorSet.m_buffers.clear();
                descriptorSet.m_textures.clear();
                descriptorSet.m_samplers.clear();
                setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_DESCRIPTOR_SET,
                                   (uint64_t)newDescriptorSet, "Kera Descriptor Set");

                for (const auto& reference : buffers)
                {
                    if (!updateDescriptorSet(descriptorSetHandle, reference.m_binding, reference.m_handle,
                                             reference.m_offset, reference.m_range))
                    {
                        success = false;
                        return false;
                    }
                }
                for (const auto& reference : textures)
                {
                    if (!updateDescriptorSet(descriptorSetHandle, reference.m_binding, reference.m_handle))
                    {
                        success = false;
                        return false;
                    }
                }
                for (const auto& reference : samplers)
                {
                    if (!updateDescriptorSet(descriptorSetHandle, reference.m_binding, reference.m_handle))
                    {
                        success = false;
                        return false;
                    }
                }

                queueDeferredDeletion(std::move(deletion));
                return true;
            });
        return success;
    }

    void VulkanRenderer::queueDeferredDeletion(VulkanDeferredDeletion deletion)
    {
        if (deletion.m_timelineValue == 0)
        {
            for (VulkanDescriptorSetResource& descriptorSet : deletion.m_descriptorSets)
            {
                if (m_device && descriptorSet.m_descriptorSet != VK_NULL_HANDLE &&
                    descriptorSet.m_descriptorPool != VK_NULL_HANDLE)
                {
                    vkFreeDescriptorSets(m_device->getVulkanDevice(), descriptorSet.m_descriptorPool, 1,
                                         &descriptorSet.m_descriptorSet);
                    descriptorSet.m_descriptorSet = VK_NULL_HANDLE;
                }
            }
            for (Buffer& buffer : deletion.m_buffers)
            {
                releaseStagingBuffer(std::move(buffer));
            }
            return;
        }
        m_deferredDeletions.push_back(std::move(deletion));
    }

    void VulkanRenderer::collectDeferredDeletions()
    {
        if (!m_device || m_frameTimelineSemaphore == VK_NULL_HANDLE || m_deferredDeletions.empty())
        {
            return;
        }

        uint64_t completedTimelineValue = 0;
        if (vkGetSemaphoreCounterValue(m_device->getVulkanDevice(), m_frameTimelineSemaphore,
                                       &completedTimelineValue) != VK_SUCCESS)
        {
            return;
        }

        auto deletionIt = m_deferredDeletions.begin();
        while (deletionIt != m_deferredDeletions.end())
        {
            if (deletionIt->m_timelineValue <= completedTimelineValue)
            {
                for (VkCommandBuffer commandBuffer : deletionIt->m_commandBuffers)
                {
                    if (commandBuffer != VK_NULL_HANDLE)
                    {
                        vkFreeCommandBuffers(m_device->getVulkanDevice(), m_device->getCommandPool(), 1,
                                             &commandBuffer);
                    }
                }
                for (VulkanDescriptorSetResource& descriptorSet : deletionIt->m_descriptorSets)
                {
                    if (descriptorSet.m_descriptorSet != VK_NULL_HANDLE &&
                        descriptorSet.m_descriptorPool != VK_NULL_HANDLE)
                    {
                        vkFreeDescriptorSets(m_device->getVulkanDevice(), descriptorSet.m_descriptorPool, 1,
                                             &descriptorSet.m_descriptorSet);
                        descriptorSet.m_descriptorSet = VK_NULL_HANDLE;
                    }
                }
                for (Buffer& buffer : deletionIt->m_buffers)
                {
                    releaseStagingBuffer(std::move(buffer));
                }
                deletionIt = m_deferredDeletions.erase(deletionIt);
            }
            else
            {
                ++deletionIt;
            }
        }
    }

    void VulkanRenderer::flushDeferredDeletions()
    {
        if (!m_device)
        {
            m_deferredDeletions.clear();
            return;
        }

        for (VulkanDeferredDeletion& deletion : m_deferredDeletions)
        {
            for (VkCommandBuffer commandBuffer : deletion.m_commandBuffers)
            {
                if (commandBuffer != VK_NULL_HANDLE)
                {
                    vkFreeCommandBuffers(m_device->getVulkanDevice(), m_device->getCommandPool(), 1, &commandBuffer);
                }
            }
            for (VulkanDescriptorSetResource& descriptorSet : deletion.m_descriptorSets)
            {
                if (descriptorSet.m_descriptorSet != VK_NULL_HANDLE && descriptorSet.m_descriptorPool != VK_NULL_HANDLE)
                {
                    vkFreeDescriptorSets(m_device->getVulkanDevice(), descriptorSet.m_descriptorPool, 1,
                                         &descriptorSet.m_descriptorSet);
                    descriptorSet.m_descriptorSet = VK_NULL_HANDLE;
                }
            }
        }
        m_deferredDeletions.clear();
        m_uploadContext.m_availableStagingBuffers.clear();

    }

    void VulkanRenderer::transitionTextureLayout(VkCommandBuffer commandBuffer, VulkanTextureResource& texture,
                                                 VkImageLayout newLayout)
    {
        if (commandBuffer == VK_NULL_HANDLE || texture.m_image == VK_NULL_HANDLE || texture.m_currentLayout == newLayout)
        {
            return;
        }

        VkImageMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask = stageMask2ForLayout(texture.m_currentLayout);
        barrier.srcAccessMask = accessMask2ForLayout(texture.m_currentLayout);
        barrier.dstStageMask = stageMask2ForLayout(newLayout);
        barrier.dstAccessMask = accessMask2ForLayout(newLayout);
        barrier.oldLayout = texture.m_currentLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = texture.m_image;
        barrier.subresourceRange.aspectMask = texture.m_aspectMask;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkDependencyInfo dependencyInfo{};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);

        texture.m_currentLayout = newLayout;
    }

    bool VulkanRenderer::copyBufferToTexture(Buffer& stagingBuffer, VulkanTextureResource& texture)
    {
        VkCommandBufferAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = m_device->getCommandPool();
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        if (vkAllocateCommandBuffers(m_device->getVulkanDevice(), &allocateInfo, &commandBuffer) != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to allocate Vulkan texture upload command buffer.");
            return false;
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
        {
            vkFreeCommandBuffers(m_device->getVulkanDevice(), m_device->getCommandPool(), 1, &commandBuffer);
            Logger::getInstance().error("Failed to begin Vulkan texture upload command buffer.");
            return false;
        }

        beginDebugLabel(m_device->getVulkanDevice(), commandBuffer, "Kera Texture Upload", 0.9f, 0.6f, 0.2f);
        transitionTextureLayout(commandBuffer, texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion{};
        copyRegion.imageSubresource.aspectMask = texture.m_aspectMask;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = {texture.m_extent.width, texture.m_extent.height, 1};
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.getVulkanBuffer(), texture.m_image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        transitionTextureLayout(commandBuffer, texture, texture.m_descriptorLayout);
        endDebugLabel(m_device->getVulkanDevice(), commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        {
            vkFreeCommandBuffers(m_device->getVulkanDevice(), m_device->getCommandPool(), 1, &commandBuffer);
            Logger::getInstance().error("Failed to end Vulkan texture upload command buffer.");
            return false;
        }

        uint64_t uploadTimelineValue = 0;
        if (!submitImmediateCommandBuffer(commandBuffer, uploadTimelineValue))
        {
            vkFreeCommandBuffers(m_device->getVulkanDevice(), m_device->getCommandPool(), 1, &commandBuffer);
            Logger::getInstance().error("Failed to submit Vulkan texture upload command buffer.");
            return false;
        }

        VulkanDeferredDeletion deletion{};
        deletion.m_timelineValue = uploadTimelineValue;
        deletion.m_buffers.push_back(std::move(stagingBuffer));
        deletion.m_commandBuffers.push_back(commandBuffer);
        queueDeferredDeletion(std::move(deletion));
        return true;
    }

    void VulkanRenderer::clearCompletedFrameResourceUse(uint32_t syncIndex)
    {
        if (syncIndex >= m_frameSyncResources.size())
        {
            return;
        }

        VulkanFrameResourceUse& resourceUse = m_frameSyncResources[syncIndex].m_resourceUse;
        resourceUse.m_buffers.clear();
        resourceUse.m_textures.clear();
        resourceUse.m_samplers.clear();
        resourceUse.m_descriptorSets.clear();
    }

    void VulkanRenderer::recordDescriptorSetUse(uint32_t syncIndex, DescriptorSetHandle descriptorSetHandle,
                                                const VulkanDescriptorSetResource& descriptorSet)
    {
        if (syncIndex >= m_frameSyncResources.size())
        {
            return;
        }

        auto addDescriptorUse = [](auto& handles, auto handle)
        {
            for (const auto& existing : handles)
            {
                if (existing == handle)
                {
                    return;
                }
            }
            handles.push_back(handle);
        };

        VulkanFrameResourceUse& resourceUse = m_frameSyncResources[syncIndex].m_resourceUse;
        addDescriptorUse(resourceUse.m_descriptorSets, descriptorSetHandle);
        for (const auto& reference : descriptorSet.m_buffers)
        {
            addDescriptorUse(resourceUse.m_buffers, reference.m_handle);
        }
        for (const auto& reference : descriptorSet.m_textures)
        {
            addDescriptorUse(resourceUse.m_textures, reference.m_handle);
        }
        for (const auto& reference : descriptorSet.m_samplers)
        {
            addDescriptorUse(resourceUse.m_samplers, reference.m_handle);
        }
    }

    bool VulkanRenderer::frameResourceUses(BufferHandle buffer) const
    {
        if (!buffer.isValid())
        {
            return false;
        }
        for (const VulkanFrameSyncResource& frameSync : m_frameSyncResources)
        {
            for (const BufferHandle& usedBuffer : frameSync.m_resourceUse.m_buffers)
            {
                if (usedBuffer == buffer)
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool VulkanRenderer::frameResourceUses(TextureHandle texture) const
    {
        if (!texture.isValid())
        {
            return false;
        }
        for (const VulkanFrameSyncResource& frameSync : m_frameSyncResources)
        {
            for (const TextureHandle& usedTexture : frameSync.m_resourceUse.m_textures)
            {
                if (usedTexture == texture)
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool VulkanRenderer::frameResourceUses(SamplerHandle sampler) const
    {
        if (!sampler.isValid())
        {
            return false;
        }
        for (const VulkanFrameSyncResource& frameSync : m_frameSyncResources)
        {
            for (const SamplerHandle& usedSampler : frameSync.m_resourceUse.m_samplers)
            {
                if (usedSampler == sampler)
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool VulkanRenderer::frameResourceUses(DescriptorSetHandle descriptorSet) const
    {
        if (!descriptorSet.isValid())
        {
            return false;
        }
        for (const VulkanFrameSyncResource& frameSync : m_frameSyncResources)
        {
            for (const DescriptorSetHandle& usedDescriptorSet : frameSync.m_resourceUse.m_descriptorSets)
            {
                if (usedDescriptorSet == descriptorSet)
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool VulkanRenderer::descriptorSetsReference(BufferHandle buffer)
    {
        if (!buffer.isValid())
        {
            return false;
        }

        bool referenced = false;
        m_descriptorSets.forEach(
            [&referenced, buffer](DescriptorSetHandle, VulkanDescriptorSetResource& descriptorSet)
            {
                for (const auto& reference : descriptorSet.m_buffers)
                {
                    if (reference.m_handle == buffer)
                    {
                        referenced = true;
                        return false;
                    }
                }
                return true;
            });
        return referenced;
    }

    bool VulkanRenderer::descriptorSetsReference(TextureHandle texture)
    {
        if (!texture.isValid())
        {
            return false;
        }

        bool referenced = false;
        m_descriptorSets.forEach(
            [&referenced, texture](DescriptorSetHandle, VulkanDescriptorSetResource& descriptorSet)
            {
                for (const auto& reference : descriptorSet.m_textures)
                {
                    if (reference.m_handle == texture)
                    {
                        referenced = true;
                        return false;
                    }
                }
                return true;
            });
        return referenced;
    }

    bool VulkanRenderer::descriptorSetsReference(SamplerHandle sampler)
    {
        if (!sampler.isValid())
        {
            return false;
        }

        bool referenced = false;
        m_descriptorSets.forEach(
            [&referenced, sampler](DescriptorSetHandle, VulkanDescriptorSetResource& descriptorSet)
            {
                for (const auto& reference : descriptorSet.m_samplers)
                {
                    if (reference.m_handle == sampler)
                    {
                        referenced = true;
                        return false;
                    }
                }
                return true;
            });
        return referenced;
    }

    bool VulkanRenderer::renderTargetsReference(TextureHandle texture)
    {
        if (!texture.isValid())
        {
            return false;
        }

        bool referenced = false;
        m_renderTargets.forEach(
            [&referenced, texture](RenderTargetHandle, VulkanRenderTargetResource& renderTarget)
            {
                if (renderTarget.m_colorTexture == texture || renderTarget.m_depthTexture == texture)
                {
                    referenced = true;
                    return false;
                }
                return true;
            });
        return referenced;
    }

    const DescriptorSetLayoutDesc* VulkanRenderer::resolveDescriptorSetLayout(
        const VulkanGraphicsPipelineResource& pipeline, uint32_t set) const
    {
        for (const DescriptorSetLayoutDesc& layout : pipeline.m_desc.descriptorSets)
        {
            if (layout.set == set)
            {
                return &layout;
            }
        }
        return nullptr;
    }

    bool VulkanRenderer::validateDescriptorBinding(const VulkanDescriptorSetResource& descriptorSet, uint32_t binding,
                                                   DescriptorType type) const
    {
        return descriptorBindingAccepts(descriptorSet.m_layout, binding, type);
    }

    bool VulkanRenderer::resolvePipelineRenderingFormats(RenderTargetHandle renderTarget, VkFormat& colorFormat,
                                                         VkFormat& depthFormat) const
    {
        if (!renderTarget.isValid())
        {
            if (!m_swapchain)
            {
                return false;
            }
            colorFormat = m_swapchain->getImageFormat();
            depthFormat = VK_FORMAT_UNDEFINED;
            return true;
        }

        const VulkanRenderTargetResource* resource = m_renderTargets.get(renderTarget);
        if (!resource)
        {
            return false;
        }

        const VulkanTextureResource* colorTexture = m_textures.get(resource->m_colorTexture);
        if (!colorTexture)
        {
            return false;
        }

        colorFormat = colorTexture->m_format;
        depthFormat = VK_FORMAT_UNDEFINED;
        if (resource->m_depthTexture.isValid())
        {
            const VulkanTextureResource* depthTexture = m_textures.get(resource->m_depthTexture);
            if (!depthTexture)
            {
                return false;
            }
            depthFormat = depthTexture->m_format;
        }
        return true;
    }

    void VulkanRenderer::transitionSwapchainImageLayout(VkCommandBuffer commandBuffer, uint32_t imageIndex,
                                                        VkImageLayout newLayout)
    {
        if (!m_swapchain || commandBuffer == VK_NULL_HANDLE || imageIndex >= m_swapchain->getImages().size() ||
            imageIndex >= m_swapchainImageLayouts.size() || m_swapchainImageLayouts[imageIndex] == newLayout)
        {
            return;
        }

        VkImageMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask = stageMask2ForLayout(m_swapchainImageLayouts[imageIndex]);
        barrier.srcAccessMask = accessMask2ForLayout(m_swapchainImageLayouts[imageIndex]);
        barrier.dstStageMask = stageMask2ForLayout(newLayout);
        barrier.dstAccessMask = accessMask2ForLayout(newLayout);
        barrier.oldLayout = m_swapchainImageLayouts[imageIndex];
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_swapchain->getImages()[imageIndex];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkDependencyInfo dependencyInfo{};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);

        m_swapchainImageLayouts[imageIndex] = newLayout;
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

        flushDeferredDeletions();

        VkDevice vkDevice = m_device->getVulkanDevice();
        for (VulkanFrameSyncResource& frameSync : m_frameSyncResources)
        {
            if (frameSync.m_renderFinishedSemaphore != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(vkDevice, frameSync.m_renderFinishedSemaphore, nullptr);
                frameSync.m_renderFinishedSemaphore = VK_NULL_HANDLE;
            }
            if (frameSync.m_imageAvailableSemaphore != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(vkDevice, frameSync.m_imageAvailableSemaphore, nullptr);
                frameSync.m_imageAvailableSemaphore = VK_NULL_HANDLE;
            }
            frameSync.m_timelineValue = 0;
        }
        if (m_frameTimelineSemaphore != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(vkDevice, m_frameTimelineSemaphore, nullptr);
            m_frameTimelineSemaphore = VK_NULL_HANDLE;
        }
        m_frameSyncResources.clear();
        m_imagesInFlight.clear();
        m_activeFrameHandles.clear();
        m_nextFrameTimelineValue = 1;
        m_currentFrameSyncIndex = 0;
    }

    bool VulkanRenderer::createDescriptorPool()
    {
        destroyDescriptorPool();
        return createDescriptorPoolBlock();
    }

    void VulkanRenderer::destroyDescriptorPool()
    {
        if (!m_device)
        {
            return;
        }

        for (VulkanDescriptorPoolResource& pool : m_descriptorPools)
        {
            if (pool.m_pool != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorPool(m_device->getVulkanDevice(), pool.m_pool, nullptr);
            }
        }
        m_descriptorPools.clear();
    }

    bool VulkanRenderer::createDescriptorPoolBlock()
    {
        if (!m_device)
        {
            return false;
        }

        constexpr uint32_t poolIndexScale = 1;
        std::array<VkDescriptorPoolSize, 3> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 128 * poolIndexScale;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        poolSizes[1].descriptorCount = 64 * poolIndexScale;
        poolSizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
        poolSizes[2].descriptorCount = 64 * poolIndexScale;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 128 * poolIndexScale;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();

        VulkanDescriptorPoolResource pool{};
        if (vkCreateDescriptorPool(m_device->getVulkanDevice(), &poolInfo, nullptr, &pool.m_pool) != VK_SUCCESS)
        {
            return false;
        }

        setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                           (uint64_t)pool.m_pool,
                           "Kera Descriptor Pool " + std::to_string(m_descriptorPools.size()));
        m_descriptorPools.push_back(pool);
        return true;
    }

    bool VulkanRenderer::createSyncObjects()
    {
        destroySyncObjects();
        if (!m_swapchain)
        {
            return false;
        }

        const uint32_t frameSyncCount = m_commandBuffers.size() < kMaxFramesInFlight
                                            ? static_cast<uint32_t>(m_commandBuffers.size())
                                            : kMaxFramesInFlight;
        if (frameSyncCount == 0)
        {
            return false;
        }

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkSemaphoreTypeCreateInfo timelineSemaphoreTypeInfo{};
        timelineSemaphoreTypeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timelineSemaphoreTypeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timelineSemaphoreTypeInfo.initialValue = 0;

        VkSemaphoreCreateInfo timelineSemaphoreInfo{};
        timelineSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        timelineSemaphoreInfo.pNext = &timelineSemaphoreTypeInfo;

        VkDevice vkDevice = m_device->getVulkanDevice();
        if (vkCreateSemaphore(vkDevice, &timelineSemaphoreInfo, nullptr, &m_frameTimelineSemaphore) != VK_SUCCESS)
        {
            destroySyncObjects();
            return false;
        }

        m_frameSyncResources.resize(frameSyncCount);
        m_imagesInFlight.assign(m_swapchain->getImageCount(), 0);
        m_activeFrameHandles.assign(frameSyncCount, {});

        for (VulkanFrameSyncResource& frameSync : m_frameSyncResources)
        {
            frameSync.m_timelineValue = 0;
            if (vkCreateSemaphore(vkDevice, &semaphoreInfo, nullptr, &frameSync.m_imageAvailableSemaphore) !=
                VK_SUCCESS)
            {
                destroySyncObjects();
                return false;
            }

            if (vkCreateSemaphore(vkDevice, &semaphoreInfo, nullptr, &frameSync.m_renderFinishedSemaphore) !=
                VK_SUCCESS)
            {
                destroySyncObjects();
                return false;
            }
        }

        m_nextFrameTimelineValue = 1;
        m_currentFrameSyncIndex = 0;
        return true;
    }
}  // namespace kera
