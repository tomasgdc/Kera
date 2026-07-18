// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/instance.h"

#include "kera/utilities/logger.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <vulkan/vulkan.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <set>
#include <string>

namespace kera
{

    namespace
    {

        // Validation layer
        const std::vector<const char*> kValidationLayers = {"VK_LAYER_KHRONOS_validation"};

        // Required device extensions
        const std::vector<const char*> kDeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                                                     VkDebugUtilsMessageTypeFlagsEXT message_type,
                                                     const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
                                                     void* p_user_data)
        {
            (void)message_type;
            (void)p_user_data;

            const std::string message = std::string("Vulkan validation: ") +
                                        (p_callback_data && p_callback_data->pMessage ? p_callback_data->pMessage : "");

            if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
            {
                Logger::getInstance().error(message);
            }
            else if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0)
            {
                Logger::getInstance().warning(message);
            }
            else if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0)
            {
                Logger::getInstance().info(message);
            }
            else
            {
                Logger::getInstance().debug(message);
            }
            return VK_FALSE;
        }

        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& create_info)
        {
            create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            create_info.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            create_info.pfnUserCallback = debugCallback;
        }

        VkResult createDebugUtilsMessengerEXT(VkInstance instance,
                                              const VkDebugUtilsMessengerCreateInfoEXT* p_create_info,
                                              const VkAllocationCallbacks* p_allocator,
                                              VkDebugUtilsMessengerEXT* p_debug_messenger)
        {
            auto func =
                (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

            if (func != nullptr)
            {
                return func(instance, p_create_info, p_allocator, p_debug_messenger);
            }
            else
            {
                return VK_ERROR_EXTENSION_NOT_PRESENT;
            }
        }

        void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger,
                                           const VkAllocationCallbacks* p_allocator)
        {
            auto func =
                (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

            if (func != nullptr)
            {
                func(instance, debug_messenger, p_allocator);
            }
        }

        bool checkValidationLayerSupport()
        {
            uint32_t layer_count;
            vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

            std::vector<VkLayerProperties> available_layers(layer_count);
            vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

            for (const char* layer_name : kValidationLayers)
            {
                bool layer_found = false;

                for (const auto& layer_properties : available_layers)
                {
                    if (strcmp(layer_name, layer_properties.layerName) == 0)
                    {
                        layer_found = true;
                        break;
                    }
                }

                if (!layer_found)
                {
                    return false;
                }
            }

            return true;
        }

    }  // anonymous namespace

    Instance::Instance() : m_instance(VK_NULL_HANDLE), m_debug_messenger(VK_NULL_HANDLE), m_validation_enabled(false) {}

    Instance::~Instance()
    {
        shutdown();
    }

    Instance::Instance(Instance&& other) noexcept
        : m_instance(other.m_instance)
        , m_debug_messenger(other.m_debug_messenger)
        , m_validation_enabled(other.m_validation_enabled)
    {
        other.m_instance = VK_NULL_HANDLE;
        other.m_debug_messenger = VK_NULL_HANDLE;
        other.m_validation_enabled = false;
    }

    Instance& Instance::operator=(Instance&& other) noexcept
    {
        if (this != &other)
        {
            shutdown();
            m_instance = other.m_instance;
            m_debug_messenger = other.m_debug_messenger;
            m_validation_enabled = other.m_validation_enabled;

            other.m_instance = VK_NULL_HANDLE;
            other.m_debug_messenger = VK_NULL_HANDLE;
            other.m_validation_enabled = false;
        }
        return *this;
    }

    bool Instance::initialize(const std::string& app_name, uint32_t app_version, bool enable_validation)
    {
        if (m_instance)
        {
            shutdown();
        }

        m_validation_enabled = enable_validation && checkValidationLayerSupport();

        if (enable_validation && !m_validation_enabled)
        {
            Logger::getInstance().warning("Vulkan validation layers requested but not available.");
        }

        if (!createInstance(app_name, app_version))
        {
            return false;
        }

        if (m_validation_enabled && !setupDebugMessenger())
        {
            Logger::getInstance().error("Failed to set up Vulkan debug messenger.");
            shutdown();
            return false;
        }

        Logger::getInstance().debug("Vulkan instance created successfully.");
        if (m_validation_enabled)
        {
            Logger::getInstance().info("Vulkan validation layers enabled.");
        }

        return true;
    }

    void Instance::shutdown()
    {
        if (m_validation_enabled)
        {
            destroyDebugMessenger();
        }

        if (m_instance)
        {
            vkDestroyInstance(m_instance, nullptr);
            m_instance = VK_NULL_HANDLE;
            Logger::getInstance().debug("Vulkan instance destroyed.");
        }
        m_validation_enabled = false;
    }

    std::vector<const char*> Instance::getRequiredExtensions() const
    {
        uint32_t count = 0;

        // SDL3 returns the pointer directly
        const char* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&count);
        if (!sdl_extensions)
        {
            Logger::getInstance().error("Failed to get SDL Vulkan extensions.");
            return {};
        }

        std::vector<const char*> extensions(sdl_extensions, sdl_extensions + count);

        if (m_validation_enabled)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    std::vector<const char*> Instance::getRequiredLayers() const
    {
        if (m_validation_enabled)
        {
            return kValidationLayers;
        }
        return {};
    }

    bool Instance::createInstance(const std::string& app_name, uint32_t app_version)
    {
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = app_name.c_str();
        app_info.applicationVersion = app_version;
        app_info.pEngineName = "Kera";
        app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
        app_info.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &app_info;

        auto extensions = getRequiredExtensions();
        create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        create_info.ppEnabledExtensionNames = extensions.data();

        auto layers = getRequiredLayers();
        create_info.enabledLayerCount = static_cast<uint32_t>(layers.size());
        create_info.ppEnabledLayerNames = layers.data();

        VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
        if (m_validation_enabled)
        {
            populateDebugMessengerCreateInfo(debug_create_info);
            create_info.pNext = &debug_create_info;
        }

        VkResult result = vkCreateInstance(&create_info, nullptr, &m_instance);
        if (result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to create Vulkan instance: " + std::to_string(result));
            return false;
        }

        return true;
    }

    bool Instance::setupDebugMessenger()
    {
        VkDebugUtilsMessengerCreateInfoEXT create_info{};
        populateDebugMessengerCreateInfo(create_info);

        return createDebugUtilsMessengerEXT(m_instance, &create_info, nullptr, &m_debug_messenger) == VK_SUCCESS;
    }

    void Instance::destroyDebugMessenger()
    {
        if (m_debug_messenger && m_instance)
        {
            destroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger, nullptr);
            m_debug_messenger = VK_NULL_HANDLE;
        }
    }

}  // namespace kera
