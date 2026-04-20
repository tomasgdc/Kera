#include "kera/renderer/instance.h"
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <iostream>
#include <algorithm>
#include <set>

namespace kera {

namespace {

// Validation layer
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// Required device extensions
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    (void)messageSeverity;
    (void)messageType;
    (void)pUserData;

    std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

VkResult createDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger) {

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");

    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void destroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator) {

    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");

    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

bool checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

} // anonymous namespace

Instance::Instance()
    : instance_(VK_NULL_HANDLE)
    , debug_messenger_(VK_NULL_HANDLE)
    , validation_enabled_(false) {
}

Instance::~Instance() {
    shutdown();
}

Instance::Instance(Instance&& other) noexcept
    : instance_(other.instance_)
    , debug_messenger_(other.debug_messenger_)
    , validation_enabled_(other.validation_enabled_) {
    other.instance_ = VK_NULL_HANDLE;
    other.debug_messenger_ = VK_NULL_HANDLE;
    other.validation_enabled_ = false;
}

Instance& Instance::operator=(Instance&& other) noexcept {
    if (this != &other) {
        shutdown();
        instance_ = other.instance_;
        debug_messenger_ = other.debug_messenger_;
        validation_enabled_ = other.validation_enabled_;

        other.instance_ = VK_NULL_HANDLE;
        other.debug_messenger_ = VK_NULL_HANDLE;
        other.validation_enabled_ = false;
    }
    return *this;
}

bool Instance::initialize(const std::string& appName, uint32_t appVersion, bool enableValidation) {
    if (instance_) {
        shutdown();
    }

    validation_enabled_ = enableValidation && checkValidationLayerSupport();

    if (enableValidation && !validation_enabled_) {
        std::cout << "Validation layers requested but not available" << std::endl;
    }

    if (!createInstance(appName, appVersion)) {
        return false;
    }

    if (validation_enabled_ && !setupDebugMessenger()) {
        std::cerr << "Failed to set up debug messenger" << std::endl;
        shutdown();
        return false;
    }

    std::cout << "Vulkan instance created successfully" << std::endl;
    if (validation_enabled_) {
        std::cout << "Validation layers enabled" << std::endl;
    }

    return true;
}

void Instance::shutdown() {
    if (validation_enabled_) {
        destroyDebugMessenger();
    }

    if (instance_) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
        std::cout << "Vulkan instance destroyed" << std::endl;
    }
}

std::vector<const char*> Instance::getRequiredExtensions() const {
    uint32_t count = 0;

    // SDL3 returns the pointer directly
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&count);
    if (!sdlExtensions) {
        std::cerr << "Failed to get SDL Vulkan extensions" << std::endl;
        return {};
    }

    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + count);

    if (validation_enabled_) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

std::vector<const char*> Instance::getRequiredLayers() const {
    if (validation_enabled_) {
        return validationLayers;
    }
    return {};
}

bool Instance::createInstance(const std::string& appName, uint32_t appVersion) {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = appName.c_str();
    appInfo.applicationVersion = appVersion;
    appInfo.pEngineName = "Kera";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    auto layers = getRequiredLayers();
    createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames = layers.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (validation_enabled_) {
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;

        createInfo.pNext = &debugCreateInfo;
    }

    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance: " << result << std::endl;
        return false;
    }

    return true;
}

bool Instance::setupDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;

    return createDebugUtilsMessengerEXT(instance_, &createInfo, nullptr, &debug_messenger_) == VK_SUCCESS;
}

void Instance::destroyDebugMessenger() {
    if (debug_messenger_ && instance_) {
        destroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
        debug_messenger_ = VK_NULL_HANDLE;
    }
}

} // namespace kera