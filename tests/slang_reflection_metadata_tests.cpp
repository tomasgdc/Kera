// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/slang_compiler.h"
#include "kera/renderer/slang_reflection.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
#if defined(_CPPUNWIND) || defined(__EXCEPTIONS)
#error "Kera tests must compile with C++ exceptions disabled."
#endif

#if defined(_MSC_VER)
    static_assert(_HAS_EXCEPTIONS == 0, "Kera MSVC builds must compile the STL with exceptions disabled.");
#endif

    const char* getEnv(const char* name)
    {
        return std::getenv(name);
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
        EXPECT_FALSE(json.empty()) << "Reflection JSON should be readable: " << path.string();
        EXPECT_TRUE(kera::parseSlangReflectionMetadata(json, metadata, &diagnostics))
            << "Reflection JSON should parse: " << path.string() << '\n'
            << diagnostics;
        return metadata;
    }
}  // namespace

TEST(KeraSlangReflectionMetadata, GeneratedJsonContracts)
{
    const char* instancedJson = getEnv("KERA_REFLECTION_INSTANCED_JSON");
    const char* lightingJson = getEnv("KERA_REFLECTION_LIGHTING_JSON");
    const char* shaderRootEnv = getEnv("KERA_REFLECTION_SHADER_ROOT");
    if (instancedJson == nullptr || lightingJson == nullptr || shaderRootEnv == nullptr)
    {
        GTEST_SKIP() << "Generated Slang reflection JSON paths are provided by kera_slang_reflection_tests.";
    }

    const kera::SlangReflectionMetadata instanced = parseMetadata(instancedJson);
    const kera::SlangReflectionBinding* globalParams = instanced.findBinding("globalParams");
    ASSERT_NE(globalParams, nullptr);
    EXPECT_EQ(globalParams->kind, kera::SlangReflectionBindingKind::ParameterBlock);
    EXPECT_EQ(globalParams->binding, 0u);
    EXPECT_EQ(globalParams->uniformSize, 128u);
    EXPECT_EQ(globalParams->typeName, "Uniforms");

    const kera::SlangReflectionEntryPoint* vertexMain = instanced.findEntryPoint("vertexMain");
    ASSERT_NE(vertexMain, nullptr);
    EXPECT_EQ(vertexMain->stage, kera::ShaderType::Vertex);
    ASSERT_EQ(vertexMain->inputs.size(), 3u);
    EXPECT_EQ(vertexMain->inputs[0].semanticName, "POSITION");
    EXPECT_EQ(vertexMain->inputs[0].location, 0u);
    EXPECT_EQ(vertexMain->inputs[1].semanticName, "COLOR");
    EXPECT_EQ(vertexMain->inputs[1].location, 1u);
    EXPECT_EQ(vertexMain->inputs[2].semanticName, "TRANSFORM");
    EXPECT_EQ(vertexMain->inputs[2].location, 2u);
    EXPECT_EQ(vertexMain->inputs[2].locationCount, 4u);

    const kera::SlangReflectionMetadata lighting = parseMetadata(lightingJson);
    const kera::SlangReflectionBinding* sceneTexture = lighting.findBinding("sceneTexture");
    ASSERT_NE(sceneTexture, nullptr);
    EXPECT_EQ(sceneTexture->kind, kera::SlangReflectionBindingKind::Resource);
    EXPECT_EQ(sceneTexture->binding, 1u);

    const kera::SlangReflectionBinding* sceneSampler = lighting.findBinding("sceneSampler");
    ASSERT_NE(sceneSampler, nullptr);
    EXPECT_EQ(sceneSampler->kind, kera::SlangReflectionBindingKind::SamplerState);
    EXPECT_EQ(sceneSampler->binding, 2u);

    const kera::SlangReflectionBinding* lightingParams = lighting.findBinding("lightingParams");
    ASSERT_NE(lightingParams, nullptr);
    EXPECT_EQ(lightingParams->kind, kera::SlangReflectionBindingKind::ConstantBuffer);
    EXPECT_EQ(lightingParams->binding, 3u);
    EXPECT_EQ(lightingParams->uniformSize, 2064u);
    EXPECT_EQ(lightingParams->typeName, "LightingUniforms");

    const kera::SlangReflectionEntryPoint* lightingFragmentMain = lighting.findEntryPoint("lightingFragmentMain");
    ASSERT_NE(lightingFragmentMain, nullptr);
    EXPECT_EQ(lightingFragmentMain->stage, kera::ShaderType::Fragment);

    std::vector<uint32_t> spirv;
    kera::SlangReflectionMetadata runtimeReflection;
    std::string diagnostics;
    const std::filesystem::path shaderRoot(shaderRootEnv);
    const bool compiled = kera::SlangCompiler::compileToSpirvAndReflect(
        {
            .shaderPath = (shaderRoot / "instanced_triangle.slang").string(),
            .entryPoint = "vertexMain",
            .shaderType = kera::ShaderType::Vertex,
        },
        spirv, runtimeReflection, &diagnostics);

    EXPECT_TRUE(compiled) << diagnostics;
    EXPECT_FALSE(spirv.empty());
    EXPECT_NE(runtimeReflection.findBinding("globalParams"), nullptr);
    EXPECT_NE(runtimeReflection.findEntryPoint("vertexMain"), nullptr);
    const kera::SlangReflectionBinding* runtimeGlobalParams = runtimeReflection.findBinding("globalParams");
    ASSERT_NE(runtimeGlobalParams, nullptr);
    EXPECT_EQ(runtimeGlobalParams->stage, kera::ShaderType::Vertex);

    std::vector<uint32_t> lightingSpirv;
    kera::SlangReflectionMetadata runtimeLightingReflection;
    diagnostics.clear();
    const bool lightingCompiled = kera::SlangCompiler::compileToSpirvAndReflect(
        {
            .shaderPath = (shaderRoot / "instanced_triangle_many_lights.slang").string(),
            .entryPoint = "lightingFragmentMain",
            .shaderType = kera::ShaderType::Fragment,
        },
        lightingSpirv, runtimeLightingReflection, &diagnostics);

    EXPECT_TRUE(lightingCompiled) << diagnostics;
    const kera::SlangReflectionBinding* runtimeLightingParams = runtimeLightingReflection.findBinding("lightingParams");
    ASSERT_NE(runtimeLightingParams, nullptr);
    EXPECT_EQ(runtimeLightingParams->stage, kera::ShaderType::Fragment);
}
