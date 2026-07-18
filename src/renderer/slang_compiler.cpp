// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/slang_compiler.h"

#include "kera/utilities/file_utils.h"
#include "kera/utilities/logger.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>

#if defined(KERA_HAS_SLANG)
#include <slang-com-ptr.h>
#include <slang.h>
#endif

namespace kera
{

    namespace
    {

#if defined(KERA_HAS_SLANG)

        struct SlangLinkedProgramContext
        {
            Slang::ComPtr<slang::IGlobalSession> global_session;
            Slang::ComPtr<slang::ISession> session;
            Slang::ComPtr<slang::IModule> module;
            Slang::ComPtr<slang::IEntryPoint> entry_point;
            Slang::ComPtr<slang::IComponentType> composite_program;
            Slang::ComPtr<slang::IComponentType> linked_program;
        };

        SlangStage shaderTypeToSlangStage(EShaderType type)
        {
            switch (type)
            {
                case EShaderType::VERTEX:
                    return SLANG_STAGE_VERTEX;
                case EShaderType::FRAGMENT:
                    return SLANG_STAGE_FRAGMENT;
                case EShaderType::COMPUTE:
                    return SLANG_STAGE_COMPUTE;
                case EShaderType::GEOMETRY:
                    return SLANG_STAGE_GEOMETRY;
                case EShaderType::TESSELLATION_CONTROL:
                    return SLANG_STAGE_HULL;
                case EShaderType::TESSELLATION_EVALUATION:
                    return SLANG_STAGE_DOMAIN;
                default:
                    return SLANG_STAGE_NONE;
            }
        }

        void appendDiagnostics(std::string* out_diagnostics, slang::IBlob* diagnostics_blob)
        {
            if (!out_diagnostics || !diagnostics_blob || diagnostics_blob->getBufferSize() == 0)
            {
                return;
            }

            const auto* text = static_cast<const char*>(diagnostics_blob->getBufferPointer());
            out_diagnostics->append(text, diagnostics_blob->getBufferSize());
        }

        std::vector<const char*> buildSearchPathPointers(const SlangCompileRequest& request,
                                                         std::vector<std::string>& owned_search_paths)
        {
            owned_search_paths = request.search_paths;

            const std::filesystem::path shader_path(request.shader_path);
            const std::string shader_directory = shader_path.has_parent_path() ? shader_path.parent_path().string()
                                                                             : std::filesystem::current_path().string();

            const bool has_shader_directory =
                std::find(owned_search_paths.begin(), owned_search_paths.end(), shader_directory) != owned_search_paths.end();

            if (!has_shader_directory)
            {
                owned_search_paths.push_back(shader_directory);
            }

            std::vector<const char*> search_path_pointers;
            search_path_pointers.reserve(owned_search_paths.size());
            for (const std::string& path : owned_search_paths)
            {
                search_path_pointers.push_back(path.c_str());
            }

            return search_path_pointers;
        }

        bool validateRequest(const SlangCompileRequest& request, const char* operation, std::string* out_diagnostics)
        {
            if (request.shader_path.empty())
            {
                if (out_diagnostics)
                {
                    *out_diagnostics = "Slang shader path is empty.";
                }
                Logger::getInstance().error(std::string("Slang ") + operation + " failed: shader path is empty.");
                return false;
            }

            if (request.entry_point.empty())
            {
                if (out_diagnostics)
                {
                    *out_diagnostics = "Slang entry point is empty.";
                }
                Logger::getInstance().error(std::string("Slang ") + operation + " failed: entry point is empty.");
                return false;
            }

            if (!FileUtils::fileExists(request.shader_path))
            {
                if (out_diagnostics)
                {
                    *out_diagnostics = "Slang shader file not found: " + request.shader_path;
                }
                Logger::getInstance().error("Slang shader file not found: " + request.shader_path);
                return false;
            }

            return true;
        }

        bool createLinkedProgram(const SlangCompileRequest& request, SlangLinkedProgramContext& out_context,
                                 std::string* out_diagnostics)
        {
            out_context = {};

            SlangResult result = slang::createGlobalSession(out_context.global_session.writeRef());
            if (SLANG_FAILED(result))
            {
                if (out_diagnostics)
                {
                    *out_diagnostics = "Failed to create Slang global session.";
                }
                Logger::getInstance().error("Failed to create Slang global session.");
                return false;
            }

            std::vector<std::string> owned_search_paths;
            const std::vector<const char*> search_path_pointers = buildSearchPathPointers(request, owned_search_paths);

            slang::TargetDesc target_desc = {};
            target_desc.structureSize = sizeof(target_desc);
            target_desc.format = SLANG_SPIRV;
            target_desc.profile = out_context.global_session->findProfile("spirv_1_5");

            slang::SessionDesc session_desc = {};
            session_desc.structureSize = sizeof(session_desc);
            session_desc.targets = &target_desc;
            session_desc.targetCount = 1;
            session_desc.searchPaths = search_path_pointers.empty() ? nullptr : search_path_pointers.data();
            session_desc.searchPathCount = static_cast<SlangInt>(search_path_pointers.size());

            result = out_context.global_session->createSession(session_desc, out_context.session.writeRef());
            if (SLANG_FAILED(result))
            {
                if (out_diagnostics)
                {
                    *out_diagnostics = "Failed to create Slang session.";
                }
                Logger::getInstance().error("Failed to create Slang session.");
                return false;
            }

            const std::filesystem::path shader_path(request.shader_path);
            const std::string module_name = shader_path.stem().string();
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            slang::IModule* raw_module = out_context.session->loadModule(module_name.c_str(), diagnostics_blob.writeRef());
            appendDiagnostics(out_diagnostics, diagnostics_blob);

            if (!raw_module)
            {
                Logger::getInstance().error("Failed to load Slang module: " + request.shader_path);
                return false;
            }

            out_context.module = raw_module;
            diagnostics_blob = nullptr;
            result = out_context.module->findAndCheckEntryPoint(
                request.entry_point.c_str(), shaderTypeToSlangStage(request.shader_type),
                out_context.entry_point.writeRef(), diagnostics_blob.writeRef());
            appendDiagnostics(out_diagnostics, diagnostics_blob);

            if (SLANG_FAILED(result))
            {
                Logger::getInstance().error("Failed to find Slang entry point '" + request.entry_point + "' in " +
                                            request.shader_path);
                return false;
            }

            slang::IComponentType* components[] = {out_context.module.get(), out_context.entry_point.get()};
            diagnostics_blob = nullptr;
            result = out_context.session->createCompositeComponentType(
                components, 2, out_context.composite_program.writeRef(), diagnostics_blob.writeRef());
            appendDiagnostics(out_diagnostics, diagnostics_blob);

            if (SLANG_FAILED(result))
            {
                Logger::getInstance().error("Failed to create Slang composite component type.");
                return false;
            }

            diagnostics_blob = nullptr;
            result = out_context.composite_program->link(out_context.linked_program.writeRef(), diagnostics_blob.writeRef());
            appendDiagnostics(out_diagnostics, diagnostics_blob);

            if (SLANG_FAILED(result))
            {
                Logger::getInstance().error("Failed to link Slang program.");
                return false;
            }

            return true;
        }

        bool reflectLinkedProgramToJson(slang::IComponentType& linked_program, SlangReflectionMetadata& out_reflection,
                                        std::string* out_diagnostics)
        {
            out_reflection = {};

            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            slang::ProgramLayout* layout = linked_program.getLayout(0, diagnostics_blob.writeRef());
            appendDiagnostics(out_diagnostics, diagnostics_blob);
            if (!layout)
            {
                if (out_diagnostics)
                {
                    out_diagnostics->append("Failed to get Slang program layout.");
                }
                Logger::getInstance().error("Failed to get Slang program layout.");
                return false;
            }

            Slang::ComPtr<slang::IBlob> json_blob;
            const SlangResult result = layout->toJson(json_blob.writeRef());
            if (SLANG_FAILED(result) || !json_blob)
            {
                if (out_diagnostics)
                {
                    out_diagnostics->append("Failed to serialize Slang reflection JSON.");
                }
                Logger::getInstance().error("Failed to serialize Slang reflection JSON.");
                return false;
            }

            const std::string reflection_json(static_cast<const char*>(json_blob->getBufferPointer()),
                                             json_blob->getBufferSize());
            return parseSlangReflectionMetadata(reflection_json, out_reflection, out_diagnostics);
        }

#endif

    }  // namespace

    bool SlangCompiler::compileToSpirv(const SlangCompileRequest& request, std::vector<uint32_t>& out_spirv,
                                       std::string* out_diagnostics)
    {
        out_spirv.clear();

        if (out_diagnostics)
        {
            out_diagnostics->clear();
        }

#if !defined(KERA_HAS_SLANG)
        if (out_diagnostics)
        {
            *out_diagnostics = "Kera was built without Slang support.";
        }
        Logger::getInstance().error("Slang compile requested but KERA_HAS_SLANG is not enabled.");
        return false;
#else
        SlangLinkedProgramContext context;
        if (!validateRequest(request, "compile", out_diagnostics) ||
            !createLinkedProgram(request, context, out_diagnostics))
        {
            return false;
        }

        Slang::ComPtr<slang::IBlob> spirv_blob;
        Slang::ComPtr<slang::IBlob> diagnostics_blob;
        SlangResult result =
            context.linked_program->getEntryPointCode(0, 0, spirv_blob.writeRef(), diagnostics_blob.writeRef());
        appendDiagnostics(out_diagnostics, diagnostics_blob);

        if (SLANG_FAILED(result) || !spirv_blob)
        {
            Logger::getInstance().error("Failed to generate SPIR-V from Slang.");
            return false;
        }

        const size_t byte_count = spirv_blob->getBufferSize();
        if (byte_count % sizeof(uint32_t) != 0)
        {
            if (out_diagnostics)
            {
                out_diagnostics->append("Generated SPIR-V size is not aligned to 32-bit words.");
            }
            Logger::getInstance().error("Generated Slang SPIR-V blob has invalid size.");
            return false;
        }

        out_spirv.resize(byte_count / sizeof(uint32_t));
        std::memcpy(out_spirv.data(), spirv_blob->getBufferPointer(), byte_count);

        Logger::getInstance().debug("Compiled Slang shader at startup: " + request.shader_path + " [" +
                                    request.entry_point + "]");
        return true;
#endif
    }

    bool SlangCompiler::compileToSpirvAndReflect(const SlangCompileRequest& request, std::vector<uint32_t>& out_spirv,
                                                 SlangReflectionMetadata& out_reflection, std::string* out_diagnostics)
    {
        out_spirv.clear();
        out_reflection = {};

        if (out_diagnostics)
        {
            out_diagnostics->clear();
        }

#if !defined(KERA_HAS_SLANG)
        if (out_diagnostics)
        {
            *out_diagnostics = "Kera was built without Slang support.";
        }
        Logger::getInstance().error("Slang compile requested but KERA_HAS_SLANG is not enabled.");
        return false;
#else
        SlangLinkedProgramContext context;
        if (!validateRequest(request, "compile", out_diagnostics) ||
            !createLinkedProgram(request, context, out_diagnostics))
        {
            return false;
        }

        Slang::ComPtr<slang::IBlob> spirv_blob;
        Slang::ComPtr<slang::IBlob> diagnostics_blob;
        SlangResult result =
            context.linked_program->getEntryPointCode(0, 0, spirv_blob.writeRef(), diagnostics_blob.writeRef());
        appendDiagnostics(out_diagnostics, diagnostics_blob);

        if (SLANG_FAILED(result) || !spirv_blob)
        {
            Logger::getInstance().error("Failed to generate SPIR-V from Slang.");
            return false;
        }

        const size_t byte_count = spirv_blob->getBufferSize();
        if (byte_count % sizeof(uint32_t) != 0)
        {
            if (out_diagnostics)
            {
                out_diagnostics->append("Generated SPIR-V size is not aligned to 32-bit words.");
            }
            Logger::getInstance().error("Generated Slang SPIR-V blob has invalid size.");
            return false;
        }

        if (!reflectLinkedProgramToJson(*context.linked_program, out_reflection, out_diagnostics))
        {
            return false;
        }

        for (SlangReflectionBinding& binding : out_reflection.bindings)
        {
            binding.stage = request.shader_type;
        }

        out_spirv.resize(byte_count / sizeof(uint32_t));
        std::memcpy(out_spirv.data(), spirv_blob->getBufferPointer(), byte_count);

        Logger::getInstance().debug("Compiled and reflected Slang shader at startup: " + request.shader_path + " [" +
                                    request.entry_point + "]");
        return true;
#endif
    }

}  // namespace kera
