// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/api.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace
{
#if defined(_CPPUNWIND) || defined(__EXCEPTIONS)
#error "Kera tests must compile with C++ exceptions disabled."
#endif

#if defined(_MSC_VER) && defined(_HAS_EXCEPTIONS) && (_HAS_EXCEPTIONS == 0)
    static_assert(_HAS_EXCEPTIONS == 0, "Kera MSVC builds must compile the STL with exceptions disabled.");
#endif

    static_assert(std::is_standard_layout_v<KeraStringView>);
    static_assert(std::is_standard_layout_v<KeraByteView>);
    static_assert(std::is_standard_layout_v<KeraHandle>);
    static_assert(std::is_standard_layout_v<KeraRendererApiV1>);
    static_assert(sizeof(KeraStringView) == sizeof(const char*) + sizeof(size_t));
    static_assert(KERA_RENDERER_ABI_VERSION == 1u);
    static_assert(offsetof(KeraRendererApiV1, abi_version) == 0u);
    static_assert(offsetof(KeraRendererApiV1, struct_size) == sizeof(uint32_t));
    static_assert(offsetof(KeraRendererApiV1, create_renderer) >= sizeof(uint32_t) * 2u);

    struct PublicVertex
    {
        float position[3];
        float color[3];
    };
}  // namespace

TEST(KeraRendererPublicApi, BufferDescriptorsAndFunctionTableAreAbiStable)
{
    const KeraStringView debug_name{
        "Public Buffer",
        sizeof("Public Buffer") - 1,
    };

    const KeraBufferDesc buffer_desc{
        256,
        KERA_BUFFER_USAGE_UNIFORM,
        KERA_MEMORY_ACCESS_CPU_WRITE,
        debug_name,
    };
    EXPECT_EQ(buffer_desc.size, 256u);
    EXPECT_EQ(buffer_desc.debug_name.data, debug_name.data);
    EXPECT_EQ(buffer_desc.debug_name.size, debug_name.size);

    KeraRendererApiV1 api{};
    api.abi_version = KERA_RENDERER_ABI_VERSION;
    api.struct_size = sizeof(KeraRendererApiV1);
    EXPECT_EQ(api.abi_version, 1u);
    EXPECT_EQ(api.struct_size, sizeof(KeraRendererApiV1));
    ASSERT_NE(keraGetRendererApiV1(), nullptr);
    EXPECT_EQ(keraGetRendererApiV1()->abi_version, KERA_RENDERER_ABI_VERSION);
    EXPECT_NE(keraGetRendererApiV1()->validate_vertex_input_layout, nullptr);
}

TEST(KeraRendererPublicApi, VertexInputLayoutBuilderOnlyPackagesFields)
{
    const kera::VertexInputLayout layout =
        kera::VertexInputLayoutBuilder{}
            .vertexBinding<PublicVertex>(0)
            .field(KERA_VERTEX_FIELD(PublicVertex, position, 0, KERA_VERTEX_FORMAT_FLOAT3))
            .field(KERA_VERTEX_FIELD(PublicVertex, color, 0, KERA_VERTEX_FORMAT_FLOAT3))
            .layout();

    ASSERT_EQ(layout.binding_count, 1u);
    EXPECT_EQ(layout.bindings[0].binding, 0u);
    EXPECT_EQ(layout.bindings[0].stride, sizeof(PublicVertex));
    ASSERT_EQ(layout.field_count, 2u);
    EXPECT_EQ(layout.fields[0].field_name.size, sizeof("position") - 1u);
    EXPECT_EQ(layout.fields[0].binding, 0u);
    EXPECT_EQ(layout.fields[1].offset, offsetof(PublicVertex, color));
}

TEST(KeraRendererPublicApi, AttachmentsAreCoreTableMembers)
{
    const KeraRendererApiV1* api = keraGetRendererApiV1();

    ASSERT_NE(api, nullptr);
    EXPECT_EQ(api->abi_version, KERA_RENDERER_ABI_VERSION);
    EXPECT_EQ(api->struct_size, sizeof(KeraRendererApiV1));
    EXPECT_TRUE(KERA_RENDERER_API_HAS_MEMBER(api, get_attachment_capabilities));
    EXPECT_TRUE(KERA_RENDERER_API_HAS_MEMBER(api, validate_attachment_texture_desc));
    EXPECT_TRUE(KERA_RENDERER_API_HAS_MEMBER(api, validate_attachment_rendering_desc));
    EXPECT_TRUE(KERA_RENDERER_API_HAS_MEMBER(api, validate_attachment_graphics_pipeline_desc));
    EXPECT_TRUE(KERA_RENDERER_API_HAS_MEMBER(api, create_attachment_texture));
    EXPECT_TRUE(KERA_RENDERER_API_HAS_MEMBER(api, create_attachment_graphics_pipeline));
    EXPECT_TRUE(KERA_RENDERER_API_HAS_MEMBER(api, begin_attachment_rendering));
    EXPECT_TRUE(KERA_RENDERER_API_HAS_MEMBER(api, end_attachment_rendering));
    EXPECT_TRUE(KERA_RENDERER_API_HAS_MEMBER(api, resolve_attachment_texture));
    EXPECT_LE(KERA_RENDERER_API_V1_SIZE_THROUGH(resolve_attachment_texture), api->struct_size);

    KeraRendererApiV1 truncated = *api;
    truncated.struct_size = KERA_RENDERER_API_V1_SIZE_THROUGH(end_attachment_rendering);
    EXPECT_FALSE(KERA_RENDERER_API_HAS_MEMBER(&truncated, resolve_attachment_texture));
    truncated.struct_size = api->struct_size;
    truncated.resolve_attachment_texture = nullptr;
    EXPECT_FALSE(KERA_RENDERER_API_HAS_MEMBER(&truncated, resolve_attachment_texture));
}

TEST(KeraRendererPublicApi, GltfSceneLoaderAppendsToTheCoreTable)
{
    const KeraRendererApiV1* api = keraGetRendererApiV1();
    ASSERT_NE(api, nullptr);
    EXPECT_TRUE(KERA_RENDERER_API_HAS_MEMBER(api, load_gltf_scene));
    EXPECT_TRUE(KERA_RENDERER_API_HAS_MEMBER(api, destroy_gltf_scene));

    KeraRendererApiV1 truncated = *api;
    truncated.struct_size = KERA_RENDERER_API_V1_SIZE_THROUGH(resolve_attachment_texture);
    EXPECT_FALSE(KERA_RENDERER_API_HAS_MEMBER(&truncated, load_gltf_scene));
}

TEST(KeraRendererPublicApi, GpuTimingAppendsToTheCoreTable)
{
    const KeraRendererApiV1* api = keraGetRendererApiV1();
    ASSERT_NE(api, nullptr);
    EXPECT_EQ(api->abi_version, KERA_RENDERER_ABI_VERSION);
    EXPECT_TRUE(KERA_RENDERER_API_HAS_MEMBER(api, get_gpu_timing_capabilities));
    EXPECT_TRUE(KERA_RENDERER_API_HAS_MEMBER(api, begin_gpu_timing_scope));
    EXPECT_TRUE(KERA_RENDERER_API_HAS_MEMBER(api, end_gpu_timing_scope));
    EXPECT_TRUE(KERA_RENDERER_API_HAS_MEMBER(api, copy_completed_gpu_timings));

    KeraRendererApiV1 truncated = *api;
    truncated.struct_size = KERA_RENDERER_API_V1_SIZE_THROUGH(destroy_gltf_scene);
    EXPECT_FALSE(KERA_RENDERER_API_HAS_MEMBER(&truncated, get_gpu_timing_capabilities));

    truncated.destroy = +[](KeraRenderer*) {};
    kera::Renderer truncated_renderer(reinterpret_cast<KeraRenderer*>(static_cast<uintptr_t>(1u)), &truncated);
    EXPECT_FALSE(truncated_renderer.supportsGpuTiming());
    EXPECT_EQ(truncated_renderer.getGpuTimingCapabilities().max_scopes_per_frame, 0u);
    EXPECT_FALSE(truncated_renderer.beginGpuTimingScope({}, 1u, 1u));
    EXPECT_FALSE(truncated_renderer.endGpuTimingScope({}));
    EXPECT_EQ(truncated_renderer.copyCompletedGpuTimings(nullptr, 0), 0u);

    kera::Renderer renderer;
    EXPECT_FALSE(renderer.supportsGpuTiming());
    EXPECT_EQ(renderer.getGpuTimingCapabilities().max_scopes_per_frame, 0u);
    EXPECT_FALSE(renderer.beginGpuTimingScope({}, 1u, 1u));
    EXPECT_FALSE(renderer.endGpuTimingScope({}));
    EXPECT_EQ(renderer.copyCompletedGpuTimings(nullptr, 0), 0u);
}

TEST(KeraRendererPublicApi, GpuTimingHelpersForwardAvailableCoreTableMembers)
{
    g_gpu_timing_wrapper_test_state = {};

    KeraRendererApiV1 api = *keraGetRendererApiV1();
    api.destroy = +[](KeraRenderer*) {};
    api.get_gpu_timing_capabilities = getGpuTimingCapabilitiesForTest;
    api.begin_gpu_timing_scope = beginGpuTimingScopeForTest;
    api.end_gpu_timing_scope = endGpuTimingScopeForTest;
    api.copy_completed_gpu_timings = copyCompletedGpuTimingsForTest;

    kera::Renderer renderer(reinterpret_cast<KeraRenderer*>(static_cast<uintptr_t>(1u)), &api);
    EXPECT_TRUE(renderer.supportsGpuTiming());
    EXPECT_EQ(renderer.getGpuTimingCapabilities().max_scopes_per_frame, 12u);
    EXPECT_TRUE(renderer.beginGpuTimingScope({}, 7u, 42u));
    EXPECT_TRUE(renderer.endGpuTimingScope({}));

    kera::GpuTimingSample samples[2]{};
    EXPECT_EQ(renderer.copyCompletedGpuTimings(samples, 2u), 1u);
    EXPECT_EQ(g_gpu_timing_wrapper_test_state.begin_calls, 1u);
    EXPECT_EQ(g_gpu_timing_wrapper_test_state.end_calls, 1u);
    EXPECT_EQ(g_gpu_timing_wrapper_test_state.copy_calls, 1u);
    EXPECT_EQ(g_gpu_timing_wrapper_test_state.scope_id, 7u);
    EXPECT_EQ(g_gpu_timing_wrapper_test_state.generation, 42u);
    EXPECT_EQ(samples[0].scope_id, 7u);
    EXPECT_EQ(samples[0].generation, 42u);
    EXPECT_EQ(samples[0].frame_index, 9u);
    EXPECT_DOUBLE_EQ(samples[0].gpu_ms, 1.5);
    EXPECT_DOUBLE_EQ(samples[0].cpu_encode_ms, 0.25);
    EXPECT_TRUE(samples[0].valid);
}

TEST(KeraRendererPublicApi, AttachmentRendererHelpersRejectUnavailableCoreTable)
{
    kera::Renderer renderer;
    KeraAttachmentError error{};
    const KeraAttachmentTextureDesc texture_desc{
        .struct_size = sizeof(KeraAttachmentTextureDesc),
    };
    const KeraAttachmentRenderingDesc rendering_desc{
        .struct_size = sizeof(KeraAttachmentRenderingDesc),
    };
    const KeraAttachmentGraphicsPipelineDesc pipeline_desc{
        .struct_size = sizeof(KeraAttachmentGraphicsPipelineDesc),
    };

    EXPECT_FALSE(renderer.supportsAttachmentRendering());
    EXPECT_FALSE(renderer.supportsAttachmentResolve());
    EXPECT_EQ(renderer.getAttachmentCapabilities().max_color_attachments, 0u);
    EXPECT_FALSE(renderer.validateAttachmentTextureDesc(texture_desc, &error));
    EXPECT_EQ(error.code, KERA_ATTACHMENT_ERROR_UNSUPPORTED);
    EXPECT_EQ(std::string_view(error.message.data, error.message.size), "Core attachment rendering is unavailable.");
    EXPECT_FALSE(renderer.validateAttachmentRenderingDesc(rendering_desc, &error));
    EXPECT_FALSE(renderer.validateAttachmentGraphicsPipelineDesc(pipeline_desc, &error));
    EXPECT_FALSE(renderer.createAttachmentTexture(texture_desc, &error).isValid());
    EXPECT_FALSE(renderer.createAttachmentGraphicsPipeline(pipeline_desc, &error).isValid());
    EXPECT_FALSE(renderer.beginAttachmentRendering({}, rendering_desc, &error));
    EXPECT_FALSE(renderer.endAttachmentRendering({}, &error));
    EXPECT_FALSE(renderer.resolveAttachmentTexture({}, {}, {}, &error));
}

TEST(KeraRendererPublicApi, AttachmentRendererHelpersRespectTruncatedCoreTable)
{
    const KeraRendererApiV1* api = keraGetRendererApiV1();
    ASSERT_NE(api, nullptr);

    KeraRendererApiV1 truncated = *api;
    truncated.destroy = +[](KeraRenderer*) {};
    truncated.struct_size = KERA_RENDERER_API_V1_SIZE_THROUGH(destroy_ibl_environment);
    kera::Renderer renderer(reinterpret_cast<KeraRenderer*>(static_cast<uintptr_t>(1u)), &truncated);
    KeraAttachmentError error{};
    const KeraAttachmentTextureDesc texture_desc{
        .struct_size = sizeof(KeraAttachmentTextureDesc),
    };
    const KeraAttachmentRenderingDesc rendering_desc{
        .struct_size = sizeof(KeraAttachmentRenderingDesc),
    };
    const KeraAttachmentGraphicsPipelineDesc pipeline_desc{
        .struct_size = sizeof(KeraAttachmentGraphicsPipelineDesc),
    };

    EXPECT_FALSE(renderer.supportsAttachmentRendering());
    EXPECT_FALSE(renderer.supportsAttachmentResolve());
    EXPECT_EQ(renderer.getAttachmentCapabilities().max_color_attachments, 0u);
    EXPECT_FALSE(renderer.validateAttachmentTextureDesc(texture_desc, &error));
    EXPECT_EQ(error.code, KERA_ATTACHMENT_ERROR_UNSUPPORTED);
    EXPECT_FALSE(renderer.validateAttachmentRenderingDesc(rendering_desc, &error));
    EXPECT_FALSE(renderer.validateAttachmentGraphicsPipelineDesc(pipeline_desc, &error));
    EXPECT_FALSE(renderer.createAttachmentTexture(texture_desc, &error).isValid());
    EXPECT_FALSE(renderer.createAttachmentGraphicsPipeline(pipeline_desc, &error).isValid());
    EXPECT_FALSE(renderer.beginAttachmentRendering({}, rendering_desc, &error));
    EXPECT_FALSE(renderer.endAttachmentRendering({}, &error));
    EXPECT_FALSE(renderer.resolveAttachmentTexture({}, {}, {}, &error));
}

TEST(KeraRendererPublicApi, CoreAttachmentsValidatePublicDescriptors)
{
    const KeraRendererApiV1* api = keraGetRendererApiV1();
    ASSERT_NE(api, nullptr);

    KeraAttachmentError error{};
    KeraAttachmentTextureDesc texture_desc{
        .struct_size = sizeof(KeraAttachmentTextureDesc),
        .width = 64,
        .height = 64,
        .format = KERA_TEXTURE_FORMAT_RGBA8,
        .usage_flags = KERA_ATTACHMENT_TEXTURE_USAGE_COLOR_ATTACHMENT | KERA_ATTACHMENT_TEXTURE_USAGE_TRANSFER_SRC,
        .sample_count = KERA_ATTACHMENT_SAMPLE_COUNT_1,
    };
    EXPECT_EQ(api->validate_attachment_texture_desc(&texture_desc, &error), 1);
    EXPECT_EQ(error.code, KERA_ATTACHMENT_ERROR_NONE);

    texture_desc.width = 0;
    EXPECT_EQ(api->validate_attachment_texture_desc(&texture_desc, &error), 0);
    EXPECT_EQ(error.code, KERA_ATTACHMENT_ERROR_VALIDATION_FAILED);
    EXPECT_EQ(std::string_view(error.message.data, error.message.size),
              "Attachment texture dimensions must be non-zero.");

    texture_desc.width = 64;
    texture_desc.usage_flags |= 1u << 31u;
    EXPECT_EQ(api->validate_attachment_texture_desc(&texture_desc, &error), 0);
    EXPECT_EQ(error.code, KERA_ATTACHMENT_ERROR_VALIDATION_FAILED);

    texture_desc.usage_flags = KERA_ATTACHMENT_TEXTURE_USAGE_COLOR_ATTACHMENT;
    texture_desc.sample_count = static_cast<KeraAttachmentSampleCount>(3);
    EXPECT_EQ(api->validate_attachment_texture_desc(&texture_desc, &error), 0);
    EXPECT_EQ(error.code, KERA_ATTACHMENT_ERROR_VALIDATION_FAILED);

    texture_desc.sample_count = KERA_ATTACHMENT_SAMPLE_COUNT_4;
    EXPECT_EQ(api->validate_attachment_texture_desc(&texture_desc, &error), 1);
    EXPECT_EQ(error.code, KERA_ATTACHMENT_ERROR_NONE);

    KeraColorAttachmentDesc color_attachment{};
    color_attachment.load_op = static_cast<KeraAttachmentLoadOp>(-1);
    KeraAttachmentRenderingDesc rendering_desc{
        .struct_size = sizeof(KeraAttachmentRenderingDesc),
        .color_attachments = &color_attachment,
        .color_attachment_count = 1,
    };
    EXPECT_EQ(api->validate_attachment_rendering_desc(&rendering_desc, &error), 0);
    EXPECT_EQ(error.code, KERA_ATTACHMENT_ERROR_VALIDATION_FAILED);

    KeraDepthAttachmentDesc depth_attachment{
        .load_op = KERA_ATTACHMENT_LOAD_OP_CLEAR,
        .store_op = KERA_ATTACHMENT_STORE_OP_STORE,
        .clear_depth = 1.0f,
    };
    rendering_desc.color_attachments = nullptr;
    rendering_desc.color_attachment_count = 0;
    rendering_desc.depth_attachment = &depth_attachment;
    EXPECT_EQ(api->validate_attachment_rendering_desc(&rendering_desc, &error), 1);
    EXPECT_EQ(error.code, KERA_ATTACHMENT_ERROR_NONE);

    const KeraTextureFormat invalid_color_format = KERA_TEXTURE_FORMAT_DEPTH32;
    KeraAttachmentGraphicsPipelineDesc pipeline_desc{
        .struct_size = sizeof(KeraAttachmentGraphicsPipelineDesc),
        .color_formats = &invalid_color_format,
        .color_format_count = 1,
        .sample_count = KERA_ATTACHMENT_SAMPLE_COUNT_1,
    };
    EXPECT_EQ(api->validate_attachment_graphics_pipeline_desc(&pipeline_desc, &error), 0);
    EXPECT_EQ(error.code, KERA_ATTACHMENT_ERROR_VALIDATION_FAILED);

    const KeraAttachmentGraphicsPipelineDesc depth_only_pipeline_desc{
        .struct_size = sizeof(KeraAttachmentGraphicsPipelineDesc),
        .color_formats = nullptr,
        .color_format_count = 0,
        .depth_format = KERA_TEXTURE_FORMAT_DEPTH32,
        .has_depth_attachment = 1,
        .sample_count = KERA_ATTACHMENT_SAMPLE_COUNT_1,
    };
    EXPECT_EQ(api->validate_attachment_graphics_pipeline_desc(&depth_only_pipeline_desc, &error), 1);
    EXPECT_EQ(error.code, KERA_ATTACHMENT_ERROR_NONE);

    const KeraTextureFormat valid_color_format = KERA_TEXTURE_FORMAT_RGBA8;
    pipeline_desc.color_formats = &valid_color_format;
    pipeline_desc.sample_count = KERA_ATTACHMENT_SAMPLE_COUNT_4;
    EXPECT_EQ(api->validate_attachment_graphics_pipeline_desc(&pipeline_desc, &error), 1);
    EXPECT_EQ(error.code, KERA_ATTACHMENT_ERROR_NONE);

    pipeline_desc.cull_mode = static_cast<KeraCullModeKind>(-1);
    EXPECT_EQ(api->validate_attachment_graphics_pipeline_desc(&pipeline_desc, &error), 0);
    EXPECT_EQ(error.code, KERA_ATTACHMENT_ERROR_VALIDATION_FAILED);
}
