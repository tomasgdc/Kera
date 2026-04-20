#include "kera/renderer/surface.h"
#include "kera/core/window.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <iostream>

namespace kera {

Surface::Surface()
    : instance_(VK_NULL_HANDLE)
    , surface_(VK_NULL_HANDLE) {
}

Surface::~Surface() {
    destroy();
}

Surface::Surface(Surface&& other) noexcept
    : instance_(other.instance_)
    , surface_(other.surface_) {
    other.instance_ = VK_NULL_HANDLE;
    other.surface_ = VK_NULL_HANDLE;
}

Surface& Surface::operator=(Surface&& other) noexcept {
    if (this != &other) {
        destroy();
        instance_ = other.instance_;
        surface_ = other.surface_;

        other.instance_ = VK_NULL_HANDLE;
        other.surface_ = VK_NULL_HANDLE;
    }
    return *this;
}

bool Surface::create(VkInstance instance, const Window& window) {
    if (surface_) {
        destroy();
    }

    instance_ = instance;

    // Create Vulkan surface from SDL window
    if (!SDL_Vulkan_CreateSurface(window.getSDLWindow(), instance, nullptr, &surface_)) {
        std::cerr << "Failed to create Vulkan surface: " << SDL_GetError() << std::endl;
        return false;
    }

    std::cout << "Vulkan surface created successfully" << std::endl;
    return true;
}

void Surface::destroy() {
    if (surface_ && instance_) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
        instance_ = VK_NULL_HANDLE;
        std::cout << "Vulkan surface destroyed" << std::endl;
    }
}

} // namespace kera