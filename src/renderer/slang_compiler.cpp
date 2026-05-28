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
            Slang::ComPtr<slang::IGlobalSession> globalSession;
            Slang::ComPtr<slang::ISession> session;
            Slang::ComPtr<slang::IModule> module;
            Slang::ComPtr<slang::IEntryPoint> entryPoint;
            Slang::ComPtr<slang::IComponentType> compositeProgram;
            Slang::ComPtr<slang::IComponentType> linkedProgram;
        };

        SlangStage shaderTypeToSlangStage(ShaderType type)
        {
            switch (type)
            {
                case ShaderType::Vertex:
                    return SLANG_STAGE_VERTEX;
                case ShaderType::Fragment:
                    return SLANG_STAGE_FRAGMENT;
                case ShaderType::Compute:
                    return SLANG_STAGE_COMPUTE;
                case ShaderType::Geometry:
                    return SLANG_STAGE_GEOMETRY;
                case ShaderType::TessellationControl:
                    return SLANG_STAGE_HULL;
                case ShaderType::TessellationEvaluation:
                    return SLANG_STAGE_DOMAIN;
                default:
                    return SLANG_STAGE_NONE;
            }
        }

        void appendDiagnostics(std::string* outDiagnostics, slang::IBlob* diagnosticsBlob)
        {
            if (!outDiagnostics || !diagnosticsBlob || diagnosticsBlob->getBufferSize() == 0)
            {
                return;
            }

            const auto* text = static_cast<const char*>(diagnosticsBlob->getBufferPointer());
            outDiagnostics->append(text, diagnosticsBlob->getBufferSize());
        }

        std::vector<const char*> buildSearchPathPointers(const SlangCompileRequest& request,
                                                         std::vector<std::string>& ownedSearchPaths)
        {
            ownedSearchPaths = request.searchPaths;

            const std::filesystem::path shaderPath(request.shaderPath);
            const std::string shaderDirectory = shaderPath.has_parent_path() ? shaderPath.parent_path().string()
                                                                             : std::filesystem::current_path().string();

            const bool hasShaderDirectory =
                std::find(ownedSearchPaths.begin(), ownedSearchPaths.end(), shaderDirectory) != ownedSearchPaths.end();

            if (!hasShaderDirectory)
            {
                ownedSearchPaths.push_back(shaderDirectory);
            }

            std::vector<const char*> searchPathPointers;
            searchPathPointers.reserve(ownedSearchPaths.size());
            for (const std::string& path : ownedSearchPaths)
            {
                searchPathPointers.push_back(path.c_str());
            }

            return searchPathPointers;
        }

        bool validateRequest(const SlangCompileRequest& request, const char* operation, std::string* outDiagnostics)
        {
            if (request.shaderPath.empty())
            {
                if (outDiagnostics)
                {
                    *outDiagnostics = "Slang shader path is empty.";
                }
                Logger::getInstance().error(std::string("Slang ") + operation + " failed: shader path is empty.");
                return false;
            }

            if (request.entryPoint.empty())
            {
                if (outDiagnostics)
                {
                    *outDiagnostics = "Slang entry point is empty.";
                }
                Logger::getInstance().error(std::string("Slang ") + operation + " failed: entry point is empty.");
                return false;
            }

            if (!FileUtils::fileExists(request.shaderPath))
            {
                if (outDiagnostics)
                {
                    *outDiagnostics = "Slang shader file not found: " + request.shaderPath;
                }
                Logger::getInstance().error("Slang shader file not found: " + request.shaderPath);
                return false;
            }

            return true;
        }

        bool createLinkedProgram(const SlangCompileRequest& request, SlangLinkedProgramContext& outContext,
                                 std::string* outDiagnostics)
        {
            outContext = {};

            SlangResult result = slang::createGlobalSession(outContext.globalSession.writeRef());
            if (SLANG_FAILED(result))
            {
                if (outDiagnostics)
                {
                    *outDiagnostics = "Failed to create Slang global session.";
                }
                Logger::getInstance().error("Failed to create Slang global session.");
                return false;
            }

            std::vector<std::string> ownedSearchPaths;
            const std::vector<const char*> searchPathPointers = buildSearchPathPointers(request, ownedSearchPaths);

            slang::TargetDesc targetDesc = {};
            targetDesc.structureSize = sizeof(targetDesc);
            targetDesc.format = SLANG_SPIRV;
            targetDesc.profile = outContext.globalSession->findProfile("spirv_1_5");

            slang::SessionDesc sessionDesc = {};
            sessionDesc.structureSize = sizeof(sessionDesc);
            sessionDesc.targets = &targetDesc;
            sessionDesc.targetCount = 1;
            sessionDesc.searchPaths = searchPathPointers.empty() ? nullptr : searchPathPointers.data();
            sessionDesc.searchPathCount = static_cast<SlangInt>(searchPathPointers.size());

            result = outContext.globalSession->createSession(sessionDesc, outContext.session.writeRef());
            if (SLANG_FAILED(result))
            {
                if (outDiagnostics)
                {
                    *outDiagnostics = "Failed to create Slang session.";
                }
                Logger::getInstance().error("Failed to create Slang session.");
                return false;
            }

            const std::filesystem::path shaderPath(request.shaderPath);
            const std::string moduleName = shaderPath.stem().string();
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            slang::IModule* rawModule = outContext.session->loadModule(moduleName.c_str(), diagnosticsBlob.writeRef());
            appendDiagnostics(outDiagnostics, diagnosticsBlob);

            if (!rawModule)
            {
                Logger::getInstance().error("Failed to load Slang module: " + request.shaderPath);
                return false;
            }

            outContext.module = rawModule;
            diagnosticsBlob = nullptr;
            result = outContext.module->findAndCheckEntryPoint(
                request.entryPoint.c_str(), shaderTypeToSlangStage(request.shaderType),
                outContext.entryPoint.writeRef(), diagnosticsBlob.writeRef());
            appendDiagnostics(outDiagnostics, diagnosticsBlob);

            if (SLANG_FAILED(result))
            {
                Logger::getInstance().error("Failed to find Slang entry point '" + request.entryPoint + "' in " +
                                            request.shaderPath);
                return false;
            }

            slang::IComponentType* components[] = {outContext.module.get(), outContext.entryPoint.get()};
            diagnosticsBlob = nullptr;
            result = outContext.session->createCompositeComponentType(
                components, 2, outContext.compositeProgram.writeRef(), diagnosticsBlob.writeRef());
            appendDiagnostics(outDiagnostics, diagnosticsBlob);

            if (SLANG_FAILED(result))
            {
                Logger::getInstance().error("Failed to create Slang composite component type.");
                return false;
            }

            diagnosticsBlob = nullptr;
            result = outContext.compositeProgram->link(outContext.linkedProgram.writeRef(), diagnosticsBlob.writeRef());
            appendDiagnostics(outDiagnostics, diagnosticsBlob);

            if (SLANG_FAILED(result))
            {
                Logger::getInstance().error("Failed to link Slang program.");
                return false;
            }

            return true;
        }

        bool reflectLinkedProgramToJson(slang::IComponentType& linkedProgram, SlangReflectionMetadata& outReflection,
                                        std::string* outDiagnostics)
        {
            outReflection = {};

            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            slang::ProgramLayout* layout = linkedProgram.getLayout(0, diagnosticsBlob.writeRef());
            appendDiagnostics(outDiagnostics, diagnosticsBlob);
            if (!layout)
            {
                if (outDiagnostics)
                {
                    outDiagnostics->append("Failed to get Slang program layout.");
                }
                Logger::getInstance().error("Failed to get Slang program layout.");
                return false;
            }

            Slang::ComPtr<slang::IBlob> jsonBlob;
            const SlangResult result = layout->toJson(jsonBlob.writeRef());
            if (SLANG_FAILED(result) || !jsonBlob)
            {
                if (outDiagnostics)
                {
                    outDiagnostics->append("Failed to serialize Slang reflection JSON.");
                }
                Logger::getInstance().error("Failed to serialize Slang reflection JSON.");
                return false;
            }

            const std::string reflectionJson(static_cast<const char*>(jsonBlob->getBufferPointer()),
                                             jsonBlob->getBufferSize());
            return parseSlangReflectionMetadata(reflectionJson, outReflection, outDiagnostics);
        }

#endif

    }  // namespace

    bool SlangCompiler::compileToSpirv(const SlangCompileRequest& request, std::vector<uint32_t>& outSpirv,
                                       std::string* outDiagnostics)
    {
        outSpirv.clear();

        if (outDiagnostics)
        {
            outDiagnostics->clear();
        }

#if !defined(KERA_HAS_SLANG)
        if (outDiagnostics)
        {
            *outDiagnostics = "Kera was built without Slang support.";
        }
        Logger::getInstance().error("Slang compile requested but KERA_HAS_SLANG is not enabled.");
        return false;
#else
        SlangLinkedProgramContext context;
        if (!validateRequest(request, "compile", outDiagnostics) ||
            !createLinkedProgram(request, context, outDiagnostics))
        {
            return false;
        }

        Slang::ComPtr<slang::IBlob> spirvBlob;
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        SlangResult result =
            context.linkedProgram->getEntryPointCode(0, 0, spirvBlob.writeRef(), diagnosticsBlob.writeRef());
        appendDiagnostics(outDiagnostics, diagnosticsBlob);

        if (SLANG_FAILED(result) || !spirvBlob)
        {
            Logger::getInstance().error("Failed to generate SPIR-V from Slang.");
            return false;
        }

        const size_t byteCount = spirvBlob->getBufferSize();
        if (byteCount % sizeof(uint32_t) != 0)
        {
            if (outDiagnostics)
            {
                outDiagnostics->append("Generated SPIR-V size is not aligned to 32-bit words.");
            }
            Logger::getInstance().error("Generated Slang SPIR-V blob has invalid size.");
            return false;
        }

        outSpirv.resize(byteCount / sizeof(uint32_t));
        std::memcpy(outSpirv.data(), spirvBlob->getBufferPointer(), byteCount);

        Logger::getInstance().debug("Compiled Slang shader at startup: " + request.shaderPath + " [" +
                                    request.entryPoint + "]");
        return true;
#endif
    }

    bool SlangCompiler::compileToSpirvAndReflect(const SlangCompileRequest& request, std::vector<uint32_t>& outSpirv,
                                                 SlangReflectionMetadata& outReflection, std::string* outDiagnostics)
    {
        outSpirv.clear();
        outReflection = {};

        if (outDiagnostics)
        {
            outDiagnostics->clear();
        }

#if !defined(KERA_HAS_SLANG)
        if (outDiagnostics)
        {
            *outDiagnostics = "Kera was built without Slang support.";
        }
        Logger::getInstance().error("Slang compile requested but KERA_HAS_SLANG is not enabled.");
        return false;
#else
        SlangLinkedProgramContext context;
        if (!validateRequest(request, "compile", outDiagnostics) ||
            !createLinkedProgram(request, context, outDiagnostics))
        {
            return false;
        }

        Slang::ComPtr<slang::IBlob> spirvBlob;
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        SlangResult result =
            context.linkedProgram->getEntryPointCode(0, 0, spirvBlob.writeRef(), diagnosticsBlob.writeRef());
        appendDiagnostics(outDiagnostics, diagnosticsBlob);

        if (SLANG_FAILED(result) || !spirvBlob)
        {
            Logger::getInstance().error("Failed to generate SPIR-V from Slang.");
            return false;
        }

        const size_t byteCount = spirvBlob->getBufferSize();
        if (byteCount % sizeof(uint32_t) != 0)
        {
            if (outDiagnostics)
            {
                outDiagnostics->append("Generated SPIR-V size is not aligned to 32-bit words.");
            }
            Logger::getInstance().error("Generated Slang SPIR-V blob has invalid size.");
            return false;
        }

        if (!reflectLinkedProgramToJson(*context.linkedProgram, outReflection, outDiagnostics))
        {
            return false;
        }
        for (SlangReflectionBinding& binding : outReflection.bindings)
        {
            binding.stage = request.shaderType;
        }

        outSpirv.resize(byteCount / sizeof(uint32_t));
        std::memcpy(outSpirv.data(), spirvBlob->getBufferPointer(), byteCount);

        Logger::getInstance().debug("Compiled and reflected Slang shader at startup: " + request.shaderPath + " [" +
                                    request.entryPoint + "]");
        return true;
#endif
    }

}  // namespace kera
