// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/shader.h"

#include "kera/renderer/device.h"
#include "kera/renderer/slang_compiler.h"
#include "kera/utilities/logger.h"

#include <vulkan/vulkan.h>

#include <fstream>

namespace kera
{

    namespace
    {

        VkShaderStageFlagBits shaderTypeToStageFlag(EShaderType type)
        {
            switch (type)
            {
                case EShaderType::VERTEX:
                    return VK_SHADER_STAGE_VERTEX_BIT;
                case EShaderType::FRAGMENT:
                    return VK_SHADER_STAGE_FRAGMENT_BIT;
                case EShaderType::COMPUTE:
                    return VK_SHADER_STAGE_COMPUTE_BIT;
                case EShaderType::GEOMETRY:
                    return VK_SHADER_STAGE_GEOMETRY_BIT;
                case EShaderType::TESSELLATION_CONTROL:
                    return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
                case EShaderType::TESSELLATION_EVALUATION:
                    return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
                default:
                    return VK_SHADER_STAGE_VERTEX_BIT;
            }
        }

    }  // anonymous namespace

    Shader::Shader() : m_device(VK_NULL_HANDLE), m_shader_module(VK_NULL_HANDLE), m_type(EShaderType::VERTEX) {}

    Shader::~Shader()
    {
        shutdown();
    }

    Shader::Shader(Shader&& other) noexcept
        : m_device(other.m_device), m_shader_module(other.m_shader_module), m_type(other.m_type)
    {
        other.m_device = VK_NULL_HANDLE;
        other.m_shader_module = VK_NULL_HANDLE;
        other.m_type = EShaderType::VERTEX;
    }

    Shader& Shader::operator=(Shader&& other) noexcept
    {
        if (this != &other)
        {
            shutdown();
            m_device = other.m_device;
            m_shader_module = other.m_shader_module;
            m_type = other.m_type;

            other.m_device = VK_NULL_HANDLE;
            other.m_shader_module = VK_NULL_HANDLE;
            other.m_type = EShaderType::VERTEX;
        }
        return *this;
    }

    bool Shader::initialize(const Device& device, EShaderType type, const std::vector<uint32_t>& spirv_code)
    {
        if (m_shader_module)
        {
            shutdown();
        }

        VkDevice vk_device = device.getVulkanDevice();
        m_device = vk_device;
        m_type = type;

        VkShaderModuleCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = spirv_code.size() * sizeof(uint32_t);
        create_info.pCode = spirv_code.data();

        VkResult result = vkCreateShaderModule(vk_device, &create_info, nullptr, &m_shader_module);
        if (result != VK_SUCCESS)
        {
            Logger::getInstance().error("Failed to create shader module: " + std::to_string(result));
            return false;
        }

        Logger::getInstance().debug("Shader module created successfully");
        return true;
    }

    bool Shader::initializeFromFile(const Device& device, EShaderType type, const std::string& filepath)
    {
        std::ifstream file(filepath, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            Logger::getInstance().error("Failed to open shader file: " + filepath);
            return false;
        }

        size_t file_size = static_cast<size_t>(file.tellg());
        std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), file_size);
        file.close();

        return initialize(device, type, buffer);
    }

    bool Shader::initializeFromSlangFile(const Device& device, EShaderType type, const std::string& shader_path,
                                         const std::string& entry_point, const std::vector<std::string>& search_paths)
    {
        std::vector<uint32_t> spirv_code;
        SlangCompileRequest request{
            .shader_path = shader_path,
            .entry_point = entry_point,
            .shader_type = type,
            .search_paths = search_paths,
        };

        if (!SlangCompiler::compileToSpirv(request, spirv_code))
        {
            return false;
        }

        return initialize(device, type, spirv_code);
    }

    void Shader::shutdown()
    {
        if (m_shader_module != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(m_device, m_shader_module, nullptr);
            m_shader_module = VK_NULL_HANDLE;
        }
        m_device = VK_NULL_HANDLE;
    }

    VkPipelineShaderStageCreateInfo Shader::getPipelineStageInfo() const
    {
        VkPipelineShaderStageCreateInfo shader_stage_info{};
        shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_info.stage = shaderTypeToStageFlag(m_type);
        shader_stage_info.module = m_shader_module;
        shader_stage_info.pName = "main";

        return shader_stage_info;
    }

}  // namespace kera
