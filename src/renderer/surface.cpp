// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/surface.h"

#include "kera/core/window.h"
#include "kera/utilities/logger.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <vulkan/vulkan.h>

namespace kera
{
    Surface::Surface() : m_instance(VK_NULL_HANDLE), m_surface(VK_NULL_HANDLE) {}

    Surface::~Surface()
    {
        destroy();
    }

    Surface::Surface(Surface&& other) noexcept : m_instance(other.m_instance), m_surface(other.m_surface)
    {
        other.m_instance = VK_NULL_HANDLE;
        other.m_surface = VK_NULL_HANDLE;
    }

    Surface& Surface::operator=(Surface&& other) noexcept
    {
        if (this != &other)
        {
            destroy();
            m_instance = other.m_instance;
            m_surface = other.m_surface;

            other.m_instance = VK_NULL_HANDLE;
            other.m_surface = VK_NULL_HANDLE;
        }
        return *this;
    }

    bool Surface::create(VkInstance instance, const Window& window)
    {
        return create(instance, window.getSDLWindow());
    }

    bool Surface::create(VkInstance instance, SDL_Window* window)
    {
        if (m_surface)
        {
            destroy();
        }

        m_instance = instance;

        // Create Vulkan surface from SDL window
        if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &m_surface))
        {
            Logger::getInstance().error("Failed to create Vulkan surface: " + std::string(SDL_GetError()));
            return false;
        }

        Logger::getInstance().debug("Vulkan surface created successfully");
        return true;
    }

    void Surface::destroy()
    {
        if (m_surface && m_instance)
        {
            vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
            m_surface = VK_NULL_HANDLE;
            m_instance = VK_NULL_HANDLE;
            Logger::getInstance().debug("Vulkan surface destroyed");
        }
    }
}  // namespace kera
