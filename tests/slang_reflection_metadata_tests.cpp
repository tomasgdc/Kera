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

#if defined(_MSC_VER) && defined(_HAS_EXCEPTIONS) && (_HAS_EXCEPTIONS == 0)
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
    const char* instanced_json = getEnv("KERA_REFLECTION_INSTANCED_JSON");
    const char* lighting_json = getEnv("KERA_REFLECTION_LIGHTING_JSON");
    const char* shader_root_env = getEnv("KERA_REFLECTION_SHADER_ROOT");
    if (instanced_json == nullptr || lighting_json == nullptr || shader_root_env == nullptr)
    {
        GTEST_SKIP() << "Generated Slang reflection JSON paths are provided by kera_slang_reflection_tests.";
    }

    const kera::SlangReflectionMetadata instanced = parseMetadata(instanced_json);
    const kera::SlangReflectionBinding* global_params = instanced.findBinding("globalParams");
    ASSERT_NE(global_params, nullptr);
    EXPECT_EQ(global_params->kind, kera::ESlangReflectionBindingKind::PARAMETER_BLOCK);
    EXPECT_EQ(global_params->binding, 0u);
    EXPECT_EQ(global_params->uniform_size, 128u);
    EXPECT_EQ(global_params->type_name, "Uniforms");

    const kera::SlangReflectionEntryPoint* vertex_main = instanced.findEntryPoint("vertexMain");
    ASSERT_NE(vertex_main, nullptr);
    EXPECT_EQ(vertex_main->stage, kera::EShaderType::VERTEX);
    ASSERT_EQ(vertex_main->inputs.size(), 3u);
    EXPECT_EQ(vertex_main->inputs[0].parameter_name, "vertex");
    EXPECT_EQ(vertex_main->inputs[0].field_name, "position");
    EXPECT_EQ(vertex_main->inputs[0].semantic_name, "POSITION");
    EXPECT_EQ(vertex_main->inputs[0].location, 0u);
    EXPECT_EQ(vertex_main->inputs[0].format, kera::EVertexFormat::FLOAT3);
    EXPECT_TRUE(vertex_main->inputs[0].has_format);
    EXPECT_EQ(vertex_main->inputs[1].parameter_name, "vertex");
    EXPECT_EQ(vertex_main->inputs[1].field_name, "color");
    EXPECT_EQ(vertex_main->inputs[1].semantic_name, "COLOR");
    EXPECT_EQ(vertex_main->inputs[1].location, 1u);
    EXPECT_EQ(vertex_main->inputs[1].format, kera::EVertexFormat::FLOAT3);
    EXPECT_TRUE(vertex_main->inputs[1].has_format);
    EXPECT_EQ(vertex_main->inputs[2].parameter_name, "instance");
    EXPECT_EQ(vertex_main->inputs[2].field_name, "model_matrix");
    EXPECT_EQ(vertex_main->inputs[2].semantic_name, "TRANSFORM");
    EXPECT_EQ(vertex_main->inputs[2].location, 2u);
    EXPECT_EQ(vertex_main->inputs[2].location_count, 4u);
    EXPECT_EQ(vertex_main->inputs[2].format, kera::EVertexFormat::FLOAT4);
    EXPECT_TRUE(vertex_main->inputs[2].has_format);

    const kera::SlangReflectionMetadata lighting = parseMetadata(lighting_json);
    const kera::SlangReflectionBinding* scene_texture = lighting.findBinding("sceneTexture");
    ASSERT_NE(scene_texture, nullptr);
    EXPECT_EQ(scene_texture->kind, kera::ESlangReflectionBindingKind::RESOURCE);
    EXPECT_EQ(scene_texture->binding, 1u);

    const kera::SlangReflectionBinding* scene_sampler = lighting.findBinding("sceneSampler");
    ASSERT_NE(scene_sampler, nullptr);
    EXPECT_EQ(scene_sampler->kind, kera::ESlangReflectionBindingKind::SAMPLER_STATE);
    EXPECT_EQ(scene_sampler->binding, 2u);

    const kera::SlangReflectionBinding* lighting_params = lighting.findBinding("lightingParams");
    ASSERT_NE(lighting_params, nullptr);
    EXPECT_EQ(lighting_params->kind, kera::ESlangReflectionBindingKind::CONSTANT_BUFFER);
    EXPECT_EQ(lighting_params->binding, 3u);
    EXPECT_EQ(lighting_params->uniform_size, 2064u);
    EXPECT_EQ(lighting_params->type_name, "LightingUniforms");

    const kera::SlangReflectionEntryPoint* lighting_fragment_main = lighting.findEntryPoint("lightingFragmentMain");
    ASSERT_NE(lighting_fragment_main, nullptr);
    EXPECT_EQ(lighting_fragment_main->stage, kera::EShaderType::FRAGMENT);

    std::vector<uint32_t> spirv;
    kera::SlangReflectionMetadata runtime_reflection;
    std::string diagnostics;
    const std::filesystem::path shader_root(shader_root_env);
    const bool compiled = kera::SlangCompiler::compileToSpirvAndReflect(
        {
            .shader_path = (shader_root / "instanced_triangle.slang").string(),
            .entry_point = "vertexMain",
            .shader_type = kera::EShaderType::VERTEX,
        },
        spirv, runtime_reflection, &diagnostics);

    EXPECT_TRUE(compiled) << diagnostics;
    EXPECT_FALSE(spirv.empty());
    EXPECT_NE(runtime_reflection.findBinding("globalParams"), nullptr);
    EXPECT_NE(runtime_reflection.findEntryPoint("vertexMain"), nullptr);
    const kera::SlangReflectionBinding* runtime_global_params = runtime_reflection.findBinding("globalParams");
    ASSERT_NE(runtime_global_params, nullptr);
    EXPECT_EQ(runtime_global_params->stage, kera::EShaderType::VERTEX);

    std::vector<uint32_t> lighting_spirv;
    kera::SlangReflectionMetadata runtime_lighting_reflection;
    diagnostics.clear();
    const bool lighting_compiled = kera::SlangCompiler::compileToSpirvAndReflect(
        {
            .shader_path = (shader_root / "instanced_triangle_many_lights.slang").string(),
            .entry_point = "lightingFragmentMain",
            .shader_type = kera::EShaderType::FRAGMENT,
        },
        lighting_spirv, runtime_lighting_reflection, &diagnostics);

    EXPECT_TRUE(lighting_compiled) << diagnostics;
    const kera::SlangReflectionBinding* runtime_lighting_params =
        runtime_lighting_reflection.findBinding("lightingParams");
    ASSERT_NE(runtime_lighting_params, nullptr);
    EXPECT_EQ(runtime_lighting_params->stage, kera::EShaderType::FRAGMENT);
}
