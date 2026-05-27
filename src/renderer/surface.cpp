#include "kera/renderer/surface.h"

#include "kera/core/window.h"
#include "kera/utilities/logger.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <vulkan/vulkan.h>

namespace kera
{
    Surface::Surface() : instance_(VK_NULL_HANDLE), surface_(VK_NULL_HANDLE) {}

    Surface::~Surface()
    {
        destroy();
    }

    Surface::Surface(Surface&& other) noexcept : instance_(other.instance_), surface_(other.surface_)
    {
        other.instance_ = VK_NULL_HANDLE;
        other.surface_ = VK_NULL_HANDLE;
    }

    Surface& Surface::operator=(Surface&& other) noexcept
    {
        if (this != &other)
        {
            destroy();
            instance_ = other.instance_;
            surface_ = other.surface_;

            other.instance_ = VK_NULL_HANDLE;
            other.surface_ = VK_NULL_HANDLE;
        }
        return *this;
    }

    bool Surface::create(VkInstance instance, const Window& window)
    {
        if (surface_)
        {
            destroy();
        }

        instance_ = instance;

        // Create Vulkan surface from SDL window
        if (!SDL_Vulkan_CreateSurface(window.getSDLWindow(), instance, nullptr, &surface_))
        {
            Logger::getInstance().error("Failed to create Vulkan surface: " + std::string(SDL_GetError()));
            return false;
        }

        Logger::getInstance().debug("Vulkan surface created successfully");
        return true;
    }

    void Surface::destroy()
    {
        if (surface_ && instance_)
        {
            vkDestroySurfaceKHR(instance_, surface_, nullptr);
            surface_ = VK_NULL_HANDLE;
            instance_ = VK_NULL_HANDLE;
            Logger::getInstance().debug("Vulkan surface destroyed");
        }
    }
}  // namespace kera
