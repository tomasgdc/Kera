// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/slang_compiler.h"
#include "kera/renderer/slang_reflection.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
    int g_failures = 0;

    void expect(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            ++g_failures;
        }
    }

    std::string readTextFile(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        std::ostringstream stream;
        stream << file.rdbuf();
        return stream.str();
    }

    kera::SlangReflectionMetadata parseMetadata(const std::filesystem::path& path)
    {
        kera::SlangReflectionMetadata metadata;
        std::string diagnostics;
        const std::string json = readTextFile(path);
        expect(!json.empty(), "reflection JSON should be readable");
        expect(kera::parseSlangReflectionMetadata(json, metadata, &diagnostics), "reflection JSON should parse");
        if (!diagnostics.empty())
        {
            std::cerr << diagnostics << '\n';
        }
        return metadata;
    }
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: kera_slang_reflection_metadata_tests <instanced-json> <lighting-json> [shader-root]\n";
        return 1;
    }

    const kera::SlangReflectionMetadata instanced = parseMetadata(argv[1]);
    const kera::SlangReflectionBinding* globalParams = instanced.findBinding("globalParams");
    expect(globalParams != nullptr, "instanced metadata should expose globalParams");
    if (globalParams)
    {
        expect(globalParams->kind == kera::SlangReflectionBindingKind::ParameterBlock,
               "globalParams should be a parameter block");
        expect(globalParams->binding == 0, "globalParams should bind at descriptor binding 0");
        expect(globalParams->uniformSize == 128, "globalParams uniform layout should be 128 bytes");
        expect(globalParams->typeName == "Uniforms", "globalParams should expose Uniforms type name");
    }

    const kera::SlangReflectionEntryPoint* vertexMain = instanced.findEntryPoint("vertexMain");
    expect(vertexMain != nullptr, "instanced metadata should expose vertexMain");
    if (vertexMain)
    {
        expect(vertexMain->stage == kera::ShaderType::Vertex, "vertexMain should be a vertex stage");
        expect(vertexMain->inputs.size() == 3, "vertexMain should expose position, color, and model matrix inputs");
        expect(vertexMain->inputs[0].semanticName == "POSITION", "first vertex input should be POSITION");
        expect(vertexMain->inputs[0].location == 0, "POSITION should use location 0");
        expect(vertexMain->inputs[1].semanticName == "COLOR", "second vertex input should be COLOR");
        expect(vertexMain->inputs[1].location == 1, "COLOR should use location 1");
        expect(vertexMain->inputs[2].semanticName == "TRANSFORM", "third vertex input should be TRANSFORM");
        expect(vertexMain->inputs[2].location == 2, "TRANSFORM should begin at location 2");
        expect(vertexMain->inputs[2].locationCount == 4, "TRANSFORM matrix should span four locations");
    }

    const kera::SlangReflectionMetadata lighting = parseMetadata(argv[2]);
    const kera::SlangReflectionBinding* sceneTexture = lighting.findBinding("sceneTexture");
    expect(sceneTexture != nullptr, "lighting metadata should expose sceneTexture");
    if (sceneTexture)
    {
        expect(sceneTexture->kind == kera::SlangReflectionBindingKind::Resource, "sceneTexture should be a resource");
        expect(sceneTexture->binding == 1, "sceneTexture should bind at descriptor binding 1");
    }

    const kera::SlangReflectionBinding* sceneSampler = lighting.findBinding("sceneSampler");
    expect(sceneSampler != nullptr, "lighting metadata should expose sceneSampler");
    if (sceneSampler)
    {
        expect(sceneSampler->kind == kera::SlangReflectionBindingKind::SamplerState,
               "sceneSampler should be a sampler");
        expect(sceneSampler->binding == 2, "sceneSampler should bind at descriptor binding 2");
    }

    const kera::SlangReflectionBinding* lightingParams = lighting.findBinding("lightingParams");
    expect(lightingParams != nullptr, "lighting metadata should expose lightingParams");
    if (lightingParams)
    {
        expect(lightingParams->kind == kera::SlangReflectionBindingKind::ConstantBuffer,
               "lightingParams should be a constant buffer");
        expect(lightingParams->binding == 3, "lightingParams should bind at descriptor binding 3");
        expect(lightingParams->uniformSize == 2064, "lightingParams uniform layout should be 2064 bytes");
        expect(lightingParams->typeName == "LightingUniforms",
               "lightingParams should expose LightingUniforms type name");
    }

    const kera::SlangReflectionEntryPoint* lightingFragmentMain = lighting.findEntryPoint("lightingFragmentMain");
    expect(lightingFragmentMain != nullptr, "lighting metadata should expose lightingFragmentMain");
    if (lightingFragmentMain)
    {
        expect(lightingFragmentMain->stage == kera::ShaderType::Fragment,
               "lightingFragmentMain should be a fragment stage");
    }

    if (argc >= 4)
    {
        std::vector<uint32_t> spirv;
        kera::SlangReflectionMetadata runtimeReflection;
        std::string diagnostics;
        const std::filesystem::path shaderRoot(argv[3]);
        const bool compiled = kera::SlangCompiler::compileToSpirvAndReflect(
            {
                .shaderPath = (shaderRoot / "instanced_triangle.slang").string(),
                .entryPoint = "vertexMain",
                .shaderType = kera::ShaderType::Vertex,
            },
            spirv, runtimeReflection, &diagnostics);

        expect(compiled, "compileToSpirvAndReflect should compile and reflect in one C++ call");
        expect(!spirv.empty(), "compileToSpirvAndReflect should return SPIR-V");
        expect(runtimeReflection.findBinding("globalParams") != nullptr,
               "compileToSpirvAndReflect should return reflected bindings");
        expect(runtimeReflection.findEntryPoint("vertexMain") != nullptr,
               "compileToSpirvAndReflect should return reflected entry points");
        const kera::SlangReflectionBinding* runtimeGlobalParams = runtimeReflection.findBinding("globalParams");
        if (runtimeGlobalParams)
        {
            expect(runtimeGlobalParams->stage == kera::ShaderType::Vertex,
                   "compileToSpirvAndReflect should tag runtime bindings with their shader stage");
        }

        std::vector<uint32_t> lightingSpirv;
        kera::SlangReflectionMetadata runtimeLightingReflection;
        const bool lightingCompiled = kera::SlangCompiler::compileToSpirvAndReflect(
            {
                .shaderPath = (shaderRoot / "instanced_triangle_many_lights.slang").string(),
                .entryPoint = "lightingFragmentMain",
                .shaderType = kera::ShaderType::Fragment,
            },
            lightingSpirv, runtimeLightingReflection, &diagnostics);

        expect(lightingCompiled, "compileToSpirvAndReflect should compile and reflect the lighting fragment shader");
        const kera::SlangReflectionBinding* runtimeLightingParams =
            runtimeLightingReflection.findBinding("lightingParams");
        expect(runtimeLightingParams != nullptr,
               "compileToSpirvAndReflect should return reflected lighting fragment bindings");
        if (runtimeLightingParams)
        {
            expect(runtimeLightingParams->stage == kera::ShaderType::Fragment,
                   "compileToSpirvAndReflect should tag fragment bindings with the fragment stage");
        }
        if (!diagnostics.empty())
        {
            std::cerr << diagnostics << '\n';
        }
    }

    return g_failures == 0 ? 0 : 1;
}
