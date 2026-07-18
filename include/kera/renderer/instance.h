// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <vector>

namespace kera
{

    class Instance
    {
    public:
        Instance();
        ~Instance();

        // Delete copy operations
        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;

        // Move operations
        Instance(Instance&& other) noexcept;
        Instance& operator=(Instance&& other) noexcept;

        bool initialize(const std::string& app_name, uint32_t app_version, bool enable_validation = true);
        void shutdown();

        VkInstance getVulkanInstance() const
        {
            return m_instance;
        }
        bool isValid() const
        {
            return m_instance != VK_NULL_HANDLE;
        }

        // Extension and layer queries
        std::vector<const char*> getRequiredExtensions() const;
        std::vector<const char*> getRequiredLayers() const;

        bool isValidationEnabled() const
        {
            return m_validation_enabled;
        }
        bool isDebugMessengerActive() const
        {
            return m_debug_messenger != VK_NULL_HANDLE;
        }

    private:
        bool createInstance(const std::string& app_name, uint32_t app_version);
        bool setupDebugMessenger();
        void destroyDebugMessenger();

        VkInstance m_instance;
        VkDebugUtilsMessengerEXT m_debug_messenger;
        bool m_validation_enabled;
    };

}  // namespace kera
