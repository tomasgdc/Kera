// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <vector>

namespace kera
{

    enum class EShaderType
    {
        VERTEX,
        FRAGMENT,
        COMPUTE,
        GEOMETRY,
        TESSELLATION_CONTROL,
        TESSELLATION_EVALUATION
    };

    class Device;

    class Shader
    {
    public:
        Shader();
        ~Shader();

        // Delete copy operations
        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;

        // Move operations
        Shader(Shader&& other) noexcept;
        Shader& operator=(Shader&& other) noexcept;

        bool initialize(const Device& device, EShaderType type, const std::vector<uint32_t>& spirv_code);
        bool initializeFromFile(const Device& device, EShaderType type, const std::string& filepath);
        bool initializeFromSlangFile(const Device& device, EShaderType type, const std::string& shader_path,
                                     const std::string& entry_point, const std::vector<std::string>& search_paths = {});
        void shutdown();

        VkShaderModule getVulkanShaderModule() const
        {
            return m_shader_module;
        }
        EShaderType getType() const
        {
            return m_type;
        }

        bool isValid() const
        {
            return m_shader_module != VK_NULL_HANDLE;
        }

        // Shader stage info for pipeline creation
        VkPipelineShaderStageCreateInfo getPipelineStageInfo() const;

    private:
        VkDevice m_device;
        VkShaderModule m_shader_module;
        EShaderType m_type;
    };

}  // namespace kera
