// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/backend/vulkan/vulkan_renderer.h"

#include "kera/core/window.h"
#include "kera/renderer/backend/vulkan/layout_utils.h"
#include "kera/renderer/command_buffer.h"
#include "kera/renderer/descriptor_contracts.h"
#include "kera/renderer/device.h"
#include "kera/renderer/instance.h"
#include "kera/renderer/physical_device.h"
#include "kera/renderer/reflection_contracts.h"
#include "kera/renderer/slang_compiler.h"
#include "kera/renderer/surface.h"
#include "kera/renderer/swapchain.h"
#include "kera/utilities/logger.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <span>
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

        std::size_t alignUp(std::size_t value, std::size_t alignement)
        {
            if (alignement <= 1)
            {
                return value;
            }
            return ((value + alignement - 1) / alignement) * alignement;
        }

        bool surfaceWouldUseZeroExtent(const SwapChainSupportDetails& support, uint32_t width, uint32_t height)
        {
            if (support.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
            {
                return support.capabilities.currentExtent.width == 0 || support.capabilities.currentExtent.height == 0;
            }
            return width == 0 || height == 0;
        }

        EShaderType toShaderType(EShaderStage stage)
        {
            switch (stage)
            {
                case EShaderStage::VERTEX:
                    return EShaderType::VERTEX;
                case EShaderStage::FRAGMENT:
                    return EShaderType::FRAGMENT;
                case EShaderStage::COMPUTE:
                    return EShaderType::COMPUTE;
                default:
                    return EShaderType::VERTEX;
            }
        }

        EBufferUsage toEBufferUsage(EBufferUsageKind usage)
        {
            switch (usage)
            {
                case EBufferUsageKind::VERTEX:
                    return EBufferUsage::VERTEX;
                case EBufferUsageKind::INDEX:
                    return EBufferUsage::INDEX;
                case EBufferUsageKind::UNIFORM:
                    return EBufferUsage::UNIFORM;
                case EBufferUsageKind::STORAGE:
                    return EBufferUsage::STORAGE;
                default:
                    return EBufferUsage::VERTEX;
            }
        }

        VkMemoryPropertyFlags toMemoryFlags(EMemoryAccess access)
        {
            switch (access)
            {
                case EMemoryAccess::GPU_ONLY:
                    return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                case EMemoryAccess::CPU_WRITE:
                    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                default:
                    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            }
        }

        VkFormat toVkTextureFormat(ETextureFormat format)
        {
            switch (format)
            {
                case ETextureFormat::DEPTH32:
                    return VK_FORMAT_D32_SFLOAT;
                case ETextureFormat::RGB_A8_SRGB:
                    return VK_FORMAT_R8G8B8A8_SRGB;
                case ETextureFormat::B10_G11_R11_UFLOAT:
                    return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
                case ETextureFormat::RGBA8:
                    return VK_FORMAT_R8G8B8A8_UNORM;
                default:
                    return VK_FORMAT_UNDEFINED;
            }
        }

        VkFilter toVkFilter(ESamplerFilter filter)
        {
            switch (filter)
            {
                case ESamplerFilter::NEAREST:
                    return VK_FILTER_NEAREST;
                case ESamplerFilter::LINEAR:
                default:
                    return VK_FILTER_LINEAR;
            }
        }

        VkSamplerMipmapMode toVkMipFilter(ESamplerMipFilter filter)
        {
            switch (filter)
            {
                case ESamplerMipFilter::NEAREST:
                    return VK_SAMPLER_MIPMAP_MODE_NEAREST;
                case ESamplerMipFilter::LINEAR:
                default:
                    return VK_SAMPLER_MIPMAP_MODE_LINEAR;
            }
        }

        VkSamplerAddressMode toVkAddressMode(ESamplerAddressMode mode)
        {
            switch (mode)
            {
                case ESamplerAddressMode::REPEAT:
                    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
                case ESamplerAddressMode::MIRRORED_REPEAT:
                    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
                case ESamplerAddressMode::CLAMP_TO_EDGE:
                default:
                    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            }
        }

        bool findMemoryType(VkPhysicalDevice physical_device, uint32_t type_filter, VkMemoryPropertyFlags properties,
                            uint32_t& out_memory_type)
        {
            VkPhysicalDeviceMemoryProperties memory_properties{};
            vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

            for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i)
            {
                if ((type_filter & (1u << i)) &&
                    (memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
                {
                    out_memory_type = i;
                    return true;
                }
            }

            return false;
        }

        VkPipelineStageFlags2 signalStageForTimelineSubmit(VkPipelineStageFlags2 producer_stage)
        {
            return producer_stage == 0 ? VK_PIPELINE_STAGE_2_NONE : producer_stage;
        }

        std::string swapchainFormatName(VkFormat format)
        {
            switch (format)
            {
                case VK_FORMAT_B8G8R8A8_UNORM:
                    return "VK_FORMAT_B8G8R8A8_UNORM";
                case VK_FORMAT_B8G8R8A8_SRGB:
                    return "VK_FORMAT_B8G8R8A8_SRGB";
                case VK_FORMAT_R8G8B8A8_UNORM:
                    return "VK_FORMAT_R8G8B8A8_UNORM";
                case VK_FORMAT_R8G8B8A8_SRGB:
                    return "VK_FORMAT_R8G8B8A8_SRGB";
                default:
                    return "VK_FORMAT_" + std::to_string(static_cast<uint32_t>(format));
            }
        }

        VkPipelineStageFlags2 renderCompleteStageMask()
        {
            return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        }

        void setDebugObjectName(VkDevice device, VkObjectType object_type, uint64_t object_handle,
                                const std::string& name)
        {
            if (device == VK_NULL_HANDLE || object_handle == 0 || name.empty())
            {
                return;
            }

            auto set_name = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
                vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
            if (!set_name)
            {
                return;
            }

            VkDebugUtilsObjectNameInfoEXT name_info{};
            name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            name_info.objectType = object_type;
            name_info.objectHandle = object_handle;
            name_info.pObjectName = name.c_str();
            set_name(device, &name_info);
        }

        std::string debugNameOrDefault(const std::string& debug_name, const std::string& fallback)
        {
            return debug_name.empty() ? fallback : debug_name;
        }

        void logValidationReport(const RendererValidationReport& report)
        {
            for (const RendererValidationIssue& issue : report.issues)
            {
                Logger::getInstance().error(std::string("[") + rendererValidationCategoryName(issue.category) + "] " +
                                            issue.message);
            }
        }

        void logVulkanCapabilities(const PhysicalDevice& physical_device, const SwapChain& swapchain)
        {
            const VkPhysicalDeviceProperties& properties = physical_device.getProperties();
            Logger::getInstance().info(
                "Vulkan capabilities: device '" + physical_device.getDeviceName() + "', API " +
                std::to_string(VK_API_VERSION_MAJOR(properties.apiVersion)) + "." +
                std::to_string(VK_API_VERSION_MINOR(properties.apiVersion)) + "." +
                std::to_string(VK_API_VERSION_PATCH(properties.apiVersion)) +
                ", required features: synchronization2, dynamic rendering, timeline semaphore, swapchain " +
                std::to_string(swapchain.getImageCount()) + " images " +
                swapchainFormatName(swapchain.getImageFormat()) + ", max frames in flight " +
                std::to_string(kMaxFramesInFlight) + ".");
        }

        void beginDebugLabel(VkDevice device, VkCommandBuffer command_buffer, const char* name, float r, float g,
                             float b)
        {
            if (device == VK_NULL_HANDLE || command_buffer == VK_NULL_HANDLE || !name)
            {
                return;
            }

            auto begin_label = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
                vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT"));
            if (!begin_label)
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
            begin_label(command_buffer, &label);
        }

        void endDebugLabel(VkDevice device, VkCommandBuffer command_buffer)
        {
            if (device == VK_NULL_HANDLE || command_buffer == VK_NULL_HANDLE)
            {
                return;
            }

            auto end_label = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
                vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT"));
            if (end_label)
            {
                end_label(command_buffer);
            }
        }

        EShaderStage shaderTypeToStage(EShaderType type)
        {
            switch (type)
            {
                case EShaderType::VERTEX:
                    return EShaderStage::VERTEX;
                case EShaderType::FRAGMENT:
                    return EShaderStage::FRAGMENT;
                case EShaderType::COMPUTE:
                    return EShaderStage::COMPUTE;
                default:
                    return EShaderStage::VERTEX;
            }
        }

        const Shader* findShader(const VulkanShaderProgramResource& program, EShaderStage stage)
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
                findShader(program, EShaderStage::VERTEX),
                findShader(program, EShaderStage::FRAGMENT),
            };
        }

        bool descriptorTypeFromReflectionKind(ESlangReflectionBindingKind kind, EDescriptorType& out_type)
        {
            switch (kind)
            {
                case ESlangReflectionBindingKind::PARAMETER_BLOCK:
                case ESlangReflectionBindingKind::CONSTANT_BUFFER:
                    out_type = EDescriptorType::UNIFORM_BUFFER;
                    return true;
                case ESlangReflectionBindingKind::RESOURCE:
                    out_type = EDescriptorType::SAMPLED_IMAGE;
                    return true;
                case ESlangReflectionBindingKind::SAMPLER_STATE:
                    out_type = EDescriptorType::SAMPLER;
                    return true;
                default:
                    return false;
            }
        }

        bool appendReflectedDescriptorBinding(DescriptorSetLayoutDesc& layout, const SlangReflectionBinding& binding)
        {
            EDescriptorType descriptor_type = EDescriptorType::UNIFORM_BUFFER;
            if (!descriptorTypeFromReflectionKind(binding.kind, descriptor_type))
            {
                Logger::getInstance().error("Shader reflection descriptor binding '" + binding.name +
                                            "' has an unsupported descriptor kind.");
                return false;
            }

            const auto duplicate = std::find_if(layout.bindings.begin(), layout.bindings.end(),
                                                [&binding](const DescriptorBindingDesc& existing)
                                                { return existing.binding == binding.binding; });
            if (duplicate != layout.bindings.end())
            {
                Logger::getInstance().error("Shader reflection descriptor binding '" + binding.name +
                                            "' duplicates an existing descriptor binding.");
                return false;
            }

            layout.bindings.push_back({
                .name = binding.name,
                .binding = binding.binding,
                .type = descriptor_type,
                .stage = EShaderStage::ALL_GRAPHICS,
                .count = binding.count,
            });
            return true;
        }

        bool deriveDescriptorSetLayoutsFromReflection(const SlangReflectionMetadata& reflection,
                                                      std::vector<DescriptorSetLayoutDesc>& out_layouts)
        {
            out_layouts.clear();
            uint32_t max_set = 0;
            bool has_bindings = false;
            for (const SlangReflectionBinding& binding : reflection.bindings)
            {
                EDescriptorType ignored_type = EDescriptorType::UNIFORM_BUFFER;
                if (descriptorTypeFromReflectionKind(binding.kind, ignored_type))
                {
                    max_set = std::max(max_set, binding.space);
                    has_bindings = true;
                }
            }

            if (!has_bindings)
            {
                return true;
            }

            out_layouts.reserve(max_set + 1);
            for (uint32_t set = 0; set <= max_set; ++set)
            {
                out_layouts.push_back({.set = set, .bindings = {}});
            }

            for (const SlangReflectionBinding& binding : reflection.bindings)
            {
                EDescriptorType ignored_type = EDescriptorType::UNIFORM_BUFFER;
                if (!descriptorTypeFromReflectionKind(binding.kind, ignored_type))
                {
                    continue;
                }

                if (!appendReflectedDescriptorBinding(out_layouts[binding.space], binding))
                {
                    return false;
                }
            }

            for (DescriptorSetLayoutDesc& layout : out_layouts)
            {
                std::sort(layout.bindings.begin(), layout.bindings.end(),
                          [](const DescriptorBindingDesc& lhs, const DescriptorBindingDesc& rhs)
                          { return lhs.binding < rhs.binding; });
            }
            return true;
        }

        void mergeSlangReflection(SlangReflectionMetadata& destination, const SlangReflectionMetadata& source)
        {
            for (const SlangReflectionBinding& binding : source.bindings)
            {
                const auto duplicate = std::find_if(destination.bindings.begin(), destination.bindings.end(),
                                                    [&binding](const SlangReflectionBinding& existing)
                                                    {
                                                        return existing.name == binding.name &&
                                                               existing.binding == binding.binding &&
                                                               existing.space == binding.space;
                                                    });
                if (duplicate == destination.bindings.end())
                {
                    destination.bindings.push_back(binding);
                }
            }

            for (const SlangReflectionEntryPoint& entry_point : source.entry_points)
            {
                const auto duplicate =
                    std::find_if(destination.entry_points.begin(), destination.entry_points.end(),
                                 [&entry_point](const SlangReflectionEntryPoint& existing)
                                 { return existing.name == entry_point.name && existing.stage == entry_point.stage; });
                if (duplicate == destination.entry_points.end())
                {
                    destination.entry_points.push_back(entry_point);
                }
            }
        }

        bool initializeVulkanShaderFromDesc(const Device& device, const ShaderModuleDesc& desc, Shader& shader,
                                            SlangReflectionMetadata* out_reflection)
        {
            const EShaderType shader_type = toShaderType(desc.stage);
            bool initialized = false;

            switch (desc.source)
            {
                case EShaderSourceKind::SLANG_FILE:
                {
                    std::vector<uint32_t> spirv_code;
                    SlangReflectionMetadata reflection;
                    SlangCompileRequest request{
                        .shader_path = desc.path,
                        .entry_point = desc.entry_point,
                        .shader_type = shader_type,
                        .search_paths = desc.search_paths,
                    };

                    std::string diagnostics;
                    if (!SlangCompiler::compileToSpirvAndReflect(request, spirv_code, reflection, &diagnostics))
                    {
                        Logger::getInstance().error("Failed to compile Slang shader: " + desc.path +
                                                    " entry=" + desc.entry_point + "\n" + diagnostics);
                        return false;
                    }

                    initialized = shader.initialize(device, shader_type, spirv_code);
                    if (initialized && out_reflection)
                    {
                        mergeSlangReflection(*out_reflection, reflection);
                    }
                    break;
                }
                case EShaderSourceKind::SPIRV_FILE:
                    initialized = shader.initializeFromFile(device, shader_type, desc.path);
                    break;
                case EShaderSourceKind::SPIRV_BINARY:
                    if (desc.spirv_code.empty())
                    {
                        Logger::getInstance().error(
                            "ShaderModuleDesc.spirvCode must not be empty for SpirvBinary "
                            "source.");
                        return false;
                    }
                    initialized = shader.initialize(device, shader_type, desc.spirv_code);
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
        , m_frame_timeline_semaphore(VK_NULL_HANDLE)
        , m_next_frame_timeline_value(1)
        , m_pipeline_cache(VK_NULL_HANDLE)
        , m_ui_initialized(false)
        , m_swapchain_recreate_requested(false)
        , m_resize_pending(false)
    {
    }

    VulkanRenderer::~VulkanRenderer()
    {
        shutdown();
    }

    bool VulkanRenderer::initialize(Window& window)
    {
        m_window = &window;
        return initialize(window.getSDLWindow(), static_cast<uint32_t>(window.getWidth()),
                          static_cast<uint32_t>(window.getHeight()));
    }

    bool VulkanRenderer::initialize(SDL_Window* window, uint32_t width, uint32_t height)
    {
        m_sdl_window = window;
        m_window_extent = {width, height};
        m_instance = std::make_shared<Instance>();
        if (!m_instance->initialize("Kera Sample", VK_MAKE_VERSION(0, 1, 0), true))
        {
            Logger::getInstance().error("Failed to create Vulkan instance");
            return false;
        }
        Logger::getInstance().info(m_instance->isDebugMessengerActive() ? "Vulkan debug messenger is active."
                                                                        : "Vulkan debug messenger is not active.");

        m_surface = std::make_shared<Surface>();
        if (!m_surface->create(m_instance->getVulkanInstance(), window))
        {
            Logger::getInstance().error("Failed to create Vulkan surface");
            return false;
        }

        m_physical_device = std::make_shared<PhysicalDevice>();
        if (!m_physical_device->initialize(m_instance->getVulkanInstance(), m_surface->getVulkanSurface()))
        {
            Logger::getInstance().error("Failed to select physical device");
            return false;
        }

        m_device = std::make_shared<Device>();
        if (!m_device->initialize(*m_physical_device))
        {
            Logger::getInstance().error("Failed to create logical device");
            return false;
        }

        VkPipelineCacheCreateInfo pipeline_cache_info{};
        pipeline_cache_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        if (vkCreatePipelineCache(m_device->getVulkanDevice(), &pipeline_cache_info, nullptr, &m_pipeline_cache) !=
            VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to create Vulkan pipeline cache");
            return false;
        }
        setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_PIPELINE_CACHE, (uint64_t)m_pipeline_cache,
                           "Kera Pipeline Cache");

        if (!createDescriptorPool())
        {
            Logger::getInstance().error("Failed to create Vulkan descriptor pool");
            return false;
        }

        if (!recreateSwapchainResources(width, height))
        {
            Logger::getInstance().error("Failed to create Vulkan swapchain resources");
            return false;
        }

        if (!createSyncObjects())
        {
            Logger::getInstance().error("Failed to create Vulkan frame synchronization objects");
            return false;
        }

        logVulkanCapabilities(*m_physical_device, *m_swapchain);
        return true;
    }

    void VulkanRenderer::shutdown()
    {
        m_upload_context.batch_active = false;
        discardPendingUploads();
        waitForDeviceIdle();
        flushDeferredDeletions();
        shutdownUi();

        m_frames.clear();
        m_descriptor_sets.clear();
        m_graphics_pipelines.clear();
        m_render_targets.clear();
        m_samplers.clear();
        m_textures.clear();
        m_buffers.clear();
        m_shader_programs.clear();
        m_shader_modules.clear();

        destroySyncObjects();
        destroyDescriptorPool();
        if (m_device && m_pipeline_cache != VK_NULL_HANDLE)
        {
            vkDestroyPipelineCache(m_device->getVulkanDevice(), m_pipeline_cache, nullptr);
            m_pipeline_cache = VK_NULL_HANDLE;
        }

        for (std::unique_ptr<CommandBuffer>& command_buffer : m_command_buffers)
        {
            if (command_buffer)
            {
                command_buffer->shutdown();
            }
        }
        m_command_buffers.clear();
        m_swapchain_image_layouts.clear();

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
        m_physical_device.reset();
        if (m_instance)
        {
            m_instance->shutdown();
            m_instance.reset();
        }
        m_window = nullptr;
        m_sdl_window = nullptr;
        m_window_extent = {};
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
        stats.resources.shader_modules = m_shader_modules.activeCount();
        stats.resources.shader_programs = m_shader_programs.activeCount();
        stats.resources.buffers = m_buffers.activeCount();
        stats.resources.textures = m_textures.activeCount();
        stats.resources.samplers = m_samplers.activeCount();
        stats.resources.render_targets = m_render_targets.activeCount();
        stats.resources.graphics_pipelines = m_graphics_pipelines.activeCount();
        stats.resources.descriptor_sets = m_descriptor_sets.activeCount();
        stats.resources.frames = m_frames.activeCount();
        return stats;
    }

    bool VulkanRenderer::initializeUi()
    {
#ifdef KERA_HAS_IMGUI
        if (m_ui_initialized)
        {
            return true;
        }

        if (!m_sdl_window || !m_instance || !m_device || !m_swapchain)
        {
            Logger::getInstance().warning("Cannot initialize ImGui before Vulkan renderer resources are ready.");
            return false;
        }

        const uint32_t image_count = m_swapchain->getImageCount();
        const uint32_t min_image_count = image_count < 2 ? image_count : 2;
        if (min_image_count < 2 || image_count < min_image_count)
        {
            Logger::getInstance().warning("Cannot initialize ImGui with fewer than two swapchain images.");
            return false;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        if (!ImGui_ImplSDL3_InitForVulkan(m_sdl_window))
        {
            Logger::getInstance().warning("Failed to initialize ImGui SDL3 backend.");
            ImGui::DestroyContext();
            return false;
        }

        const VkFormat ui_color_attachment_format = m_swapchain->getImageFormat();
        ImGui_ImplVulkan_InitInfo init_info{};
        init_info.ApiVersion = VK_API_VERSION_1_3;
        init_info.Instance = m_instance->getVulkanInstance();
        init_info.PhysicalDevice = m_device->getVulkanPhysicalDevice();
        init_info.Device = m_device->getVulkanDevice();
        init_info.QueueFamily = m_device->getGraphicsQueueFamilyIndex();
        init_info.Queue = m_device->getGraphicsQueue();
        init_info.DescriptorPoolSize = 256;
        init_info.MinImageCount = min_image_count;
        init_info.ImageCount = image_count;
        init_info.UseDynamicRendering = true;
        init_info.PipelineInfoMain.RenderPass = VK_NULL_HANDLE;
        init_info.PipelineInfoMain.Subpass = 0;
        init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
#ifdef IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
        init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &ui_color_attachment_format;
#endif

        if (!ImGui_ImplVulkan_Init(&init_info))
        {
            Logger::getInstance().warning("Failed to initialize ImGui Vulkan backend.");
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            return false;
        }

        m_ui_initialized = true;
        return true;
#else
        Logger::getInstance().warning("Kera was built without ImGui support.");
        return false;
#endif
    }

    void VulkanRenderer::shutdownUi()
    {
#ifdef KERA_HAS_IMGUI
        if (!m_ui_initialized)
        {
            return;
        }

        waitForDeviceIdle();
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
#endif
        m_ui_initialized = false;
    }

    void VulkanRenderer::handleUiEvent(const SDL_Event& event)
    {
#ifdef KERA_HAS_IMGUI
        if (m_ui_initialized)
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
        if (!m_ui_initialized)
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
        if (m_ui_initialized)
        {
            ImGui::Render();
        }
#endif
    }

    void VulkanRenderer::renderUi(FrameHandle frame_handle)
    {
#ifdef KERA_HAS_IMGUI
        if (!m_ui_initialized)
        {
            return;
        }

        VulkanFrameResource* frame = m_frames.get(frame_handle);
        if (!frame)
        {
            Logger::getInstance().error("Invalid frame handle passed to renderUi.");
            return;
        }
        if (!frame->m_render_pass_active)
        {
            Logger::getInstance().error("Cannot render ImGui draw data outside an active Vulkan render pass.");
            return;
        }

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frame->m_command_buffer);
#else
        (void)frameHandle;
#endif
    }

    bool VulkanRenderer::resize(Extent2D new_extent)
    {
        m_window_extent = new_extent;
        if (!m_device || !m_surface || !m_physical_device || !m_swapchain)
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return false;
        }

        if (new_extent.width == 0 || new_extent.height == 0)
        {
            m_swapchain_recreate_requested = true;
            m_resize_pending = false;
            return true;
        }

        if (hasActiveFrames())
        {
            Logger::getInstance().info("Deferring Vulkan resize until the active frame completes.");
            m_pending_resize_extent = new_extent;
            m_resize_pending = true;
            m_swapchain_recreate_requested = true;
            return true;
        }

        if (!refreshSwapchainSupport())
        {
            return false;
        }
        if (surfaceWouldUseZeroExtent(m_physical_device->getSwapChainSupport(), new_extent.width, new_extent.height))
        {
            Logger::getInstance().debug("Deferring Vulkan resize while the surface reports a zero extent.");
            m_pending_resize_extent = new_extent;
            m_resize_pending = true;
            m_swapchain_recreate_requested = true;
            return true;
        }

        const bool restore_ui = m_ui_initialized;
        if (restore_ui)
        {
            shutdownUi();
        }

        waitForDeviceIdle();

        if (!recreateSwapchainResources(new_extent.width, new_extent.height))
        {
            Logger::getInstance().error("Failed to recreate Vulkan swapchain resources.");
            return false;
        }

        if (!createSyncObjects())
        {
            Logger::getInstance().error("Failed to recreate Vulkan frame synchronization objects.");
            return false;
        }

        if (restore_ui && !initializeUi())
        {
            Logger::getInstance().warning("Failed to restore ImGui after resize.");
        }

        m_swapchain_recreate_requested = false;
        m_resize_pending = false;
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
        if (!initializeVulkanShaderFromDesc(*m_device, desc, resource.m_shader, nullptr))
        {
            return {};
        }

        ShaderModuleHandle handle = m_shader_modules.emplace(std::move(resource));
        if (VulkanShaderModuleResource* named_resource = m_shader_modules.get(handle))
        {
            setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_SHADER_MODULE,
                               (uint64_t)named_resource->m_shader.getVulkanShaderModule(),
                               debugNameOrDefault(desc.debug_name, "Kera Shader Module"));
        }
        return handle;
    }

    bool VulkanRenderer::destroyShaderModule(ShaderModuleHandle module)
    {
        waitForDeviceIdle();
        return m_shader_modules.remove(module);
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

        bool has_vertex_stage = false;
        bool has_fragment_stage = false;
        for (const ShaderModuleDesc& stage_desc : desc.stages)
        {
            if (stage_desc.stage == EShaderStage::VERTEX)
            {
                if (has_vertex_stage)
                {
                    Logger::getInstance().error("Shader program contains duplicate vertex stages.");
                    return {};
                }
                has_vertex_stage = true;
            }
            else if (stage_desc.stage == EShaderStage::FRAGMENT)
            {
                if (has_fragment_stage)
                {
                    Logger::getInstance().error("Shader program contains duplicate fragment stages.");
                    return {};
                }
                has_fragment_stage = true;
            }

            Shader shader;
            if (!initializeVulkanShaderFromDesc(*m_device, stage_desc, shader, &resource.m_reflection))
            {
                return {};
            }
            setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_SHADER_MODULE,
                               (uint64_t)shader.getVulkanShaderModule(),
                               debugNameOrDefault(stage_desc.debug_name, "Kera Shader Program Stage"));
            resource.m_shaders.push_back(std::move(shader));
        }

        return m_shader_programs.emplace(std::move(resource));
    }

    ShaderProgramHandle VulkanRenderer::createGraphicsShaderProgram(const GraphicsShaderProgramDesc& desc)
    {
        return createShaderProgram({
            .stages =
                {
                    {
                        .path = desc.path,
                        .entry_point = desc.vertex_entry_point,
                        .stage = EShaderStage::VERTEX,
                        .source = desc.source,
                        .search_paths = {},
                        .spirv_code = {},
                        .debug_name = desc.debug_name.empty() ? std::string{} : desc.debug_name + " Vertex Shader",
                    },
                    {
                        .path = desc.path,
                        .entry_point = desc.fragment_entry_point,
                        .stage = EShaderStage::FRAGMENT,
                        .source = desc.source,
                        .search_paths = {},
                        .spirv_code = {},
                        .debug_name = desc.debug_name.empty() ? std::string{} : desc.debug_name + " Fragment Shader",
                    },
                },
            .debug_name = desc.debug_name,
        });
    }

    const SlangReflectionMetadata* VulkanRenderer::getShaderProgramReflection(ShaderProgramHandle program) const
    {
        const VulkanShaderProgramResource* shader_program = m_shader_programs.get(program);
        return shader_program ? &shader_program->m_reflection : nullptr;
    }

    std::vector<SlangReflectionEntryPoint> VulkanRenderer::getShaderProgramEntryPoints(
        ShaderProgramHandle program) const
    {
        const VulkanShaderProgramResource* shader_program = m_shader_programs.get(program);
        return shader_program ? shader_program->m_reflection.entry_points : std::vector<SlangReflectionEntryPoint>{};
    }

    std::vector<SlangReflectionBinding> VulkanRenderer::getShaderProgramDescriptorBindings(
        ShaderProgramHandle program) const
    {
        const VulkanShaderProgramResource* shader_program = m_shader_programs.get(program);
        return shader_program ? shader_program->m_reflection.bindings : std::vector<SlangReflectionBinding>{};
    }

    std::vector<SlangReflectionInput> VulkanRenderer::getShaderProgramVertexInputs(ShaderProgramHandle program,
                                                                                   const std::string& entry_point) const
    {
        const VulkanShaderProgramResource* shader_program = m_shader_programs.get(program);
        if (!shader_program)
        {
            return {};
        }

        const SlangReflectionEntryPoint* reflected_entry_point = shader_program->m_reflection.findEntryPoint(entry_point);
        return reflected_entry_point ? reflected_entry_point->inputs : std::vector<SlangReflectionInput>{};
    }

    bool VulkanRenderer::destroyShaderProgram(ShaderProgramHandle program)
    {
        bool referenced_by_pipeline = false;
        m_graphics_pipelines.forEach(
            [&referenced_by_pipeline, program](GraphicsPipelineHandle, VulkanGraphicsPipelineResource& pipeline)
            {
                if (pipeline.m_program == program)
                {
                    referenced_by_pipeline = true;
                    return false;
                }
                return true;
            });
        if (referenced_by_pipeline)
        {
            Logger::getInstance().error("Cannot destroy a Vulkan shader program referenced by a graphics pipeline.");
            return false;
        }

        waitForDeviceIdle();
        return m_shader_programs.remove(program);
    }

    BufferHandle VulkanRenderer::createBuffer(const BufferDesc& desc)
    {
        if (!m_device)
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return {};
        }

        VulkanBufferResource resource{};
        if (!resource.m_buffer.initialize(*m_device, static_cast<VkDeviceSize>(desc.size), toEBufferUsage(desc.usage),
                                          toMemoryFlags(desc.memory_access)))
        {
            Logger::getInstance().error("Failed to create Vulkan buffer.");
            return {};
        }

        BufferHandle handle = m_buffers.emplace(std::move(resource));
        if (VulkanBufferResource* named_resource = m_buffers.get(handle))
        {
            setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_BUFFER,
                               (uint64_t)named_resource->m_buffer.getVulkanBuffer(),
                               debugNameOrDefault(desc.debug_name, "Kera Buffer"));
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
        deletion.m_timeline_value = getLastSubmittedTimelineValue();
        deletion.m_buffer_resources.push_back(std::move(*resource));
        queueDeferredDeletion(std::move(deletion));
        return true;
    }

    bool VulkanRenderer::mapBuffer(BufferHandle buffer_handle, void** data)
    {
        VulkanBufferResource* resource = m_buffers.get(buffer_handle);
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

    void VulkanRenderer::unmapBuffer(BufferHandle buffer_handle)
    {
        VulkanBufferResource* resource = m_buffers.get(buffer_handle);
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

        ++m_stats.buffer_uploads_this_frame;
        return true;
    }

    BufferHandle VulkanRenderer::createUniformRingBuffer(std::size_t element_size, uint32_t slot_count)
    {
        if (element_size == 0)
        {
            Logger::getInstance().error("Uniform ring buffer element size must be non-zero.");
            return {};
        }

        const uint32_t resolved_slot_count =
            slot_count == 0
                ? static_cast<uint32_t>(m_frame_sync_resources.empty() ? kMaxFramesInFlight : m_frame_sync_resources.size())
                : slot_count;

        VkPhysicalDeviceProperties physical_device_properties{};
        vkGetPhysicalDeviceProperties(m_device->getVulkanPhysicalDevice(), &physical_device_properties);
        const std::size_t uniform_alignment =
            static_cast<std::size_t>(physical_device_properties.limits.minUniformBufferOffsetAlignment);

        const std::size_t ring_slot_stride = alignUp(element_size, uniform_alignment);
        if (ring_slot_stride < element_size ||
            ring_slot_stride > std::numeric_limits<std::size_t>::max() / resolved_slot_count)
        {
            Logger::getInstance().error("Uniform ring buffer size exceeds addresable memory.");
            return {};
        }

        VulkanBufferResource resource{};
        resource.m_ring_slot_size = element_size;
        resource.m_ring_slot_stride = ring_slot_stride;
        resource.m_ring_slot_count = resolved_slot_count;
        if (!resource.m_buffer.initialize(*m_device, static_cast<VkDeviceSize>(ring_slot_stride * resolved_slot_count),
                                          EBufferUsage::UNIFORM, toMemoryFlags(EMemoryAccess::CPU_WRITE)))
        {
            Logger::getInstance().error("Failed to create Vulkan uniform ring buffer.");
            return {};
        }

        BufferHandle handle = m_buffers.emplace(std::move(resource));
        if (VulkanBufferResource* named_resource = m_buffers.get(handle))
        {
            setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_BUFFER,
                               (uint64_t)named_resource->m_buffer.getVulkanBuffer(), "Kera Uniform Ring Buffer");
        }
        return handle;
    }

    bool VulkanRenderer::uploadUniformRingBuffer(BufferHandle buffer_handle, FrameHandle frame_handle, const void* data,
                                                 std::size_t size)
    {
        VulkanBufferResource* buffer = m_buffers.get(buffer_handle);
        VulkanFrameResource* frame = m_frames.get(frame_handle);

        if (!buffer || !frame)
        {
            Logger::getInstance().error("Invalid handle passed to uploadUniformRingBuffer.");
            return false;
        }

        if (buffer->m_ring_slot_size == 0 || buffer->m_ring_slot_stride == 0 || buffer->m_ring_slot_count == 0)
        {
            Logger::getInstance().error("Buffer is not a Vulkan uniform ring buffer.");
            return false;
        }

        if (!data || size == 0)
        {
            Logger::getInstance().error("Invalid data pointer or size passed to uploadUniformRingBuffer.");
            return false;
        }

        if (size > buffer->m_ring_slot_size)
        {
            Logger::getInstance().error("Uniform ring buffer upload exceeds slot size.");
            return false;
        }

        const uint32_t slot = getUniformRingBufferSlot(buffer_handle, frame_handle);
        if (slot == std::numeric_limits<uint32_t>::max())
        {
            Logger::getInstance().error("Failed to resolve uniform ring buffer slot for the given frame.");
            return false;
        }

        const std::size_t offset = getUniformRingBufferSlotOffset(buffer_handle, slot);
        if (offset == std::numeric_limits<uint32_t>::max())
        {
            Logger::getInstance().error("Failed to resolve uniform ring buffer slot offset.");
            return false;
        }

        return uploadBuffer(buffer_handle, data, size, offset);
    }

    UniformRingBufferInfo VulkanRenderer::getUniformRingBufferInfo(BufferHandle buffer_handle) const
    {
        const VulkanBufferResource* buffer = m_buffers.get(buffer_handle);
        if (!buffer || buffer->m_ring_slot_stride == 0 || buffer->m_ring_slot_count == 0)
        {
            return {};
        }

        UniformRingBufferInfo buffer_info = {
            .element_size = buffer->m_ring_slot_size,
            .slot_stride = buffer->m_ring_slot_stride,
            .slot_count = buffer->m_ring_slot_count,
        };

        return buffer_info;
    }

    uint32_t VulkanRenderer::getUniformRingBufferSlot(BufferHandle buffer_handle, FrameHandle frame_handle) const
    {
        const VulkanBufferResource* buffer = m_buffers.get(buffer_handle);
        const VulkanFrameResource* frame = m_frames.get(frame_handle);
        if (!buffer || !frame || buffer->m_ring_slot_stride == 0 || buffer->m_ring_slot_count == 0)
        {
            return std::numeric_limits<uint32_t>::max();
        }
        return frame->m_sync_index % buffer->m_ring_slot_count;
    }

    std::size_t VulkanRenderer::getUniformRingBufferSlotOffset(BufferHandle buffer_handle, uint32_t slot) const
    {
        const VulkanBufferResource* buffer = m_buffers.get(buffer_handle);
        if (!buffer || buffer->m_ring_slot_stride == 0 || buffer->m_ring_slot_count == 0)
        {
            return std::numeric_limits<uint32_t>::max();
        }

        if (slot >= buffer->m_ring_slot_count)
        {
            Logger::getInstance().error("Uniform ring buffer slot index exceeds the slot count.");
            return std::numeric_limits<uint32_t>::max();
        }

        return buffer->m_ring_slot_stride * slot;
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

        if (desc.mip_levels == 0)
        {
            Logger::getInstance().error("Texture mip level count must be non-zero.");
            return {};
        }

        const uint32_t full_mip_levels = textureFullMipLevelCount(desc.width, desc.height);
        const uint32_t requested_mip_levels =
            desc.generate_mipmaps && desc.mip_levels == 1 ? full_mip_levels : desc.mip_levels;
        if (requested_mip_levels > full_mip_levels)
        {
            Logger::getInstance().error("Texture mip level count exceeds the texture extent.");
            return {};
        }

        if (requested_mip_levels > 1 && (desc.depth_stencil || desc.render_target))
        {
            Logger::getInstance().error("Mipmapped render-target or depth textures are not supported.");
            return {};
        }

        if (desc.generate_mipmaps && !desc.sampled)
        {
            Logger::getInstance().error("Generated mipmaps require a sampled texture.");
            return {};
        }

        VkImageUsageFlags usage = 0;
        if (desc.render_target)
        {
            usage |=
                desc.depth_stencil ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }

        if (desc.sampled)
        {
            usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            if (requested_mip_levels > 1)
            {
                usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            }
        }

        if (usage == 0)
        {
            Logger::getInstance().error("TextureDesc must request at least one usage.");
            return {};
        }

        VulkanTextureResource resource{};
        resource.m_device = m_device->getVulkanDevice();
        resource.m_format = toVkTextureFormat(desc.format);
        if (desc.generate_mipmaps && requested_mip_levels > 1)
        {
            VkFormatProperties format_properties{};
            vkGetPhysicalDeviceFormatProperties(m_device->getVulkanPhysicalDevice(), resource.m_format,
                                                &format_properties);
            const VkFormatFeatureFlags required_features = VK_FORMAT_FEATURE_BLIT_SRC_BIT |
                                                          VK_FORMAT_FEATURE_BLIT_DST_BIT |
                                                          VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
            if ((format_properties.optimalTilingFeatures & required_features) != required_features)
            {
                Logger::getInstance().error("Texture format does not support generated mipmaps.");
                return {};
            }
        }

        resource.m_texture_format = desc.format;
        resource.m_extent = {desc.width, desc.height};
        resource.m_mip_levels = requested_mip_levels;
        resource.m_generate_mipmaps = desc.generate_mipmaps && requested_mip_levels > 1;
        resource.m_aspect_mask = desc.depth_stencil ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        resource.m_sampled = desc.sampled;
        resource.m_render_target = desc.render_target;
        resource.m_depth_stencil = desc.depth_stencil;
        resource.m_current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        resource.m_descriptor_layout =
            desc.sampled ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
        resource.m_render_target_final_layout = desc.render_target
                                                 ? (desc.depth_stencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                                                                      : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                                 : VK_IMAGE_LAYOUT_UNDEFINED;

        const bool is_cube = desc.dimension == ETextureDimension::TEXTURE_CUBE;
        const uint32_t array_layers = is_cube ? 6 : 1;
        resource.m_array_layers = array_layers;

        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.flags = is_cube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent = {desc.width, desc.height, 1};
        image_info.mipLevels = resource.m_mip_levels;
        image_info.arrayLayers = array_layers;
        image_info.format = resource.m_format;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = usage;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;

        VkDevice vk_device = m_device->getVulkanDevice();
        if (vkCreateImage(vk_device, &image_info, nullptr, &resource.m_image) != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to create Vulkan texture image.");
            return {};
        }

        VkMemoryRequirements memory_requirements{};
        vkGetImageMemoryRequirements(vk_device, resource.m_image, &memory_requirements);

        VkMemoryAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = memory_requirements.size;
        if (!findMemoryType(m_device->getVulkanPhysicalDevice(), memory_requirements.memoryTypeBits,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocate_info.memoryTypeIndex))
        {
            Logger::getInstance().error("Failed to find suitable Vulkan texture memory type.");
            resource.shutdown();
            return {};
        }

        if (vkAllocateMemory(vk_device, &allocate_info, nullptr, &resource.m_memory) != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to allocate Vulkan texture memory.");
            resource.shutdown();
            return {};
        }

        vkBindImageMemory(vk_device, resource.m_image, resource.m_memory, 0);

        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = resource.m_image;
        view_info.viewType = is_cube ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = resource.m_format;
        view_info.subresourceRange.aspectMask = resource.m_aspect_mask;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = resource.m_mip_levels;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = array_layers;

        if (vkCreateImageView(vk_device, &view_info, nullptr, &resource.m_image_view) != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to create Vulkan texture view.");
            resource.shutdown();
            return {};
        }

        TextureHandle handle = m_textures.emplace(std::move(resource));
        if (VulkanTextureResource* named_resource = m_textures.get(handle))
        {
            setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_IMAGE, (uint64_t)named_resource->m_image,
                               debugNameOrDefault(desc.debug_name, "Kera Texture Image"));
            setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_IMAGE_VIEW,
                               (uint64_t)named_resource->m_image_view,
                               desc.debug_name.empty() ? std::string("Kera Texture View") : desc.debug_name + " View");
        }
        return handle;
    }

    bool VulkanRenderer::beginUploadBatch()
    {
        if (!m_device)
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return false;
        }

        if (m_upload_context.batch_active)
        {
            Logger::getInstance().error("Cannot begin a new upload batch while another batch is active.");
            return false;
        }

        if (!m_upload_context.pending_texture_uploads.empty())
        {
            Logger::getInstance().warning("Can not begin Vulkan Texture upload batch with pending uploads.");
            discardPendingUploads();
            return false;
        }

        m_upload_context.batch_active = true;
        return true;
    }

    bool VulkanRenderer::endUploadBatch()
    {
        if (!m_upload_context.batch_active)
        {
            Logger::getInstance().error("Cannot end an upload batch when no batch is active.");
            return false;
        }

        m_upload_context.batch_active = false;
        if (flushUploads())
        {
            return true;
        }

        discardPendingUploads();
        return false;
    }

    void VulkanRenderer::cancelUploadBatch()
    {
        m_upload_context.batch_active = false;
        discardPendingUploads();
    }

    bool VulkanRenderer::uploadTexture(TextureHandle texture_handle, const void* data, std::size_t size)
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

        VulkanTextureResource* texture = m_textures.get(texture_handle);
        if (!texture)
        {
            Logger::getInstance().error("Invalid texture handle passed to uploadTexture.");
            return false;
        }

        if (!texture->m_sampled || texture->m_descriptor_layout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
            Logger::getInstance().error("Texture upload requires a sampled Vulkan texture.");
            return false;
        }

        if (texture->m_depth_stencil)
        {
            Logger::getInstance().error("Depth texture upload is not supported by uploadTexture.");
            return false;
        }

        if (texture->m_mip_levels > 1 && !texture->m_generate_mipmaps)
        {
            Logger::getInstance().error(
                "Texture upload cannot populate multiple mip levels unless generateMipmaps is enabled.");
            return false;
        }

        const std::size_t bytes_per_pixel = textureFormatBytesPerPixel(texture->m_texture_format);
        if (bytes_per_pixel == 0)
        {
            Logger::getInstance().error("Texture upload uses an unsupported texture format.");
            return false;
        }

        const std::size_t expected_size = static_cast<std::size_t>(texture->m_extent.width) *
                                         static_cast<std::size_t>(texture->m_extent.height) * bytes_per_pixel;
        if (size < expected_size)
        {
            Logger::getInstance().error("Texture upload data is smaller than the texture extent requires.");
            return false;
        }

        Buffer staging_buffer = acquireStagingBuffer(static_cast<VkDeviceSize>(expected_size));
        if (!staging_buffer.isValid())
        {
            Logger::getInstance().error("Failed to create Vulkan texture staging buffer.");
            return false;
        }

        if (!staging_buffer.copyFrom(data, static_cast<VkDeviceSize>(expected_size)))
        {
            Logger::getInstance().error("Failed to upload data to Vulkan texture staging buffer.");
            return false;
        }

        VkBufferImageCopy copy_region{};
        copy_region.bufferOffset = 0;
        copy_region.bufferRowLength = 0;
        copy_region.bufferImageHeight = 0;
        copy_region.imageSubresource.aspectMask = texture->m_aspect_mask;
        copy_region.imageSubresource.mipLevel = 0;
        copy_region.imageSubresource.baseArrayLayer = 0;
        copy_region.imageSubresource.layerCount = 1;
        copy_region.imageOffset = {0, 0, 0};
        copy_region.imageExtent = {texture->m_extent.width, texture->m_extent.height, 1};

        PendingTextureUpload pending_upload{};
        pending_upload.texture = texture_handle;
        pending_upload.staging_buffer = std::move(staging_buffer);
        pending_upload.regions.push_back(copy_region);
        pending_upload.generate_mipmaps = texture->m_generate_mipmaps;

        m_upload_context.pending_texture_uploads.push_back(std::move(pending_upload));
        if (m_upload_context.batch_active || flushUploads())
        {
            return true;
        }

        discardPendingUploads();
        return false;
    }

    bool VulkanRenderer::uploadTextureSubresource(TextureHandle texture_handle, const TexturePrepareUpload& upload)
    {
        if (!m_device)
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return false;
        }

        if (!upload.data || upload.size == 0)
        {
            Logger::getInstance().error("Texture upload data must be non-empty.");
            return false;
        }

        VulkanTextureResource* texture = m_textures.get(texture_handle);
        if (!texture)
        {
            Logger::getInstance().error("Invalid texture handle passed to uploadTexture.");
            return false;
        }

        if (!texture->m_sampled || texture->m_descriptor_layout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
            Logger::getInstance().error("Texture upload requires a sampled Vulkan texture.");
            return false;
        }

        if (texture->m_depth_stencil)
        {
            Logger::getInstance().error("Depth texture upload is not supported by uploadTexture.");
            return false;
        }

        std::vector<VkBufferImageCopy> copy_regions;
        copy_regions.reserve(upload.subresource_count);

        for (std::size_t i = 0; i < upload.subresource_count; ++i)
        {
            const TextureSubresourceUpload& subresource = upload.subresources[i];
            if (subresource.mip_level >= texture->m_mip_levels)
            {
                Logger::getInstance().error("Texture upload subresource mip level exceeds the texture's mip levels.");
                return false;
            }

            if (subresource.array_layer >= texture->m_array_layers)
            {
                Logger::getInstance().error(
                    "Texture upload subresource array layer exceeds the texture's array layers.");
                return false;
            }

            if (subresource.offset > upload.size || subresource.size > upload.size - subresource.offset)
            {
                Logger::getInstance().error("Texture upload subresource exceeds the provided data size.");
                return false;
            }

            VkBufferImageCopy region{};
            region.bufferOffset = subresource.offset;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = texture->m_aspect_mask;
            region.imageSubresource.mipLevel = subresource.mip_level;
            region.imageSubresource.baseArrayLayer = subresource.array_layer;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = {0, 0, 0};
            region.imageExtent = {subresource.width, subresource.height, 1};
            copy_regions.push_back(region);
        }

        Buffer staging_buffer = acquireStagingBuffer(static_cast<VkDeviceSize>(upload.size));
        if (!staging_buffer.isValid())
        {
            Logger::getInstance().error("Failed to create Vulkan texture staging buffer.");
            return false;
        }

        if (!staging_buffer.copyFrom(upload.data, static_cast<VkDeviceSize>(upload.size)))
        {
            Logger::getInstance().error("Failed to upload data to Vulkan texture staging buffer.");
            return false;
        }

        PendingTextureUpload pending_upload{};
        pending_upload.texture = texture_handle;
        pending_upload.staging_buffer = std::move(staging_buffer);
        pending_upload.regions = std::move(copy_regions);
        pending_upload.generate_mipmaps = false;

        m_upload_context.pending_texture_uploads.push_back(std::move(pending_upload));
        if (m_upload_context.batch_active || flushUploads())
        {
            return true;
        }

        discardPendingUploads();
        return false;
    }

    bool VulkanRenderer::destroyTexture(TextureHandle texture)
    {
        const auto pending_upload =
            std::find_if(m_upload_context.pending_texture_uploads.begin(), m_upload_context.pending_texture_uploads.end(),
                         [texture](const PendingTextureUpload& upload) { return upload.texture == texture; });

        if (pending_upload != m_upload_context.pending_texture_uploads.end())
        {
            Logger::getInstance().error("Cannot destroy a Vulkan texture that has pending uploads.");
            return false;
        }

        if (descriptorSetsReference(texture))
        {
            Logger::getInstance().error(
                "Cannot destroy a Vulkan texture that is still referenced by a descriptor set.");
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
        deletion.m_timeline_value = getLastSubmittedTimelineValue();
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

        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = toVkFilter(desc.mag_filter);
        sampler_info.minFilter = toVkFilter(desc.min_filter);
        sampler_info.addressModeU = toVkAddressMode(desc.address_mode_u);
        sampler_info.addressModeV = toVkAddressMode(desc.address_mode_v);
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VkPhysicalDeviceProperties physical_device_properties{};
        vkGetPhysicalDeviceProperties(m_device->getVulkanPhysicalDevice(), &physical_device_properties);
        const float requested_anisotropy = std::max(desc.max_anisotropy, 1.0f);
        const float max_anisotropy = std::min(requested_anisotropy, physical_device_properties.limits.maxSamplerAnisotropy);
        sampler_info.anisotropyEnable = max_anisotropy > 1.0f ? VK_TRUE : VK_FALSE;
        sampler_info.maxAnisotropy = max_anisotropy;
        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.mipmapMode = toVkMipFilter(desc.mip_filter);
        sampler_info.minLod = std::max(desc.min_lod, 0.0f);
        sampler_info.maxLod = std::max(desc.max_lod, sampler_info.minLod);

        if (vkCreateSampler(resource.m_device, &sampler_info, nullptr, &resource.m_sampler) != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to create Vulkan sampler.");
            return {};
        }

        SamplerHandle handle = m_samplers.emplace(std::move(resource));
        if (VulkanSamplerResource* named_resource = m_samplers.get(handle))
        {
            setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_SAMPLER, (uint64_t)named_resource->m_sampler,
                               debugNameOrDefault(desc.debug_name, "Kera Sampler"));
        }
        return handle;
    }

    bool VulkanRenderer::destroySampler(SamplerHandle sampler)
    {
        if (descriptorSetsReference(sampler))
        {
            Logger::getInstance().error(
                "Cannot destroy a Vulkan sampler that is still referenced by a descriptor set.");
            return false;
        }

        std::optional<VulkanSamplerResource> resource = m_samplers.take(sampler);
        if (!resource)
        {
            return false;
        }

        VulkanDeferredDeletion deletion{};
        deletion.m_timeline_value = getLastSubmittedTimelineValue();
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

        VulkanTextureResource* texture = m_textures.get(desc.color_texture);
        if (!texture)
        {
            Logger::getInstance().error("Invalid color texture passed to createRenderTarget.");
            return {};
        }
        if (!texture->m_render_target)
        {
            Logger::getInstance().error(
                "Texture passed to createRenderTarget was not created for render-target usage.");
            return {};
        }
        if (texture->m_depth_stencil)
        {
            Logger::getInstance().error("Color texture passed to createRenderTarget must not be a depth texture.");
            return {};
        }

        VulkanTextureResource* depth_texture = nullptr;
        if (desc.depth_texture.isValid())
        {
            depth_texture = m_textures.get(desc.depth_texture);
            if (!depth_texture)
            {
                Logger::getInstance().error("Invalid depth texture passed to createRenderTarget.");
                return {};
            }
            if (!depth_texture->m_render_target || !depth_texture->m_depth_stencil)
            {
                Logger::getInstance().error(
                    "Depth texture passed to createRenderTarget is not depth render-target usage.");
                return {};
            }
            if (depth_texture->m_extent.width != texture->m_extent.width ||
                depth_texture->m_extent.height != texture->m_extent.height)
            {
                Logger::getInstance().error("Render target color and depth textures must have matching extents.");
                return {};
            }
        }

        VulkanRenderTargetResource resource{};
        resource.m_color_texture = desc.color_texture;
        resource.m_depth_texture = desc.depth_texture;
        resource.m_extent = texture->m_extent;
        return m_render_targets.emplace(std::move(resource));
    }

    bool VulkanRenderer::destroyRenderTarget(RenderTargetHandle render_target)
    {
        return m_render_targets.remove(render_target);
    }

    GraphicsPipelineHandle VulkanRenderer::createGraphicsPipeline(const GraphicsPipelineDesc& desc,
                                                                  ShaderProgramHandle program)
    {
        if (!m_device || !m_swapchain)
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return {};
        }

        VkFormat color_format = VK_FORMAT_UNDEFINED;
        VkFormat depth_format = VK_FORMAT_UNDEFINED;
        if (!resolvePipelineRenderingFormats(desc.render_target, color_format, depth_format))
        {
            Logger::getInstance().error("Invalid render target passed to createGraphicsPipeline.");
            return {};
        }

        VulkanShaderProgramResource* shader_program = m_shader_programs.get(program);
        if (!shader_program)
        {
            Logger::getInstance().error("Invalid shader program handle passed to createGraphicsPipeline.");
            return {};
        }

        const auto graphics_shaders = getGraphicsShaders(*shader_program);
        if (!graphics_shaders[0] || !graphics_shaders[1])
        {
            Logger::getInstance().error(
                "Graphics pipeline creation requires shader program stages for both "
                "vertex and fragment shaders.");
            return {};
        }

        VulkanGraphicsPipelineResource resource{};
        GraphicsPipelineDesc effective_desc = desc;
        if (effective_desc.descriptor_sets.empty() &&
            !deriveDescriptorSetLayoutsFromReflection(shader_program->m_reflection, effective_desc.descriptor_sets))
        {
            Logger::getInstance().error("Failed to derive graphics pipeline descriptor layouts from Slang reflection.");
            return {};
        }

        resource.m_desc = effective_desc;
        resource.m_program = program;
        if (!resource.m_pipeline.initialize(
                *m_device, m_pipeline_cache, color_format, depth_format,
                std::span<const Shader* const>(graphics_shaders.data(), graphics_shaders.size()), effective_desc))
        {
            Logger::getInstance().error("Failed to create Vulkan graphics pipeline.");
            return {};
        }

        GraphicsPipelineHandle handle = m_graphics_pipelines.emplace(std::move(resource));
        if (VulkanGraphicsPipelineResource* named_resource = m_graphics_pipelines.get(handle))
        {
            setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_PIPELINE,
                               (uint64_t)named_resource->m_pipeline.getVulkanPipeline(),
                               debugNameOrDefault(effective_desc.debug_name, "Kera Graphics Pipeline"));
        }
        return handle;
    }

    GraphicsPipelineHandle VulkanRenderer::createGraphicsPipeline(const GraphicsPipelineCreateDesc& desc)
    {
        GraphicsPipelineDesc pipeline_desc{};
        pipeline_desc.topology = desc.topology;
        pipeline_desc.cull_mode = desc.cull_mode;
        pipeline_desc.front_face = desc.front_face;
        pipeline_desc.blend_mode = desc.blend_mode;
        pipeline_desc.render_target = desc.render_target;
        pipeline_desc.vertex_layout = desc.vertex_layout;
        pipeline_desc.descriptor_sets = desc.descriptor_sets;
        pipeline_desc.depth_test = desc.depth_test;
        pipeline_desc.depth_write = desc.depth_write;
        pipeline_desc.debug_name = desc.debug_name;

        if (!desc.vertex_bindings.empty() || !desc.vertex_fields.empty())
        {
            const SlangReflectionMetadata* reflection = getShaderProgramReflection(desc.shader_program);
            if (!reflection)
            {
                Logger::getInstance().error("Shader program reflection is missing while validating vertex input.");
                return {};
            }

            const VertexInputLayoutBuildResult vertex_input =
                buildValidatedVertexInputLayout(*reflection, {
                                                                 .debug_name = desc.debug_name,
                                                                 .bindings = desc.vertex_bindings,
                                                                 .fields = desc.vertex_fields,
                                                             });
            if (!vertex_input)
            {
                Logger::getInstance().error(vertex_input.errorMessage());
                return {};
            }
            pipeline_desc.vertex_layout = vertex_input.layout();
        }

        return createGraphicsPipeline(pipeline_desc, desc.shader_program);
    }

    std::vector<DescriptorSetLayoutDesc> VulkanRenderer::getGraphicsPipelineDescriptorSets(
        GraphicsPipelineHandle pipeline_handle) const
    {
        const VulkanGraphicsPipelineResource* pipeline = m_graphics_pipelines.get(pipeline_handle);
        if (!pipeline)
        {
            Logger::getInstance().error(
                "Invalid graphics pipeline handle passed to getGraphicsPipelineDescriptorSets.");
            return {};
        }

        return pipeline->m_desc.descriptor_sets;
    }

    VertexLayoutDesc VulkanRenderer::getGraphicsPipelineVertexLayout(GraphicsPipelineHandle pipeline_handle) const
    {
        const VulkanGraphicsPipelineResource* pipeline = m_graphics_pipelines.get(pipeline_handle);
        if (!pipeline)
        {
            Logger::getInstance().error("Invalid graphics pipeline handle passed to getGraphicsPipelineVertexLayout.");
            return {};
        }

        return pipeline->m_desc.vertex_layout;
    }

    bool VulkanRenderer::destroyGraphicsPipeline(GraphicsPipelineHandle pipeline)
    {
        std::optional<VulkanGraphicsPipelineResource> resource = m_graphics_pipelines.take(pipeline);
        if (!resource)
        {
            return false;
        }

        VulkanDeferredDeletion deletion{};
        deletion.m_timeline_value = getLastSubmittedTimelineValue();
        deletion.m_graphics_pipelines.push_back(std::move(*resource));
        queueDeferredDeletion(std::move(deletion));
        return true;
    }

    DescriptorSetHandle VulkanRenderer::createDescriptorSet(GraphicsPipelineHandle pipeline_handle)
    {
        VulkanGraphicsPipelineResource* pipeline = m_graphics_pipelines.get(pipeline_handle);
        if (!pipeline)
        {
            Logger::getInstance().error("Invalid graphics pipeline handle passed to createDescriptorSet.");
            return {};
        }

        if (pipeline->m_desc.descriptor_sets.empty())
        {
            Logger::getInstance().error("Graphics pipeline does not expose any descriptor set layouts.");
            return {};
        }

        if (pipeline->m_desc.descriptor_sets.size() != 1)
        {
            Logger::getInstance().error(
                "Graphics pipeline exposes multiple descriptor set layouts; call createDescriptorSet with a set "
                "index.");
            return {};
        }

        return createDescriptorSet(pipeline_handle, pipeline->m_desc.descriptor_sets.front().set);
    }

    DescriptorSetHandle VulkanRenderer::createDescriptorSet(GraphicsPipelineHandle pipeline_handle, uint32_t set)
    {
        if (!m_device || m_descriptor_pools.empty())
        {
            Logger::getInstance().error("Renderer descriptor resources are not initialized.");
            return {};
        }

        VulkanGraphicsPipelineResource* pipeline = m_graphics_pipelines.get(pipeline_handle);
        if (!pipeline)
        {
            Logger::getInstance().error("Invalid graphics pipeline handle passed to createDescriptorSet.");
            return {};
        }

        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
        VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
        if (!allocateDescriptorSet(*pipeline, set, descriptor_set, descriptor_pool))
        {
            Logger::getInstance().error("Failed to allocate Vulkan descriptor set.");
            return {};
        }

        VulkanDescriptorSetResource resource{};
        resource.m_descriptor_set = descriptor_set;
        resource.m_descriptor_pool = descriptor_pool;
        resource.m_pipeline = pipeline_handle;
        resource.m_set = set;
        const DescriptorSetLayoutDesc* layout_desc = resolveDescriptorSetLayout(*pipeline, set);
        if (!layout_desc)
        {
            Logger::getInstance().error("Graphics pipeline descriptor set metadata is missing.");
            vkFreeDescriptorSets(m_device->getVulkanDevice(), descriptor_pool, 1, &descriptor_set);
            return {};
        }
        resource.m_layout = *layout_desc;
        resource.m_debug_name = pipeline->m_desc.debug_name.empty()
                                   ? "Kera Descriptor Set"
                                   : pipeline->m_desc.debug_name + " Descriptor Set " + std::to_string(set);
        DescriptorSetHandle descriptor_set_handle = m_descriptor_sets.emplace(resource);
        setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)descriptor_set,
                           resource.m_debug_name);
        return descriptor_set_handle;
    }

    bool VulkanRenderer::updateDescriptorSet(DescriptorSetHandle set_handle, uint32_t binding, BufferHandle buffer_handle,
                                             std::size_t offset, std::size_t range)
    {
        if (!m_device)
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return false;
        }

        VulkanDescriptorSetResource* descriptor_set = m_descriptor_sets.get(set_handle);
        VulkanBufferResource* buffer = m_buffers.get(buffer_handle);
        if (!descriptor_set || !buffer)
        {
            Logger::getInstance().error("Invalid handle passed to updateDescriptorSet.");
            return false;
        }

        if (!validateDescriptorBinding(*descriptor_set, binding, EDescriptorType::UNIFORM_BUFFER))
        {
            Logger::getInstance().error("Descriptor binding does not accept a uniform buffer.");
            return false;
        }

        std::size_t descriptor_offset = offset;
        if (buffer->m_ring_slot_size != 0 && buffer->m_ring_slot_stride != 0 && buffer->m_ring_slot_count != 0 &&
            offset < buffer->m_ring_slot_size * buffer->m_ring_slot_count && offset % buffer->m_ring_slot_size == 0)
        {
            const std::size_t ring_slot = offset / buffer->m_ring_slot_size;
            descriptor_offset = ring_slot * buffer->m_ring_slot_stride;
        }

        const VkDeviceSize buffer_size = buffer->m_buffer.getSize();
        if (descriptor_offset > buffer_size)
        {
            Logger::getInstance().error("Descriptor buffer offset exceeds buffer size.");
            return false;
        }

        const VkDeviceSize descriptor_range =
            range == 0 ? buffer_size - static_cast<VkDeviceSize>(descriptor_offset) : static_cast<VkDeviceSize>(range);
        if (static_cast<VkDeviceSize>(descriptor_offset) + descriptor_range > buffer_size)
        {
            Logger::getInstance().error("Descriptor buffer range exceeds buffer size.");
            return false;
        }

        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = buffer->m_buffer.getVulkanBuffer();
        buffer_info.offset = static_cast<VkDeviceSize>(descriptor_offset);
        buffer_info.range = descriptor_range;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptor_set->m_descriptor_set;
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &buffer_info;

        vkUpdateDescriptorSets(m_device->getVulkanDevice(), 1, &write, 0, nullptr);
        bool updated_binding = false;
        for (auto& reference : descriptor_set->m_buffers)
        {
            if (reference.m_binding == binding)
            {
                reference.m_handle = buffer_handle;
                reference.m_offset = descriptor_offset;
                reference.m_range = static_cast<std::size_t>(descriptor_range);
                updated_binding = true;
                break;
            }
        }
        if (!updated_binding)
        {
            descriptor_set->m_buffers.push_back(
                {binding, buffer_handle, descriptor_offset, static_cast<std::size_t>(descriptor_range)});
        }
        return true;
    }

    bool VulkanRenderer::updateDescriptorSet(DescriptorSetHandle set_handle, uint32_t binding,
                                             TextureHandle texture_handle)
    {
        if (!m_device)
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return false;
        }

        VulkanDescriptorSetResource* descriptor_set = m_descriptor_sets.get(set_handle);
        VulkanTextureResource* texture = m_textures.get(texture_handle);
        if (!descriptor_set || !texture)
        {
            Logger::getInstance().error("Invalid handle passed to texture updateDescriptorSet.");
            return false;
        }
        if (!validateDescriptorBinding(*descriptor_set, binding, EDescriptorType::SAMPLED_IMAGE))
        {
            Logger::getInstance().error("Descriptor binding does not accept a sampled image.");
            return false;
        }
        if (!texture->m_sampled || texture->m_descriptor_layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            Logger::getInstance().error("Texture is not compatible with sampled-image descriptor usage.");
            return false;
        }

        VkDescriptorImageInfo image_info{};
        image_info.imageLayout = texture->m_descriptor_layout;
        image_info.imageView = texture->m_image_view;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptor_set->m_descriptor_set;
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.pImageInfo = &image_info;

        vkUpdateDescriptorSets(m_device->getVulkanDevice(), 1, &write, 0, nullptr);
        bool updated_binding = false;
        for (auto& reference : descriptor_set->m_textures)
        {
            if (reference.m_binding == binding)
            {
                reference.m_handle = texture_handle;
                updated_binding = true;
                break;
            }
        }
        if (!updated_binding)
        {
            descriptor_set->m_textures.push_back({binding, texture_handle, 0, 0});
        }
        return true;
    }

    bool VulkanRenderer::updateDescriptorSet(DescriptorSetHandle set_handle, uint32_t binding,
                                             SamplerHandle sampler_handle)
    {
        if (!m_device)
        {
            Logger::getInstance().error("Renderer is not initialized.");
            return false;
        }

        VulkanDescriptorSetResource* descriptor_set = m_descriptor_sets.get(set_handle);
        VulkanSamplerResource* sampler = m_samplers.get(sampler_handle);
        if (!descriptor_set || !sampler)
        {
            Logger::getInstance().error("Invalid handle passed to sampler updateDescriptorSet.");
            return false;
        }
        if (!validateDescriptorBinding(*descriptor_set, binding, EDescriptorType::SAMPLER))
        {
            Logger::getInstance().error("Descriptor binding does not accept a sampler.");
            return false;
        }

        VkDescriptorImageInfo image_info{};
        image_info.sampler = sampler->m_sampler;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptor_set->m_descriptor_set;
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        write.pImageInfo = &image_info;

        vkUpdateDescriptorSets(m_device->getVulkanDevice(), 1, &write, 0, nullptr);
        bool updated_binding = false;
        for (auto& reference : descriptor_set->m_samplers)
        {
            if (reference.m_binding == binding)
            {
                reference.m_handle = sampler_handle;
                updated_binding = true;
                break;
            }
        }
        if (!updated_binding)
        {
            descriptor_set->m_samplers.push_back({binding, sampler_handle, 0, 0});
        }
        return true;
    }

    bool VulkanRenderer::updateDescriptorSet(DescriptorSetHandle set_handle, const std::string& name,
                                             BufferHandle buffer_handle, std::size_t offset, std::size_t range)
    {
        VulkanDescriptorSetResource* descriptor_set = m_descriptor_sets.get(set_handle);
        if (!descriptor_set)
        {
            Logger::getInstance().error("Invalid descriptor set handle passed to named buffer updateDescriptorSet.");
            return false;
        }

        const DescriptorBindingDesc* binding = findDescriptorBinding(descriptor_set->m_layout, name);
        if (!binding || binding->type != EDescriptorType::UNIFORM_BUFFER)
        {
            Logger::getInstance().error("Descriptor binding name '" + name + "' does not accept a uniform buffer.");
            return false;
        }

        return updateDescriptorSet(set_handle, binding->binding, buffer_handle, offset, range);
    }

    bool VulkanRenderer::updateDescriptorSet(DescriptorSetHandle set_handle, const std::string& name,
                                             TextureHandle texture_handle)
    {
        VulkanDescriptorSetResource* descriptor_set = m_descriptor_sets.get(set_handle);
        if (!descriptor_set)
        {
            Logger::getInstance().error("Invalid descriptor set handle passed to named texture updateDescriptorSet.");
            return false;
        }

        const DescriptorBindingDesc* binding = findDescriptorBinding(descriptor_set->m_layout, name);
        if (!binding || binding->type != EDescriptorType::SAMPLED_IMAGE)
        {
            Logger::getInstance().error("Descriptor binding name '" + name + "' does not accept a sampled image.");
            return false;
        }

        return updateDescriptorSet(set_handle, binding->binding, texture_handle);
    }

    bool VulkanRenderer::updateDescriptorSet(DescriptorSetHandle set_handle, const std::string& name,
                                             SamplerHandle sampler_handle)
    {
        VulkanDescriptorSetResource* descriptor_set = m_descriptor_sets.get(set_handle);
        if (!descriptor_set)
        {
            Logger::getInstance().error("Invalid descriptor set handle passed to named sampler updateDescriptorSet.");
            return false;
        }

        const DescriptorBindingDesc* binding = findDescriptorBinding(descriptor_set->m_layout, name);
        if (!binding || binding->type != EDescriptorType::SAMPLER)
        {
            Logger::getInstance().error("Descriptor binding name '" + name + "' does not accept a sampler.");
            return false;
        }

        return updateDescriptorSet(set_handle, binding->binding, sampler_handle);
    }

    bool VulkanRenderer::destroyDescriptorSet(DescriptorSetHandle set_handle)
    {
        VulkanDescriptorSetResource* descriptor_set = m_descriptor_sets.get(set_handle);
        if (!descriptor_set)
        {
            return false;
        }

        std::optional<VulkanDescriptorSetResource> resource = m_descriptor_sets.take(set_handle);
        if (!resource)
        {
            return false;
        }

        VulkanDeferredDeletion deletion{};
        deletion.m_timeline_value = getLastSubmittedTimelineValue();
        deletion.m_descriptor_sets.push_back(std::move(*resource));
        queueDeferredDeletion(std::move(deletion));
        return true;
    }

    bool VulkanRenderer::validateDescriptorSet(DescriptorSetHandle set_handle) const
    {
        const RendererValidationReport report = validateDescriptorSetDetailed(set_handle);
        logValidationReport(report);
        m_stats.validation_issues_this_frame += static_cast<uint32_t>(report.issues.size());
        return report.ok();
    }

    bool VulkanRenderer::validateGraphicsPipelineDescriptorSets(GraphicsPipelineHandle pipeline_handle) const
    {
        const RendererValidationReport report = validateGraphicsPipelineDescriptorSetsDetailed(pipeline_handle);
        logValidationReport(report);
        m_stats.validation_issues_this_frame += static_cast<uint32_t>(report.issues.size());
        return report.ok();
    }

    RendererValidationReport VulkanRenderer::validateDescriptorSetDetailed(DescriptorSetHandle set_handle) const
    {
        RendererValidationReport report;
        const VulkanDescriptorSetResource* descriptor_set = m_descriptor_sets.get(set_handle);
        if (!descriptor_set)
        {
            report.addIssue(ERendererErrorCode::INVALID_HANDLE, ERendererValidationCategory::DESCRIPTOR,
                            "Invalid descriptor set handle passed to validateDescriptorSetDetailed.");
            return report;
        }

        return validateDescriptorSetResource(*descriptor_set);
    }

    RendererValidationReport VulkanRenderer::validateGraphicsPipelineDescriptorSetsDetailed(
        GraphicsPipelineHandle pipeline_handle) const
    {
        RendererValidationReport report;
        const VulkanGraphicsPipelineResource* pipeline = m_graphics_pipelines.get(pipeline_handle);
        if (!pipeline)
        {
            report.addIssue(
                ERendererErrorCode::INVALID_HANDLE, ERendererValidationCategory::DESCRIPTOR,
                "Invalid graphics pipeline handle passed to validateGraphicsPipelineDescriptorSetsDetailed.");
            return report;
        }

        std::vector<uint32_t> allocated_sets;
        m_descriptor_sets.forEach(
            [this, pipeline_handle, &allocated_sets, &report](DescriptorSetHandle,
                                                            const VulkanDescriptorSetResource& descriptor_set)
            {
                if (descriptor_set.m_pipeline != pipeline_handle)
                {
                    return true;
                }

                allocated_sets.push_back(descriptor_set.m_set);
                RendererValidationReport descriptor_report = validateDescriptorSetResource(descriptor_set);
                report.issues.insert(report.issues.end(), descriptor_report.issues.begin(),
                                     descriptor_report.issues.end());
                return true;
            });

        for (const DescriptorSetLayoutDesc& layout : pipeline->m_desc.descriptor_sets)
        {
            const bool set_allocated =
                std::find(allocated_sets.begin(), allocated_sets.end(), layout.set) != allocated_sets.end();
            if (!set_allocated)
            {
                report.addIssue(
                    ERendererErrorCode::VALIDATION_FAILED, ERendererValidationCategory::DESCRIPTOR,
                    "Graphics pipeline is missing allocated descriptor set " + std::to_string(layout.set) + ".",
                    layout.set);
            }
        }

        return report;
    }

    bool VulkanRenderer::setDebugName(BufferHandle buffer_handle, const std::string& name)
    {
        VulkanBufferResource* buffer = m_buffers.get(buffer_handle);
        if (!m_device || !buffer || name.empty())
        {
            return false;
        }

        setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_BUFFER,
                           (uint64_t)buffer->m_buffer.getVulkanBuffer(), name);
        return true;
    }

    bool VulkanRenderer::setDebugName(TextureHandle texture_handle, const std::string& name)
    {
        VulkanTextureResource* texture = m_textures.get(texture_handle);
        if (!m_device || !texture || name.empty())
        {
            return false;
        }

        setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_IMAGE, (uint64_t)texture->m_image, name);
        setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)texture->m_image_view,
                           name + " View");
        return true;
    }

    bool VulkanRenderer::setDebugName(SamplerHandle sampler_handle, const std::string& name)
    {
        VulkanSamplerResource* sampler = m_samplers.get(sampler_handle);
        if (!m_device || !sampler || name.empty())
        {
            return false;
        }

        setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_SAMPLER, (uint64_t)sampler->m_sampler, name);
        return true;
    }

    bool VulkanRenderer::setDebugName(GraphicsPipelineHandle pipeline_handle, const std::string& name)
    {
        VulkanGraphicsPipelineResource* pipeline = m_graphics_pipelines.get(pipeline_handle);
        if (!m_device || !pipeline || name.empty())
        {
            return false;
        }

        pipeline->m_desc.debug_name = name;
        setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_PIPELINE,
                           (uint64_t)pipeline->m_pipeline.getVulkanPipeline(), name);
        return true;
    }

    bool VulkanRenderer::setDebugName(DescriptorSetHandle descriptor_set_handle, const std::string& name)
    {
        VulkanDescriptorSetResource* descriptor_set = m_descriptor_sets.get(descriptor_set_handle);
        if (!m_device || !descriptor_set || name.empty())
        {
            return false;
        }

        descriptor_set->m_debug_name = name;
        setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_DESCRIPTOR_SET,
                           (uint64_t)descriptor_set->m_descriptor_set, name);
        return true;
    }

    FrameHandle VulkanRenderer::beginFrame()
    {
        if (!m_device || !m_swapchain || m_command_buffers.empty() || m_frame_sync_resources.empty())
        {
            Logger::getInstance().error("Renderer frame resources are not initialized.");
            return {};
        }

        if ((m_swapchain_recreate_requested && m_window_extent.width == 0) || m_window_extent.height == 0)
        {
            return {};
        }

        const VkExtent2D current_swapchain_extent = m_swapchain->getExtent();
        if (current_swapchain_extent.width == 0 || current_swapchain_extent.height == 0)
        {
            m_swapchain_recreate_requested = true;
            return {};
        }

        const uint32_t sync_index = m_current_frame_sync_index;
        VulkanFrameSyncResource& frame_sync = m_frame_sync_resources[sync_index];
        CommandBuffer& command_buffer = *m_command_buffers[sync_index];
        if (sync_index >= m_active_frame_handles.size())
        {
            Logger::getInstance().error("Frame sync slot is missing active-frame tracking.");
            return {};
        }

        FrameHandle& active_frame_handle = m_active_frame_handles[sync_index];
        if (active_frame_handle.isValid())
        {
            if (m_frames.get(active_frame_handle))
            {
                Logger::getInstance().error("Cannot begin a new Vulkan frame while the current sync slot is active.");
                return {};
            }
            active_frame_handle = {};
        }

        if (!waitForTimelineValue(frame_sync.m_timeline_value))
        {
            Logger::getInstance().error("Failed to wait for Vulkan frame timeline.");
            return {};
        }
        command_buffer.markCompleted();
        clearCompletedFrameResourceUse(sync_index);
        collectDeferredDeletions();

        const uint32_t swapchain_image_count = m_swapchain->getImageCount();
        if (m_images_in_flight.size() != swapchain_image_count || m_swapchain_image_layouts.size() != swapchain_image_count ||
            m_swapchain->getImageViews().size() < swapchain_image_count)
        {
            Logger::getInstance().error("Swapchain frame resources do not match swapchain image count.");
            return {};
        }

        if (!command_buffer.reset() || !command_buffer.begin())
        {
            Logger::getInstance().error("Failed to begin Vulkan command buffer.");
            return {};
        }

        uint32_t image_index = 0;
        const VkResult acquire_result =
            m_swapchain->acquireNextImage(frame_sync.m_image_available_semaphore, VK_NULL_HANDLE, &image_index);

        if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            Logger::getInstance().info("Vulkan swapchain is out of date during image acquire; recreating.");
            m_swapchain_recreate_requested = true;
            command_buffer.reset();
            if (!recreateSwapchainFromWindow())
            {
                Logger::getInstance().error("Failed to recreate Vulkan swapchain after image acquire.");
            }
            return {};
        }

        if (acquire_result == VK_SUBOPTIMAL_KHR)
        {
            m_swapchain_recreate_requested = true;
        }
        else if (acquire_result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to acquire Vulkan swapchain image.");
            command_buffer.reset();
            return {};
        }

        if (image_index >= m_images_in_flight.size())
        {
            Logger::getInstance().error("Swapchain image index exceeded in-flight image tracking.");
            command_buffer.reset();
            return {};
        }

        if (m_images_in_flight[image_index] != 0)
        {
            if (!waitForTimelineValue(m_images_in_flight[image_index]))
            {
                Logger::getInstance().error("Failed to wait for in-flight Vulkan swapchain image timeline.");
                command_buffer.reset();
                return {};
            }
            collectDeferredDeletions();
        }

        VkImageView image_view = m_swapchain->getImageViews()[image_index];
        if (image_view == VK_NULL_HANDLE)
        {
            Logger::getInstance().error("Failed to get Vulkan swapchain image view.");
            command_buffer.reset();
            return {};
        }

        VulkanFrameResource frame{};
        frame.m_command_buffer = command_buffer.getVulkanCommandBuffer();
        frame.m_color_image_view = image_view;
        frame.m_extent = m_swapchain->getExtent();
        frame.m_image_index = image_index;
        frame.m_sync_index = sync_index;
        m_stats.draw_calls_this_frame = 0;
        m_stats.pipelines_bound_this_frame = 0;
        m_stats.descriptor_sets_bound_this_frame = 0;
        m_stats.vertex_buffers_bound_this_frame = 0;
        m_stats.index_buffers_bound_this_frame = 0;
        m_stats.buffer_uploads_this_frame = 0;
        m_stats.texture_uploads_this_frame = 0;
        m_stats.validation_issues_this_frame = 0;
        ++m_stats.frame_index;
        FrameHandle frame_handle = m_frames.emplace(frame);
        active_frame_handle = frame_handle;
        return frame_handle;
    }

    void VulkanRenderer::beginRenderPass(FrameHandle frame_handle, const RenderPassDesc& desc)
    {
        VulkanFrameResource* frame = m_frames.get(frame_handle);
        if (!frame)
        {
            Logger::getInstance().error("Invalid frame handle passed to beginRenderPass.");
            return;
        }
        if (frame->m_render_pass_active)
        {
            Logger::getInstance().error("Cannot begin a Vulkan render pass while another render pass is active.");
            return;
        }
        if (frame->m_extent.width == 0 || frame->m_extent.height == 0)
        {
            Logger::getInstance().error("Cannot begin a Vulkan backbuffer pass with a zero extent.");
            return;
        }

        transitionSwapchainImageLayout(frame->m_command_buffer, frame->m_image_index,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkRenderingAttachmentInfo color_attachment{};
        color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_attachment.imageView = frame->m_color_image_view;
        color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.clearValue.color = {
            {desc.clear_color.r, desc.clear_color.g, desc.clear_color.b, desc.clear_color.a}};

        VkRenderingInfo rendering_info{};
        rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering_info.renderArea.offset = {0, 0};
        rendering_info.renderArea.extent = frame->m_extent;
        rendering_info.layerCount = 1;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachments = &color_attachment;

        beginDebugLabel(m_device->getVulkanDevice(), frame->m_command_buffer, "Kera Backbuffer Pass", 0.2f, 0.4f, 0.9f);
        vkCmdBeginRendering(frame->m_command_buffer, &rendering_info);
        frame->m_render_pass_active = true;
        frame->m_active_render_target_texture = {};
        frame->m_active_depth_texture = {};
        frame->m_render_pass_final_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(frame->m_extent.width);
        viewport.height = static_cast<float>(frame->m_extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(frame->m_command_buffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = frame->m_extent;
        vkCmdSetScissor(frame->m_command_buffer, 0, 1, &scissor);
    }

    void VulkanRenderer::beginRenderPass(FrameHandle frame_handle, RenderTargetHandle render_target_handle,
                                         const RenderPassDesc& desc)
    {
        VulkanFrameResource* frame = m_frames.get(frame_handle);
        VulkanRenderTargetResource* render_target = m_render_targets.get(render_target_handle);
        if (!frame || !render_target)
        {
            Logger::getInstance().error("Invalid handle passed to render target beginRenderPass.");
            return;
        }
        if (frame->m_render_pass_active)
        {
            Logger::getInstance().error(
                "Cannot begin a Vulkan render target pass while another render pass is active.");
            return;
        }
        VulkanTextureResource* texture = m_textures.get(render_target->m_color_texture);
        if (!texture)
        {
            Logger::getInstance().error("Render target color texture is invalid.");
            return;
        }
        if (render_target->m_extent.width == 0 || render_target->m_extent.height == 0)
        {
            Logger::getInstance().error("Cannot begin a Vulkan render target pass with a zero extent.");
            return;
        }

        transitionTextureLayout(frame->m_command_buffer, *texture, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VulkanTextureResource* depth_texture =
            render_target->m_depth_texture.isValid() ? m_textures.get(render_target->m_depth_texture) : nullptr;
        if (render_target->m_depth_texture.isValid() && !depth_texture)
        {
            Logger::getInstance().error("Render target depth texture is invalid.");
            return;
        }
        if (depth_texture)
        {
            transitionTextureLayout(frame->m_command_buffer, *depth_texture,
                                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }

        VkRenderingAttachmentInfo color_attachment{};
        color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_attachment.imageView = texture->m_image_view;
        color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.clearValue.color = {
            {desc.clear_color.r, desc.clear_color.g, desc.clear_color.b, desc.clear_color.a}};

        VkRenderingAttachmentInfo depth_attachment{};
        if (depth_texture)
        {
            depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depth_attachment.imageView = depth_texture->m_image_view;
            depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depth_attachment.clearValue.depthStencil = {desc.clear_depth, 0};
        }

        VkRenderingInfo rendering_info{};
        rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering_info.renderArea.offset = {0, 0};
        rendering_info.renderArea.extent = render_target->m_extent;
        rendering_info.layerCount = 1;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachments = &color_attachment;
        rendering_info.pDepthAttachment = depth_texture ? &depth_attachment : nullptr;

        beginDebugLabel(m_device->getVulkanDevice(), frame->m_command_buffer, "Kera Render Target Pass", 0.4f, 0.8f,
                        0.4f);
        vkCmdBeginRendering(frame->m_command_buffer, &rendering_info);
        frame->m_render_pass_active = true;
        frame->m_active_render_target_texture = render_target->m_color_texture;
        frame->m_active_depth_texture = render_target->m_depth_texture;
        frame->m_render_pass_final_layout = texture->m_render_target_final_layout;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(render_target->m_extent.width);
        viewport.height = static_cast<float>(render_target->m_extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(frame->m_command_buffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = render_target->m_extent;
        vkCmdSetScissor(frame->m_command_buffer, 0, 1, &scissor);
    }

    void VulkanRenderer::endRenderPass(FrameHandle frame_handle)
    {
        VulkanFrameResource* frame = m_frames.get(frame_handle);
        if (!frame)
        {
            Logger::getInstance().error("Invalid frame handle passed to endRenderPass.");
            return;
        }
        if (!frame->m_render_pass_active)
        {
            Logger::getInstance().error("Cannot end a Vulkan render pass when no render pass is active.");
            return;
        }

        vkCmdEndRendering(frame->m_command_buffer);
        endDebugLabel(m_device->getVulkanDevice(), frame->m_command_buffer);
        frame->m_render_pass_active = false;
        if (frame->m_active_render_target_texture.isValid())
        {
            VulkanTextureResource* texture = m_textures.get(frame->m_active_render_target_texture);
            if (texture)
            {
                transitionTextureLayout(frame->m_command_buffer, *texture, frame->m_render_pass_final_layout);
            }
            frame->m_active_render_target_texture = {};
        }
        else
        {
            transitionSwapchainImageLayout(frame->m_command_buffer, frame->m_image_index, frame->m_render_pass_final_layout);
        }
        if (frame->m_active_depth_texture.isValid())
        {
            VulkanTextureResource* texture = m_textures.get(frame->m_active_depth_texture);
            if (texture)
            {
                texture->m_current_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }
            frame->m_active_depth_texture = {};
        }
        frame->m_render_pass_final_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    void VulkanRenderer::bindPipeline(FrameHandle frame_handle, GraphicsPipelineHandle pipeline_handle)
    {
        VulkanFrameResource* frame = m_frames.get(frame_handle);
        VulkanGraphicsPipelineResource* pipeline = m_graphics_pipelines.get(pipeline_handle);

        if (!frame || !pipeline)
        {
            Logger::getInstance().error("Invalid handle passed to bindPipeline.");
            return;
        }
        if (!frame->m_render_pass_active)
        {
            Logger::getInstance().error("Cannot bind a Vulkan graphics pipeline outside an active render pass.");
            return;
        }

        vkCmdBindPipeline(frame->m_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline->m_pipeline.getVulkanPipeline());
        ++m_stats.pipelines_bound_this_frame;
    }

    void VulkanRenderer::bindVertexBuffer(FrameHandle frame_handle, uint32_t slot, BufferHandle buffer_handle,
                                          std::size_t offset)
    {
        VulkanFrameResource* frame = m_frames.get(frame_handle);
        VulkanBufferResource* buffer = m_buffers.get(buffer_handle);
        if (!frame || !buffer)
        {
            Logger::getInstance().error("Invalid handle passed to bindVertexBuffer.");
            return;
        }
        if (!frame->m_render_pass_active)
        {
            Logger::getInstance().error("Cannot bind a Vulkan vertex buffer outside an active render pass.");
            return;
        }

        VkBuffer vertex_buffers[] = {buffer->m_buffer.getVulkanBuffer()};
        VkDeviceSize offsets[] = {static_cast<VkDeviceSize>(offset)};
        vkCmdBindVertexBuffers(frame->m_command_buffer, slot, 1, vertex_buffers, offsets);
        ++m_stats.vertex_buffers_bound_this_frame;
    }

    void VulkanRenderer::bindIndexBuffer(FrameHandle frame_handle, BufferHandle buffer_handle, EIndexFormat format,
                                         std::size_t offset)
    {
        VulkanFrameResource* frame = m_frames.get(frame_handle);
        VulkanBufferResource* buffer = m_buffers.get(buffer_handle);
        if (!frame || !buffer)
        {
            Logger::getInstance().error("Invalid handle passed to bindIndexBuffer.");
            return;
        }
        if (!frame->m_render_pass_active)
        {
            Logger::getInstance().error("Cannot bind a Vulkan index buffer outside an active render pass.");
            return;
        }

        const VkIndexType index_type = format == EIndexFormat::U_INT32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
        vkCmdBindIndexBuffer(frame->m_command_buffer, buffer->m_buffer.getVulkanBuffer(),
                             static_cast<VkDeviceSize>(offset), index_type);
        ++m_stats.index_buffers_bound_this_frame;
    }

    void VulkanRenderer::bindDescriptorSet(FrameHandle frame_handle, GraphicsPipelineHandle pipeline_handle,
                                           DescriptorSetHandle descriptor_set_handle)
    {
        VulkanDescriptorSetResource* descriptor_set = m_descriptor_sets.get(descriptor_set_handle);
        if (!descriptor_set)
        {
            Logger::getInstance().error("Invalid descriptor set handle passed to bindDescriptorSet.");
            return;
        }

        bindDescriptorSet(frame_handle, pipeline_handle, descriptor_set->m_set, descriptor_set_handle);
    }

    void VulkanRenderer::bindDescriptorSet(FrameHandle frame_handle, GraphicsPipelineHandle pipeline_handle, uint32_t set,
                                           DescriptorSetHandle descriptor_set_handle)
    {
        VulkanFrameResource* frame = m_frames.get(frame_handle);
        VulkanGraphicsPipelineResource* pipeline = m_graphics_pipelines.get(pipeline_handle);
        VulkanDescriptorSetResource* descriptor_set = m_descriptor_sets.get(descriptor_set_handle);
        if (!frame || !pipeline || !descriptor_set)
        {
            Logger::getInstance().error("Invalid handle passed to bindDescriptorSet.");
            return;
        }
        if (!frame->m_render_pass_active)
        {
            Logger::getInstance().error("Cannot bind a Vulkan descriptor set outside an active render pass.");
            return;
        }

        if (descriptor_set->m_set != set || descriptor_set->m_pipeline != pipeline_handle)
        {
            Logger::getInstance().error("Descriptor set is not compatible with the requested pipeline set.");
            return;
        }

        const DescriptorSetLayoutDesc* expected_layout = resolveDescriptorSetLayout(*pipeline, set);
        if (!expected_layout || !descriptorSetLayoutsCompatible(descriptor_set->m_layout, *expected_layout))
        {
            Logger::getInstance().error("Descriptor set layout is not compatible with the requested pipeline.");
            return;
        }

        vkCmdBindDescriptorSets(frame->m_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline->m_pipeline.getPipelineLayout(), set, 1, &descriptor_set->m_descriptor_set, 0,
                                nullptr);
        recordDescriptorSetUse(frame->m_sync_index, descriptor_set_handle, *descriptor_set);
        ++m_stats.descriptor_sets_bound_this_frame;
    }

    void VulkanRenderer::drawIndexed(FrameHandle frame_handle, uint32_t index_count, uint32_t instance_count)
    {
        VulkanFrameResource* frame = m_frames.get(frame_handle);
        if (!frame)
        {
            Logger::getInstance().error("Invalid frame handle passed to drawIndexed.");
            return;
        }
        if (!frame->m_render_pass_active)
        {
            Logger::getInstance().error("Cannot draw Vulkan indexed geometry outside an active render pass.");
            return;
        }

        vkCmdDrawIndexed(frame->m_command_buffer, index_count, instance_count, 0, 0, 0);
        ++m_stats.draw_calls_this_frame;
    }

    bool VulkanRenderer::endFrame(FrameHandle frame_handle)
    {
        if (!m_device || m_command_buffers.empty() || m_frame_sync_resources.empty() || !m_swapchain)
        {
            Logger::getInstance().error("Renderer frame resources are not initialized.");
            return false;
        }

        VulkanFrameResource* frame = m_frames.get(frame_handle);
        if (!frame)
        {
            Logger::getInstance().error("Invalid frame handle passed to endFrame.");
            return false;
        }

        if (frame->m_sync_index >= m_frame_sync_resources.size() || frame->m_sync_index >= m_command_buffers.size())
        {
            Logger::getInstance().error("Frame references an invalid sync resource.");
            releaseFrame(frame_handle, frame->m_sync_index);
            return false;
        }

        const uint32_t sync_index = frame->m_sync_index;

        if (sync_index >= m_active_frame_handles.size() || m_active_frame_handles[sync_index] != frame_handle)
        {
            Logger::getInstance().error("Frame handle does not match the active Vulkan frame slot.");
            releaseFrame(frame_handle, sync_index);
            return false;
        }

        if (frame->m_render_pass_active)
        {
            Logger::getInstance().error("Cannot end a Vulkan frame while a render pass is still active.");
            releaseFrame(frame_handle, sync_index);
            return false;
        }

        VulkanFrameSyncResource& frame_sync = m_frame_sync_resources[sync_index];
        CommandBuffer& command_buffer = *m_command_buffers[sync_index];

        if (!command_buffer.end())
        {
            Logger::getInstance().error("Failed to end Vulkan command buffer.");
            releaseFrame(frame_handle, sync_index);
            return false;
        }

        const uint32_t image_index = frame->m_image_index;
        if (image_index >= m_swapchain->getImageCount() || image_index >= m_images_in_flight.size())
        {
            Logger::getInstance().error("Swapchain image index exceeded available swapchain images.");
            releaseFrame(frame_handle, sync_index);
            return false;
        }

        const uint64_t signal_timeline_value = reserveTimelineValue();

        VkSemaphoreSubmitInfo wait_semaphore_info{};
        wait_semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        wait_semaphore_info.semaphore = frame_sync.m_image_available_semaphore;
        wait_semaphore_info.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkCommandBufferSubmitInfo command_buffer_info{};
        command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        command_buffer_info.commandBuffer = frame->m_command_buffer;

        std::array<VkSemaphoreSubmitInfo, 2> signal_semaphore_infos{};
        signal_semaphore_infos[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signal_semaphore_infos[0].semaphore = frame_sync.m_render_finished_semaphore;
        signal_semaphore_infos[0].stageMask = renderCompleteStageMask();
        signal_semaphore_infos[1].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signal_semaphore_infos[1].semaphore = m_frame_timeline_semaphore;
        signal_semaphore_infos[1].value = signal_timeline_value;
        signal_semaphore_infos[1].stageMask = renderCompleteStageMask();

        VkSubmitInfo2 submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submit_info.waitSemaphoreInfoCount = 1;
        submit_info.pWaitSemaphoreInfos = &wait_semaphore_info;
        submit_info.commandBufferInfoCount = 1;
        submit_info.pCommandBufferInfos = &command_buffer_info;
        submit_info.signalSemaphoreInfoCount = static_cast<uint32_t>(signal_semaphore_infos.size());
        submit_info.pSignalSemaphoreInfos = signal_semaphore_infos.data();
        VkResult submit_result = vkQueueSubmit2(m_device->getGraphicsQueue(), 1, &submit_info, VK_NULL_HANDLE);

        if (submit_result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to submit Vulkan draw command buffer.");
            releaseFrame(frame_handle, sync_index);
            return false;
        }
        command_buffer.markSubmitted();
        frame_sync.m_timeline_value = signal_timeline_value;
        m_images_in_flight[image_index] = signal_timeline_value;

        const VkResult present_result =
            m_swapchain->present(image_index, frame_sync.m_render_finished_semaphore, m_device->getPresentQueue());

        const bool should_recreate_swapchain = m_swapchain_recreate_requested ||
                                             present_result == VK_ERROR_OUT_OF_DATE_KHR ||
                                             present_result == VK_SUBOPTIMAL_KHR;

        if (present_result != VK_SUCCESS && present_result != VK_SUBOPTIMAL_KHR &&
            present_result != VK_ERROR_OUT_OF_DATE_KHR)
        {
            Logger::getInstance().error("Failed to present Vulkan swapchain image.");
            releaseFrame(frame_handle, sync_index);
            return false;
        }

        releaseFrame(frame_handle, sync_index);
        m_current_frame_sync_index = (m_current_frame_sync_index + 1) % static_cast<uint32_t>(m_frame_sync_resources.size());
        if (should_recreate_swapchain)
        {
            Logger::getInstance().info("Vulkan swapchain needs recreation after present.");
            const bool has_pending_resize = m_resize_pending;
            const Extent2D pending_resize_extent = m_pending_resize_extent;
            bool recreate_succeeded = false;
            if (has_pending_resize && pending_resize_extent.width > 0 && pending_resize_extent.height > 0)
            {
                recreate_succeeded = resize(pending_resize_extent);
            }
            else
            {
                recreate_succeeded = recreateSwapchainFromWindow();
            }

            if (!recreate_succeeded)
            {
                Logger::getInstance().error("Failed to recreate Vulkan swapchain after present.");
                if (has_pending_resize)
                {
                    m_pending_resize_extent = pending_resize_extent;
                    m_resize_pending = true;
                    m_swapchain_recreate_requested = true;
                }
                return false;
            }
        }
        collectDeferredDeletions();
        return true;
    }

    bool VulkanRenderer::recreateSwapchainResources(uint32_t width, uint32_t height)
    {
        if (!m_physical_device || !m_device || !m_surface)
        {
            return false;
        }
        if (width == 0 || height == 0)
        {
            m_swapchain_recreate_requested = true;
            return false;
        }

        if (hasActiveFrames())
        {
            Logger::getInstance().error("Cannot recreate Vulkan swapchain while a frame is active.");
            return false;
        }

        if (!refreshSwapchainSupport())
        {
            return false;
        }
        if (surfaceWouldUseZeroExtent(m_physical_device->getSwapChainSupport(), width, height))
        {
            m_swapchain_recreate_requested = true;
            return false;
        }

        m_graphics_pipelines.forEach(
            [](GraphicsPipelineHandle, VulkanGraphicsPipelineResource& resource)
            {
                resource.m_pipeline.shutdown();
                return true;
            });
        m_frames.clear();
        m_active_frame_handles.clear();

        for (std::unique_ptr<CommandBuffer>& command_buffer : m_command_buffers)
        {
            if (command_buffer)
            {
                command_buffer->shutdown();
            }
        }
        m_command_buffers.clear();

        if (!m_swapchain)
        {
            m_swapchain = std::make_shared<SwapChain>();
        }

        if (!m_swapchain->initialize(*m_physical_device, *m_device, m_surface->getVulkanSurface(), width, height))
        {
            return false;
        }
        m_swapchain_image_layouts.assign(m_swapchain->getImageCount(), VK_IMAGE_LAYOUT_UNDEFINED);

        const uint32_t frame_resource_count =
            m_swapchain->getImageCount() < kMaxFramesInFlight ? m_swapchain->getImageCount() : kMaxFramesInFlight;
        if (frame_resource_count == 0)
        {
            return false;
        }

        m_command_buffers.reserve(frame_resource_count);
        for (uint32_t index = 0; index < frame_resource_count; ++index)
        {
            auto command_buffer = std::make_unique<CommandBuffer>();
            if (!command_buffer->initialize(*m_device))
            {
                return false;
            }
            m_command_buffers.push_back(std::move(command_buffer));
        }

        return recreateLiveGraphicsPipelines();
    }

    bool VulkanRenderer::recreateLiveGraphicsPipelines()
    {
        if (!m_device || !m_swapchain)
        {
            return false;
        }

        return m_graphics_pipelines.forEach(
            [this](GraphicsPipelineHandle pipeline_handle, VulkanGraphicsPipelineResource& resource)
            {
                VulkanShaderProgramResource* shader_program = m_shader_programs.get(resource.m_program);
                if (!shader_program)
                {
                    Logger::getInstance().error("Live graphics pipeline references an invalid shader program.");
                    return false;
                }

                const auto graphics_shaders = getGraphicsShaders(*shader_program);
                if (!graphics_shaders[0] || !graphics_shaders[1])
                {
                    Logger::getInstance().error("Live graphics pipeline is missing vertex or fragment shaders.");
                    return false;
                }

                VkFormat color_format = VK_FORMAT_UNDEFINED;
                VkFormat depth_format = VK_FORMAT_UNDEFINED;
                if (!resolvePipelineRenderingFormats(resource.m_desc.render_target, color_format, depth_format))
                {
                    Logger::getInstance().error("Live graphics pipeline references an invalid render target.");
                    return false;
                }

                if (!resource.m_pipeline.initialize(
                        *m_device, m_pipeline_cache, color_format, depth_format,
                        std::span<const Shader* const>(graphics_shaders.data(), graphics_shaders.size()),
                        resource.m_desc))
                {
                    Logger::getInstance().error("Failed to recreate live graphics pipeline.");
                    return false;
                }
                setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_PIPELINE,
                                   (uint64_t)resource.m_pipeline.getVulkanPipeline(),
                                   debugNameOrDefault(resource.m_desc.debug_name, "Kera Graphics Pipeline"));

                if (!reallocateDescriptorSetsForPipeline(pipeline_handle, resource))
                {
                    Logger::getInstance().error("Failed to refresh descriptor sets for recreated live pipeline.");
                    return false;
                }

                return true;
            });
    }

    bool VulkanRenderer::recreateSwapchainFromWindow()
    {
        if (!m_sdl_window)
        {
            Logger::getInstance().error("Cannot recreate Vulkan swapchain without a window.");
            return false;
        }

        int width = static_cast<int>(m_window_extent.width);
        int height = static_cast<int>(m_window_extent.height);
        if (m_window)
        {
            width = m_window->getWidth();
            height = m_window->getHeight();
        }
        if (width <= 0 || height <= 0)
        {
            m_swapchain_recreate_requested = true;
            return true;
        }

        return resize({
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
        });
    }

    bool VulkanRenderer::refreshSwapchainSupport()
    {
        if (!m_surface || !m_physical_device)
        {
            return false;
        }
        return m_physical_device->refreshSwapchainSupport(m_surface->getVulkanSurface());
    }

    bool VulkanRenderer::hasActiveFrames() const
    {
        return m_frames.activeCount() > 0;
    }

    void VulkanRenderer::releaseFrame(FrameHandle frame_handle, uint32_t sync_index)
    {
        if (sync_index < m_active_frame_handles.size() && m_active_frame_handles[sync_index] == frame_handle)
        {
            m_active_frame_handles[sync_index] = {};
        }
        else
        {
            for (FrameHandle& active_frame_handle : m_active_frame_handles)
            {
                if (active_frame_handle == frame_handle)
                {
                    active_frame_handle = {};
                    break;
                }
            }
        }

        m_frames.remove(frame_handle);
    }

    bool VulkanRenderer::waitForTimelineValue(uint64_t timeline_value)
    {
        if (timeline_value == 0)
        {
            return true;
        }
        if (!m_device || m_frame_timeline_semaphore == VK_NULL_HANDLE)
        {
            return false;
        }

        VkSemaphoreWaitInfo wait_info{};
        wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        wait_info.semaphoreCount = 1;
        wait_info.pSemaphores = &m_frame_timeline_semaphore;
        wait_info.pValues = &timeline_value;
        return vkWaitSemaphores(m_device->getVulkanDevice(), &wait_info, UINT64_MAX) == VK_SUCCESS;
    }

    uint64_t VulkanRenderer::reserveTimelineValue()
    {
        return m_next_frame_timeline_value++;
    }

    uint64_t VulkanRenderer::getLastSubmittedTimelineValue() const
    {
        uint64_t timeline_value = 0;
        for (const VulkanFrameSyncResource& frame_sync : m_frame_sync_resources)
        {
            timeline_value = timeline_value < frame_sync.m_timeline_value ? frame_sync.m_timeline_value : timeline_value;
        }
        for (const uint64_t image_timeline_value : m_images_in_flight)
        {
            timeline_value = timeline_value < image_timeline_value ? image_timeline_value : timeline_value;
        }
        return timeline_value;
    }

    bool VulkanRenderer::submitImmediateCommandBuffer(VkCommandBuffer command_buffer, uint64_t& timeline_value)
    {
        if (!m_device || m_frame_timeline_semaphore == VK_NULL_HANDLE || command_buffer == VK_NULL_HANDLE)
        {
            return false;
        }

        timeline_value = reserveTimelineValue();

        VkCommandBufferSubmitInfo command_buffer_info{};
        command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        command_buffer_info.commandBuffer = command_buffer;

        VkSemaphoreSubmitInfo signal_semaphore_info{};
        signal_semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signal_semaphore_info.semaphore = m_frame_timeline_semaphore;
        signal_semaphore_info.value = timeline_value;
        signal_semaphore_info.stageMask = signalStageForTimelineSubmit(VK_PIPELINE_STAGE_2_TRANSFER_BIT);

        VkSubmitInfo2 submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submit_info.commandBufferInfoCount = 1;
        submit_info.pCommandBufferInfos = &command_buffer_info;
        submit_info.signalSemaphoreInfoCount = 1;
        submit_info.pSignalSemaphoreInfos = &signal_semaphore_info;
        return vkQueueSubmit2(m_device->getGraphicsQueue(), 1, &submit_info, VK_NULL_HANDLE) == VK_SUCCESS;
    }

    Buffer VulkanRenderer::acquireStagingBuffer(VkDeviceSize size)
    {
        for (auto buffer_it = m_upload_context.available_staging_buffers.begin();
             buffer_it != m_upload_context.available_staging_buffers.end(); ++buffer_it)
        {
            if (buffer_it->getSize() >= size)
            {
                Buffer buffer = std::move(*buffer_it);
                m_upload_context.available_staging_buffers.erase(buffer_it);
                return buffer;
            }
        }

        Buffer buffer;
        buffer.initialize(*m_device, size, EBufferUsage::TRANSFER_SRC,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        return buffer;
    }

    void VulkanRenderer::releaseStagingBuffer(Buffer&& buffer)
    {
        if (buffer.isValid())
        {
            m_upload_context.available_staging_buffers.push_back(std::move(buffer));
        }
    }

    bool VulkanRenderer::allocateDescriptorSet(const VulkanGraphicsPipelineResource& pipeline, uint32_t set,
                                               VkDescriptorSet& descriptor_set, VkDescriptorPool& descriptor_pool)
    {
        if (!m_device || m_descriptor_pools.empty())
        {
            return false;
        }

        VkDescriptorSetLayout layout = pipeline.m_pipeline.getDescriptorSetLayout(set);
        if (layout == VK_NULL_HANDLE)
        {
            Logger::getInstance().error("Graphics pipeline does not declare the requested descriptor set.");
            return false;
        }

        VkDescriptorSetAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate_info.descriptorSetCount = 1;
        allocate_info.pSetLayouts = &layout;

        VkResult allocate_result = VK_ERROR_OUT_OF_POOL_MEMORY;
        for (VulkanDescriptorPoolResource& pool : m_descriptor_pools)
        {
            allocate_info.descriptorPool = pool.m_pool;
            allocate_result = vkAllocateDescriptorSets(m_device->getVulkanDevice(), &allocate_info, &descriptor_set);
            if (allocate_result == VK_SUCCESS)
            {
                descriptor_pool = pool.m_pool;
                ++pool.m_allocated_sets;
                break;
            }
        }

        if (allocate_result == VK_ERROR_OUT_OF_POOL_MEMORY || allocate_result == VK_ERROR_FRAGMENTED_POOL)
        {
            if (createDescriptorPoolBlock())
            {
                VulkanDescriptorPoolResource& pool = m_descriptor_pools.back();
                allocate_info.descriptorPool = pool.m_pool;
                allocate_result = vkAllocateDescriptorSets(m_device->getVulkanDevice(), &allocate_info, &descriptor_set);
                if (allocate_result == VK_SUCCESS)
                {
                    descriptor_pool = pool.m_pool;
                    ++pool.m_allocated_sets;
                }
            }
        }

        return allocate_result == VK_SUCCESS;
    }

    bool VulkanRenderer::reallocateDescriptorSetsForPipeline(GraphicsPipelineHandle pipeline_handle,
                                                             VulkanGraphicsPipelineResource& pipeline)
    {
        bool success = true;
        m_descriptor_sets.forEach(
            [this, pipeline_handle, &pipeline, &success](DescriptorSetHandle descriptor_set_handle,
                                                        VulkanDescriptorSetResource& descriptor_set)
            {
                if (descriptor_set.m_pipeline != pipeline_handle)
                {
                    return true;
                }

                const std::vector<VulkanDescriptorBindingReference<BufferHandle>> buffers = descriptor_set.m_buffers;
                const std::vector<VulkanDescriptorBindingReference<TextureHandle>> textures = descriptor_set.m_textures;
                const std::vector<VulkanDescriptorBindingReference<SamplerHandle>> samplers = descriptor_set.m_samplers;

                VkDescriptorSet new_descriptor_set = VK_NULL_HANDLE;
                VkDescriptorPool new_descriptor_pool = VK_NULL_HANDLE;
                if (!allocateDescriptorSet(pipeline, descriptor_set.m_set, new_descriptor_set, new_descriptor_pool))
                {
                    Logger::getInstance().error("Failed to reallocate Vulkan descriptor set for recreated pipeline.");
                    success = false;
                    return false;
                }

                VulkanDeferredDeletion deletion{};
                deletion.m_timeline_value = getLastSubmittedTimelineValue();
                VulkanDescriptorSetResource old_descriptor_set{};
                old_descriptor_set.m_descriptor_set = descriptor_set.m_descriptor_set;
                old_descriptor_set.m_descriptor_pool = descriptor_set.m_descriptor_pool;
                deletion.m_descriptor_sets.push_back(std::move(old_descriptor_set));

                descriptor_set.m_descriptor_set = new_descriptor_set;
                descriptor_set.m_descriptor_pool = new_descriptor_pool;
                descriptor_set.m_buffers.clear();
                descriptor_set.m_textures.clear();
                descriptor_set.m_samplers.clear();
                setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_DESCRIPTOR_SET,
                                   (uint64_t)new_descriptor_set,
                                   debugNameOrDefault(descriptor_set.m_debug_name, "Kera Descriptor Set"));

                for (const auto& reference : buffers)
                {
                    if (!updateDescriptorSet(descriptor_set_handle, reference.m_binding, reference.m_handle,
                                             reference.m_offset, reference.m_range))
                    {
                        success = false;
                        return false;
                    }
                }
                for (const auto& reference : textures)
                {
                    if (!updateDescriptorSet(descriptor_set_handle, reference.m_binding, reference.m_handle))
                    {
                        success = false;
                        return false;
                    }
                }
                for (const auto& reference : samplers)
                {
                    if (!updateDescriptorSet(descriptor_set_handle, reference.m_binding, reference.m_handle))
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
        if (deletion.m_timeline_value == 0)
        {
            for (VulkanDescriptorSetResource& descriptor_set : deletion.m_descriptor_sets)
            {
                if (m_device && descriptor_set.m_descriptor_set != VK_NULL_HANDLE &&
                    descriptor_set.m_descriptor_pool != VK_NULL_HANDLE)
                {
                    vkFreeDescriptorSets(m_device->getVulkanDevice(), descriptor_set.m_descriptor_pool, 1,
                                         &descriptor_set.m_descriptor_set);
                    descriptor_set.m_descriptor_set = VK_NULL_HANDLE;
                }
            }
            for (Buffer& buffer : deletion.m_buffers)
            {
                releaseStagingBuffer(std::move(buffer));
            }
            return;
        }
        m_deferred_deletions.push_back(std::move(deletion));
    }

    void VulkanRenderer::collectDeferredDeletions()
    {
        if (!m_device || m_frame_timeline_semaphore == VK_NULL_HANDLE || m_deferred_deletions.empty())
        {
            return;
        }

        uint64_t completed_timeline_value = 0;
        if (vkGetSemaphoreCounterValue(m_device->getVulkanDevice(), m_frame_timeline_semaphore,
                                       &completed_timeline_value) != VK_SUCCESS)
        {
            return;
        }

        auto deletion_it = m_deferred_deletions.begin();
        while (deletion_it != m_deferred_deletions.end())
        {
            if (deletion_it->m_timeline_value <= completed_timeline_value)
            {
                for (VkCommandBuffer command_buffer : deletion_it->m_command_buffers)
                {
                    if (command_buffer != VK_NULL_HANDLE)
                    {
                        vkFreeCommandBuffers(m_device->getVulkanDevice(), m_device->getCommandPool(), 1,
                                             &command_buffer);
                    }
                }
                for (VulkanDescriptorSetResource& descriptor_set : deletion_it->m_descriptor_sets)
                {
                    if (descriptor_set.m_descriptor_set != VK_NULL_HANDLE &&
                        descriptor_set.m_descriptor_pool != VK_NULL_HANDLE)
                    {
                        vkFreeDescriptorSets(m_device->getVulkanDevice(), descriptor_set.m_descriptor_pool, 1,
                                             &descriptor_set.m_descriptor_set);
                        descriptor_set.m_descriptor_set = VK_NULL_HANDLE;
                    }
                }
                for (Buffer& buffer : deletion_it->m_buffers)
                {
                    releaseStagingBuffer(std::move(buffer));
                }
                deletion_it = m_deferred_deletions.erase(deletion_it);
            }
            else
            {
                ++deletion_it;
            }
        }
    }

    void VulkanRenderer::flushDeferredDeletions()
    {
        if (!m_device)
        {
            m_deferred_deletions.clear();
            return;
        }

        for (VulkanDeferredDeletion& deletion : m_deferred_deletions)
        {
            for (VkCommandBuffer command_buffer : deletion.m_command_buffers)
            {
                if (command_buffer != VK_NULL_HANDLE)
                {
                    vkFreeCommandBuffers(m_device->getVulkanDevice(), m_device->getCommandPool(), 1, &command_buffer);
                }
            }
            for (VulkanDescriptorSetResource& descriptor_set : deletion.m_descriptor_sets)
            {
                if (descriptor_set.m_descriptor_set != VK_NULL_HANDLE && descriptor_set.m_descriptor_pool != VK_NULL_HANDLE)
                {
                    vkFreeDescriptorSets(m_device->getVulkanDevice(), descriptor_set.m_descriptor_pool, 1,
                                         &descriptor_set.m_descriptor_set);
                    descriptor_set.m_descriptor_set = VK_NULL_HANDLE;
                }
            }
        }
        m_deferred_deletions.clear();
        m_upload_context.available_staging_buffers.clear();
    }

    void VulkanRenderer::transitionTextureLayout(VkCommandBuffer command_buffer, VulkanTextureResource& texture,
                                                 VkImageLayout new_layout)
    {
        if (command_buffer == VK_NULL_HANDLE || texture.m_image == VK_NULL_HANDLE ||
            texture.m_current_layout == new_layout)
        {
            return;
        }

        VkImageMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask = vulkanStageMaskForImageLayout(texture.m_current_layout);
        barrier.srcAccessMask = vulkanAccessMaskForImageLayout(texture.m_current_layout);
        barrier.dstStageMask = vulkanStageMaskForImageLayout(new_layout);
        barrier.dstAccessMask = vulkanAccessMaskForImageLayout(new_layout);
        barrier.oldLayout = texture.m_current_layout;
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = texture.m_image;
        barrier.subresourceRange.aspectMask = texture.m_aspect_mask;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = texture.m_mip_levels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = texture.m_array_layers;

        VkDependencyInfo dependency_info{};
        dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency_info.imageMemoryBarrierCount = 1;
        dependency_info.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(command_buffer, &dependency_info);

        texture.m_current_layout = new_layout;
    }

    bool VulkanRenderer::generateTextureMipmaps(VkCommandBuffer command_buffer, VulkanTextureResource& texture)
    {
        if (texture.m_mip_levels <= 1)
        {
            return true;
        }
        if (texture.m_current_layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            Logger::getInstance().error("Texture mipmap generation requires transfer-dst layout.");
            return false;
        }

        int32_t mip_width = static_cast<int32_t>(texture.m_extent.width);
        int32_t mip_height = static_cast<int32_t>(texture.m_extent.height);

        for (uint32_t mip_level = 1; mip_level < texture.m_mip_levels; ++mip_level)
        {
            VkImageMemoryBarrier2 source_barrier{};
            source_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            source_barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            source_barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            source_barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            source_barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
            source_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            source_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            source_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            source_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            source_barrier.image = texture.m_image;
            source_barrier.subresourceRange.aspectMask = texture.m_aspect_mask;
            source_barrier.subresourceRange.baseMipLevel = mip_level - 1;
            source_barrier.subresourceRange.levelCount = 1;
            source_barrier.subresourceRange.baseArrayLayer = 0;
            source_barrier.subresourceRange.layerCount = 1;

            VkDependencyInfo source_dependency{};
            source_dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            source_dependency.imageMemoryBarrierCount = 1;
            source_dependency.pImageMemoryBarriers = &source_barrier;
            vkCmdPipelineBarrier2(command_buffer, &source_dependency);

            VkImageBlit blit{};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mip_width, mip_height, 1};
            blit.srcSubresource.aspectMask = texture.m_aspect_mask;
            blit.srcSubresource.mipLevel = mip_level - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {mip_width > 1 ? mip_width / 2 : 1, mip_height > 1 ? mip_height / 2 : 1, 1};
            blit.dstSubresource.aspectMask = texture.m_aspect_mask;
            blit.dstSubresource.mipLevel = mip_level;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(command_buffer, texture.m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture.m_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

            VkImageMemoryBarrier2 read_barrier{};
            read_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            read_barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            read_barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
            read_barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            read_barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
            read_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            read_barrier.newLayout = texture.m_descriptor_layout;
            read_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            read_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            read_barrier.image = texture.m_image;
            read_barrier.subresourceRange.aspectMask = texture.m_aspect_mask;
            read_barrier.subresourceRange.baseMipLevel = mip_level - 1;
            read_barrier.subresourceRange.levelCount = 1;
            read_barrier.subresourceRange.baseArrayLayer = 0;
            read_barrier.subresourceRange.layerCount = 1;

            VkDependencyInfo read_dependency{};
            read_dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            read_dependency.imageMemoryBarrierCount = 1;
            read_dependency.pImageMemoryBarriers = &read_barrier;
            vkCmdPipelineBarrier2(command_buffer, &read_dependency);

            mip_width = mip_width > 1 ? mip_width / 2 : 1;
            mip_height = mip_height > 1 ? mip_height / 2 : 1;
        }

        VkImageMemoryBarrier2 last_barrier{};
        last_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        last_barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        last_barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        last_barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        last_barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
        last_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        last_barrier.newLayout = texture.m_descriptor_layout;
        last_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        last_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        last_barrier.image = texture.m_image;
        last_barrier.subresourceRange.aspectMask = texture.m_aspect_mask;
        last_barrier.subresourceRange.baseMipLevel = texture.m_mip_levels - 1;
        last_barrier.subresourceRange.levelCount = 1;
        last_barrier.subresourceRange.baseArrayLayer = 0;
        last_barrier.subresourceRange.layerCount = 1;

        VkDependencyInfo last_dependency{};
        last_dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        last_dependency.imageMemoryBarrierCount = 1;
        last_dependency.pImageMemoryBarriers = &last_barrier;
        vkCmdPipelineBarrier2(command_buffer, &last_dependency);

        texture.m_current_layout = texture.m_descriptor_layout;
        return true;
    }

    bool VulkanRenderer::recordTextureUpload(VkCommandBuffer command_buffer, PendingTextureUpload& pending_upload,
                                             VulkanTextureResource& texture)
    {
        if (command_buffer == VK_NULL_HANDLE || !pending_upload.staging_buffer.isValid() ||
            texture.m_image == VK_NULL_HANDLE || pending_upload.regions.empty())
        {
            return false;
        }

        beginDebugLabel(m_device->getVulkanDevice(), command_buffer, "Kera Texture Upload", 0.9f, 0.6f, 0.2f);
        transitionTextureLayout(command_buffer, texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        vkCmdCopyBufferToImage(command_buffer, pending_upload.staging_buffer.getVulkanBuffer(), texture.m_image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               static_cast<uint32_t>(pending_upload.regions.size()), pending_upload.regions.data());

        if (pending_upload.generate_mipmaps)
        {
            if (!generateTextureMipmaps(command_buffer, texture))
            {
                endDebugLabel(m_device->getVulkanDevice(), command_buffer);
                Logger::getInstance().error("Failed to generate Vulkan texture mipmaps.");
                return false;
            }
        }
        else
        {
            transitionTextureLayout(command_buffer, texture, texture.m_descriptor_layout);
        }

        endDebugLabel(m_device->getVulkanDevice(), command_buffer);
        return true;
    }

    void VulkanRenderer::clearCompletedFrameResourceUse(uint32_t sync_index)
    {
        if (sync_index >= m_frame_sync_resources.size())
        {
            return;
        }

        VulkanFrameResourceUse& resource_use = m_frame_sync_resources[sync_index].m_resource_use;
        resource_use.m_buffers.clear();
        resource_use.m_textures.clear();
        resource_use.m_samplers.clear();
        resource_use.m_descriptor_sets.clear();
    }

    void VulkanRenderer::recordDescriptorSetUse(uint32_t sync_index, DescriptorSetHandle descriptor_set_handle,
                                                const VulkanDescriptorSetResource& descriptor_set)
    {
        if (sync_index >= m_frame_sync_resources.size())
        {
            return;
        }

        auto add_descriptor_use = [](auto& handles, auto handle)
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

        VulkanFrameResourceUse& resource_use = m_frame_sync_resources[sync_index].m_resource_use;
        add_descriptor_use(resource_use.m_descriptor_sets, descriptor_set_handle);
        for (const auto& reference : descriptor_set.m_buffers)
        {
            add_descriptor_use(resource_use.m_buffers, reference.m_handle);
        }
        for (const auto& reference : descriptor_set.m_textures)
        {
            add_descriptor_use(resource_use.m_textures, reference.m_handle);
        }
        for (const auto& reference : descriptor_set.m_samplers)
        {
            add_descriptor_use(resource_use.m_samplers, reference.m_handle);
        }
    }

    bool VulkanRenderer::frameResourceUses(BufferHandle buffer) const
    {
        if (!buffer.isValid())
        {
            return false;
        }
        for (const VulkanFrameSyncResource& frame_sync : m_frame_sync_resources)
        {
            for (const BufferHandle& used_buffer : frame_sync.m_resource_use.m_buffers)
            {
                if (used_buffer == buffer)
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
        for (const VulkanFrameSyncResource& frame_sync : m_frame_sync_resources)
        {
            for (const TextureHandle& used_texture : frame_sync.m_resource_use.m_textures)
            {
                if (used_texture == texture)
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
        for (const VulkanFrameSyncResource& frame_sync : m_frame_sync_resources)
        {
            for (const SamplerHandle& used_sampler : frame_sync.m_resource_use.m_samplers)
            {
                if (used_sampler == sampler)
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool VulkanRenderer::frameResourceUses(DescriptorSetHandle descriptor_set) const
    {
        if (!descriptor_set.isValid())
        {
            return false;
        }
        for (const VulkanFrameSyncResource& frame_sync : m_frame_sync_resources)
        {
            for (const DescriptorSetHandle& used_descriptor_set : frame_sync.m_resource_use.m_descriptor_sets)
            {
                if (used_descriptor_set == descriptor_set)
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
        m_descriptor_sets.forEach(
            [&referenced, buffer](DescriptorSetHandle, VulkanDescriptorSetResource& descriptor_set)
            {
                for (const auto& reference : descriptor_set.m_buffers)
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
        m_descriptor_sets.forEach(
            [&referenced, texture](DescriptorSetHandle, VulkanDescriptorSetResource& descriptor_set)
            {
                for (const auto& reference : descriptor_set.m_textures)
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
        m_descriptor_sets.forEach(
            [&referenced, sampler](DescriptorSetHandle, VulkanDescriptorSetResource& descriptor_set)
            {
                for (const auto& reference : descriptor_set.m_samplers)
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
        m_render_targets.forEach(
            [&referenced, texture](RenderTargetHandle, VulkanRenderTargetResource& render_target)
            {
                if (render_target.m_color_texture == texture || render_target.m_depth_texture == texture)
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
        for (const DescriptorSetLayoutDesc& layout : pipeline.m_desc.descriptor_sets)
        {
            if (layout.set == set)
            {
                return &layout;
            }
        }
        return nullptr;
    }

    bool VulkanRenderer::validateDescriptorBinding(const VulkanDescriptorSetResource& descriptor_set, uint32_t binding,
                                                   EDescriptorType type) const
    {
        return descriptorBindingAccepts(descriptor_set.m_layout, binding, type);
    }

    RendererValidationReport VulkanRenderer::validateDescriptorSetResource(
        const VulkanDescriptorSetResource& descriptor_set) const
    {
        RendererValidationReport report;
        for (const DescriptorBindingDesc& binding : descriptor_set.m_layout.bindings)
        {
            switch (binding.type)
            {
                case EDescriptorType::UNIFORM_BUFFER:
                {
                    const auto reference =
                        std::find_if(descriptor_set.m_buffers.begin(), descriptor_set.m_buffers.end(),
                                     [&binding](const auto& item) { return item.m_binding == binding.binding; });
                    if (reference == descriptor_set.m_buffers.end())
                    {
                        report.addIssue(ERendererErrorCode::VALIDATION_FAILED, ERendererValidationCategory::DESCRIPTOR,
                                        "Descriptor set " + std::to_string(descriptor_set.m_set) +
                                            " is missing uniform buffer binding '" + binding.name + "'.",
                                        descriptor_set.m_set, binding.binding, binding.name);
                    }
                    else if (!m_buffers.get(reference->m_handle))
                    {
                        report.addIssue(ERendererErrorCode::INVALID_HANDLE, ERendererValidationCategory::DESCRIPTOR,
                                        "Descriptor set " + std::to_string(descriptor_set.m_set) +
                                            " references an invalid uniform buffer for binding '" + binding.name + "'.",
                                        descriptor_set.m_set, binding.binding, binding.name);
                    }
                    break;
                }
                case EDescriptorType::SAMPLED_IMAGE:
                {
                    const auto reference =
                        std::find_if(descriptor_set.m_textures.begin(), descriptor_set.m_textures.end(),
                                     [&binding](const auto& item) { return item.m_binding == binding.binding; });
                    if (reference == descriptor_set.m_textures.end())
                    {
                        report.addIssue(ERendererErrorCode::VALIDATION_FAILED, ERendererValidationCategory::DESCRIPTOR,
                                        "Descriptor set " + std::to_string(descriptor_set.m_set) +
                                            " is missing sampled image binding '" + binding.name + "'.",
                                        descriptor_set.m_set, binding.binding, binding.name);
                    }
                    else if (!m_textures.get(reference->m_handle))
                    {
                        report.addIssue(ERendererErrorCode::INVALID_HANDLE, ERendererValidationCategory::DESCRIPTOR,
                                        "Descriptor set " + std::to_string(descriptor_set.m_set) +
                                            " references an invalid texture for binding '" + binding.name + "'.",
                                        descriptor_set.m_set, binding.binding, binding.name);
                    }
                    break;
                }
                case EDescriptorType::SAMPLER:
                {
                    const auto reference =
                        std::find_if(descriptor_set.m_samplers.begin(), descriptor_set.m_samplers.end(),
                                     [&binding](const auto& item) { return item.m_binding == binding.binding; });
                    if (reference == descriptor_set.m_samplers.end())
                    {
                        report.addIssue(ERendererErrorCode::VALIDATION_FAILED, ERendererValidationCategory::DESCRIPTOR,
                                        "Descriptor set " + std::to_string(descriptor_set.m_set) +
                                            " is missing sampler binding '" + binding.name + "'.",
                                        descriptor_set.m_set, binding.binding, binding.name);
                    }
                    else if (!m_samplers.get(reference->m_handle))
                    {
                        report.addIssue(ERendererErrorCode::INVALID_HANDLE, ERendererValidationCategory::DESCRIPTOR,
                                        "Descriptor set " + std::to_string(descriptor_set.m_set) +
                                            " references an invalid sampler for binding '" + binding.name + "'.",
                                        descriptor_set.m_set, binding.binding, binding.name);
                    }
                    break;
                }
            }
        }

        return report;
    }

    bool VulkanRenderer::resolvePipelineRenderingFormats(RenderTargetHandle render_target, VkFormat& color_format,
                                                         VkFormat& depth_format) const
    {
        if (!render_target.isValid())
        {
            if (!m_swapchain)
            {
                return false;
            }
            color_format = m_swapchain->getImageFormat();
            depth_format = VK_FORMAT_UNDEFINED;
            return true;
        }

        const VulkanRenderTargetResource* resource = m_render_targets.get(render_target);
        if (!resource)
        {
            return false;
        }

        const VulkanTextureResource* color_texture = m_textures.get(resource->m_color_texture);
        if (!color_texture)
        {
            return false;
        }

        color_format = color_texture->m_format;
        depth_format = VK_FORMAT_UNDEFINED;
        if (resource->m_depth_texture.isValid())
        {
            const VulkanTextureResource* depth_texture = m_textures.get(resource->m_depth_texture);
            if (!depth_texture)
            {
                return false;
            }
            depth_format = depth_texture->m_format;
        }
        return true;
    }

    void VulkanRenderer::transitionSwapchainImageLayout(VkCommandBuffer command_buffer, uint32_t image_index,
                                                        VkImageLayout new_layout)
    {
        if (!m_swapchain || command_buffer == VK_NULL_HANDLE || image_index >= m_swapchain->getImages().size() ||
            image_index >= m_swapchain_image_layouts.size() || m_swapchain_image_layouts[image_index] == new_layout)
        {
            return;
        }

        VkImageMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask = vulkanStageMaskForImageLayout(m_swapchain_image_layouts[image_index]);
        barrier.srcAccessMask = vulkanAccessMaskForImageLayout(m_swapchain_image_layouts[image_index]);
        barrier.dstStageMask = vulkanStageMaskForImageLayout(new_layout);
        barrier.dstAccessMask = vulkanAccessMaskForImageLayout(new_layout);
        barrier.oldLayout = m_swapchain_image_layouts[image_index];
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_swapchain->getImages()[image_index];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkDependencyInfo dependency_info{};
        dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency_info.imageMemoryBarrierCount = 1;
        dependency_info.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(command_buffer, &dependency_info);

        m_swapchain_image_layouts[image_index] = new_layout;
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

        VkDevice vk_device = m_device->getVulkanDevice();
        for (VulkanFrameSyncResource& frame_sync : m_frame_sync_resources)
        {
            if (frame_sync.m_render_finished_semaphore != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(vk_device, frame_sync.m_render_finished_semaphore, nullptr);
                frame_sync.m_render_finished_semaphore = VK_NULL_HANDLE;
            }
            if (frame_sync.m_image_available_semaphore != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(vk_device, frame_sync.m_image_available_semaphore, nullptr);
                frame_sync.m_image_available_semaphore = VK_NULL_HANDLE;
            }
            frame_sync.m_timeline_value = 0;
        }
        if (m_frame_timeline_semaphore != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(vk_device, m_frame_timeline_semaphore, nullptr);
            m_frame_timeline_semaphore = VK_NULL_HANDLE;
        }
        m_frame_sync_resources.clear();
        m_images_in_flight.clear();
        m_active_frame_handles.clear();
        m_next_frame_timeline_value = 1;
        m_current_frame_sync_index = 0;
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

        for (VulkanDescriptorPoolResource& pool : m_descriptor_pools)
        {
            if (pool.m_pool != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorPool(m_device->getVulkanDevice(), pool.m_pool, nullptr);
            }
        }
        m_descriptor_pools.clear();
    }

    bool VulkanRenderer::flushUploads()
    {
        auto& pending_upload = m_upload_context.pending_texture_uploads;

        if (pending_upload.empty())
        {
            return true;
        }

        VkCommandBufferAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.commandPool = m_device->getCommandPool();
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = 1;

        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        if (vkAllocateCommandBuffers(m_device->getVulkanDevice(), &allocate_info, &command_buffer) != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to allocate Vulkan flush upload command buffer.");
            return false;
        }

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS)
        {
            vkFreeCommandBuffers(m_device->getVulkanDevice(), m_device->getCommandPool(), 1, &command_buffer);
            Logger::getInstance().error("Failed to begin Vulkan flush upload command buffer.");
            return false;
        }

        struct RecordedTextureLayout
        {
            VulkanTextureResource* texture = nullptr;
            VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        };

        std::vector<RecordedTextureLayout> recorded_layouts;
        recorded_layouts.reserve(pending_upload.size());
        const auto restore_recorded_layouts = [&recorded_layouts]
        {
            for (const RecordedTextureLayout& record : recorded_layouts)
            {
                record.texture->m_current_layout = record.layout;
            }
        };

        for (PendingTextureUpload& upload : pending_upload)
        {
            VulkanTextureResource* texture = m_textures.get(upload.texture);
            if (!texture)
            {
                vkEndCommandBuffer(command_buffer);
                vkFreeCommandBuffers(m_device->getVulkanDevice(), m_device->getCommandPool(), 1, &command_buffer);
                restore_recorded_layouts();

                Logger::getInstance().error("Failed to find Vulkan texture for pending upload.");
                return false;
            }

            const auto recorded_layout_it =
                std::find_if(recorded_layouts.begin(), recorded_layouts.end(),
                             [texture](const RecordedTextureLayout& record) { return record.texture == texture; });

            if (recorded_layout_it == recorded_layouts.end())
            {
                recorded_layouts.push_back({texture, texture->m_current_layout});
            }

            if (!recordTextureUpload(command_buffer, upload, *texture))
            {
                vkEndCommandBuffer(command_buffer);
                vkFreeCommandBuffers(m_device->getVulkanDevice(), m_device->getCommandPool(), 1, &command_buffer);
                restore_recorded_layouts();

                Logger::getInstance().error("Failed to record Vulkan texture upload command.");
                return false;
            }
        }

        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
        {
            vkFreeCommandBuffers(m_device->getVulkanDevice(), m_device->getCommandPool(), 1, &command_buffer);
            restore_recorded_layouts();
            Logger::getInstance().error("Failed to end Vulkan flush upload command buffer.");
            return false;
        }

        uint64_t upload_timeline_value = 0;
        if (!submitImmediateCommandBuffer(command_buffer, upload_timeline_value))
        {
            vkFreeCommandBuffers(m_device->getVulkanDevice(), m_device->getCommandPool(), 1, &command_buffer);
            restore_recorded_layouts();
            Logger::getInstance().error("Failed to submit Vulkan flush upload command buffer.");
            return false;
        }

        VulkanDeferredDeletion deletion{};
        deletion.m_timeline_value = upload_timeline_value;
        deletion.m_command_buffers.push_back(command_buffer);

        for (PendingTextureUpload& upload : pending_upload)
        {
            deletion.m_buffers.push_back(std::move(upload.staging_buffer));
        }

        queueDeferredDeletion(std::move(deletion));
        pending_upload.clear();
        return true;
    }

    void VulkanRenderer::discardPendingUploads()
    {
        for (PendingTextureUpload& upload : m_upload_context.pending_texture_uploads)
        {
            releaseStagingBuffer(std::move(upload.staging_buffer));
        }
        m_upload_context.pending_texture_uploads.clear();
    }

    bool VulkanRenderer::createDescriptorPoolBlock()
    {
        if (!m_device)
        {
            return false;
        }

        constexpr uint32_t kPoolIndexScale = 1;
        std::array<VkDescriptorPoolSize, 3> pool_sizes{};
        pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_sizes[0].descriptorCount = 128 * kPoolIndexScale;
        pool_sizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        pool_sizes[1].descriptorCount = 64 * kPoolIndexScale;
        pool_sizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
        pool_sizes[2].descriptorCount = 64 * kPoolIndexScale;

        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 128 * kPoolIndexScale;
        pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        pool_info.pPoolSizes = pool_sizes.data();

        VulkanDescriptorPoolResource pool{};
        if (vkCreateDescriptorPool(m_device->getVulkanDevice(), &pool_info, nullptr, &pool.m_pool) != VK_SUCCESS)
        {
            return false;
        }

        setDebugObjectName(m_device->getVulkanDevice(), VK_OBJECT_TYPE_DESCRIPTOR_POOL, (uint64_t)pool.m_pool,
                           "Kera Descriptor Pool " + std::to_string(m_descriptor_pools.size()));
        m_descriptor_pools.push_back(pool);
        return true;
    }

    bool VulkanRenderer::createSyncObjects()
    {
        destroySyncObjects();
        if (!m_swapchain)
        {
            return false;
        }

        const uint32_t frame_sync_count = m_command_buffers.size() < kMaxFramesInFlight
                                            ? static_cast<uint32_t>(m_command_buffers.size())
                                            : kMaxFramesInFlight;
        if (frame_sync_count == 0)
        {
            return false;
        }

        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkSemaphoreTypeCreateInfo timeline_semaphore_type_info{};
        timeline_semaphore_type_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timeline_semaphore_type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timeline_semaphore_type_info.initialValue = 0;

        VkSemaphoreCreateInfo timeline_semaphore_info{};
        timeline_semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        timeline_semaphore_info.pNext = &timeline_semaphore_type_info;

        VkDevice vk_device = m_device->getVulkanDevice();
        if (vkCreateSemaphore(vk_device, &timeline_semaphore_info, nullptr, &m_frame_timeline_semaphore) != VK_SUCCESS)
        {
            destroySyncObjects();
            return false;
        }

        m_frame_sync_resources.resize(frame_sync_count);
        m_images_in_flight.assign(m_swapchain->getImageCount(), 0);
        m_active_frame_handles.assign(frame_sync_count, {});

        for (VulkanFrameSyncResource& frame_sync : m_frame_sync_resources)
        {
            frame_sync.m_timeline_value = 0;
            if (vkCreateSemaphore(vk_device, &semaphore_info, nullptr, &frame_sync.m_image_available_semaphore) !=
                VK_SUCCESS)
            {
                destroySyncObjects();
                return false;
            }

            if (vkCreateSemaphore(vk_device, &semaphore_info, nullptr, &frame_sync.m_render_finished_semaphore) !=
                VK_SUCCESS)
            {
                destroySyncObjects();
                return false;
            }
        }

        m_next_frame_timeline_value = 1;
        m_current_frame_sync_index = 0;
        return true;
    }
}  // namespace kera
