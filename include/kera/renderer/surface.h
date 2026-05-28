// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vulkan/vulkan.h>

#include <memory>

struct SDL_Window;

namespace kera
{

    class Window;

    class Surface
    {
    public:
        Surface();
        ~Surface();

        // Delete copy operations
        Surface(const Surface&) = delete;
        Surface& operator=(const Surface&) = delete;

        // Move operations
        Surface(Surface&& other) noexcept;
        Surface& operator=(Surface&& other) noexcept;

        bool create(VkInstance instance, const Window& window);
        bool create(VkInstance instance, SDL_Window* window);
        void destroy();

        VkSurfaceKHR getVulkanSurface() const
        {
            return surface_;
        }
        bool isValid() const
        {
            return surface_ != VK_NULL_HANDLE;
        }

    private:
        VkInstance instance_;
        VkSurfaceKHR surface_;
    };

}  // namespace kera
