// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/interfaces.h"
#include "kera/renderer/reflection_contracts.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace
{
    struct Vertex
    {
        float position[3];
        float color[3];
    };

    struct InstanceData
    {
        float model_matrix[16];
    };

    struct Uniforms
    {
        float matrix[16];
    };

    kera::SlangReflectionInput makeInput(const std::string& parameter_name, const std::string& field_name,
                                         const std::string& semantic_name, uint32_t location,
                                         kera::EVertexFormat format, uint32_t location_count = 1)
    {
        return {
            .name = field_name,
            .parameter_name = parameter_name,
            .field_name = field_name,
            .semantic_name = semantic_name,
            .location = location,
            .location_count = location_count,
            .format = format,
            .has_format = true,
        };
    }

    kera::SlangReflectionMetadata makeTriangleReflection()
    {
        kera::SlangReflectionMetadata reflection{};
        reflection.entry_points.push_back({
            .name = "vertexMain",
            .stage = kera::EShaderType::VERTEX,
            .inputs =
                {
                    makeInput("vertex", "position", "POSITION", 0, kera::EVertexFormat::FLOAT3),
                    makeInput("vertex", "color", "COLOR", 1, kera::EVertexFormat::FLOAT3),
                },
        });
        reflection.bindings.push_back({
            .name = "globalParams",
            .kind = kera::ESlangReflectionBindingKind::CONSTANT_BUFFER,
            .stage = kera::EShaderType::VERTEX,
            .binding = 0,
            .space = 0,
            .count = 1,
            .type_name = "Uniforms",
            .uniform_size = sizeof(Uniforms),
        });
        return reflection;
    }

    kera::SlangReflectionMetadata makeInstancedReflection()
    {
        kera::SlangReflectionMetadata reflection = makeTriangleReflection();
        reflection.entry_points.front().inputs.push_back(
            makeInput("instance", "model_matrix", "TRANSFORM", 2, kera::EVertexFormat::FLOAT4, 4));
        return reflection;
    }

    kera::VertexInputLayoutBuilder makeValidBuilder()
    {
        return kera::VertexInputLayoutBuilder{}
            .debugName("Vertex Input Test")
            .vertexBinding<Vertex>(0)
            .field("position", 0, offsetof(Vertex, position), kera::EVertexFormat::FLOAT3)
            .field("color", 0, offsetof(Vertex, color), kera::EVertexFormat::FLOAT3);
    }

    kera::VertexInputLayoutBuildResult build(kera::VertexInputLayoutBuilder builder,
                                             const kera::SlangReflectionMetadata& reflection)
    {
        return std::move(builder).build(reflection);
    }

    class FakeRenderer final : public kera::IRenderer
    {
    public:
        kera::EGraphicsBackend getBackend() const override
        {
            return kera::EGraphicsBackend::VULKAN;
        }
        kera::Extent2D getDrawableExtent() const override
        {
            return {};
        }
        kera::RendererStats getStats() const override
        {
            return {};
        }
        void shutdown() override {}
        bool resize(kera::Extent2D) override
        {
            return true;
        }
        bool initializeUi() override
        {
            return false;
        }
        void shutdownUi() override {}
        void handleUiEvent(const SDL_Event&) override {}
        void beginUi() override {}
        void endUi() override {}
        void renderUi(kera::FrameHandle) override {}
        bool isUiAvailable() const override
        {
            return false;
        }

        kera::ShaderModuleHandle createShaderModule(const kera::ShaderModuleDesc&) override
        {
            return {};
        }
        bool destroyShaderModule(kera::ShaderModuleHandle) override
        {
            return false;
        }
        kera::ShaderProgramHandle createShaderProgram(const kera::ShaderProgramDesc&) override
        {
            return {};
        }
        kera::ShaderProgramHandle createGraphicsShaderProgram(const kera::GraphicsShaderProgramDesc&) override
        {
            return {};
        }
        const kera::SlangReflectionMetadata* getShaderProgramReflection(kera::ShaderProgramHandle) const override
        {
            return nullptr;
        }
        bool destroyShaderProgram(kera::ShaderProgramHandle) override
        {
            return false;
        }

        kera::BufferHandle createBuffer(const kera::BufferDesc&) override
        {
            return {};
        }
        bool destroyBuffer(kera::BufferHandle) override
        {
            return false;
        }
        bool mapBuffer(kera::BufferHandle, void**) override
        {
            return false;
        }
        void unmapBuffer(kera::BufferHandle) override {}
        bool uploadBuffer(kera::BufferHandle, const void*, std::size_t, std::size_t) override
        {
            return false;
        }
        kera::BufferHandle createUniformRingBuffer(std::size_t, uint32_t) override
        {
            return {};
        }
        bool uploadUniformRingBuffer(kera::BufferHandle, kera::FrameHandle, const void*, std::size_t) override
        {
            return false;
        }
        kera::UniformRingBufferInfo getUniformRingBufferInfo(kera::BufferHandle buffer) const
        {
            return kera::UniformRingBufferInfo{};
        }
        uint32_t getUniformRingBufferSlot(kera::BufferHandle buffer, kera::FrameHandle frame) const
        {
            return 0;
        }
        std::size_t getUniformRingBufferSlotOffset(kera::BufferHandle buffer, uint32_t slot) const
        {
            return 0;
        }
        kera::TextureHandle createTexture(const kera::TextureDesc&) override
        {
            return {};
        }
        int32_t upload_batch_begin_count = 0;
        int32_t upload_batch_end_count = 0;
        int32_t upload_batch_cancel_count = 0;
        bool upload_batch_begin_succeeds = true;
        bool upload_batch_end_succeeds = true;
        bool texture_uploads_suceed = true;

        bool beginUploadBatch() override
        {
            return false;
        }
        bool endUploadBatch() override
        {
            return false;
        }
        void cancelUploadBatch() override {}
        bool uploadTexture(kera::TextureHandle, const void*, std::size_t) override
        {
            return false;
        }
        bool uploadTextureSubresource(kera::TextureHandle texture, const kera::TexturePrepareUpload& upload)
        {
            return false;
        }
        bool destroyTexture(kera::TextureHandle) override
        {
            return false;
        }
        kera::SamplerHandle createSampler(const kera::SamplerDesc&) override
        {
            return {};
        }
        bool destroySampler(kera::SamplerHandle) override
        {
            return false;
        }
        kera::RenderTargetHandle createRenderTarget(const kera::RenderTargetDesc&) override
        {
            return {};
        }
        bool destroyRenderTarget(kera::RenderTargetHandle) override
        {
            return false;
        }

        kera::GraphicsPipelineHandle createGraphicsPipeline(const kera::GraphicsPipelineDesc&,
                                                            kera::ShaderProgramHandle) override
        {
            return {};
        }
        kera::GraphicsPipelineHandle createGraphicsPipeline(const kera::GraphicsPipelineCreateDesc&) override
        {
            return {};
        }
        std::vector<kera::DescriptorSetLayoutDesc> getGraphicsPipelineDescriptorSets(
            kera::GraphicsPipelineHandle) const override
        {
            return {};
        }
        bool destroyGraphicsPipeline(kera::GraphicsPipelineHandle) override
        {
            return false;
        }
        kera::DescriptorSetHandle createDescriptorSet(kera::GraphicsPipelineHandle) override
        {
            return {};
        }
        kera::DescriptorSetHandle createDescriptorSet(kera::GraphicsPipelineHandle, uint32_t) override
        {
            return {};
        }
        bool updateDescriptorSet(kera::DescriptorSetHandle, uint32_t, kera::BufferHandle, std::size_t,
                                 std::size_t) override
        {
            return false;
        }
        bool updateDescriptorSet(kera::DescriptorSetHandle, uint32_t, kera::TextureHandle) override
        {
            return false;
        }
        bool updateDescriptorSet(kera::DescriptorSetHandle, uint32_t, kera::SamplerHandle) override
        {
            return false;
        }
        bool updateDescriptorSet(kera::DescriptorSetHandle, const std::string& name, kera::BufferHandle, std::size_t,
                                 std::size_t) override
        {
            return name == "globalParams";
        }
        bool updateDescriptorSet(kera::DescriptorSetHandle, const std::string& name, kera::TextureHandle) override
        {
            return name == "sceneTexture";
        }
        bool updateDescriptorSet(kera::DescriptorSetHandle, const std::string& name, kera::SamplerHandle) override
        {
            return name == "sceneSampler";
        }
        bool destroyDescriptorSet(kera::DescriptorSetHandle) override
        {
            return false;
        }

        kera::FrameHandle beginFrame() override
        {
            return {};
        }
        void beginRenderPass(kera::FrameHandle, const kera::RenderPassDesc&) override {}
        void beginRenderPass(kera::FrameHandle, kera::RenderTargetHandle, const kera::RenderPassDesc&) override {}
        void endRenderPass(kera::FrameHandle) override {}
        void bindPipeline(kera::FrameHandle, kera::GraphicsPipelineHandle) override {}
        void bindVertexBuffer(kera::FrameHandle, uint32_t, kera::BufferHandle, std::size_t) override {}
        void bindIndexBuffer(kera::FrameHandle, kera::BufferHandle, kera::EIndexFormat, std::size_t) override {}
        void bindDescriptorSet(kera::FrameHandle, kera::GraphicsPipelineHandle, kera::DescriptorSetHandle) override {}
        void bindDescriptorSet(kera::FrameHandle, kera::GraphicsPipelineHandle, uint32_t,
                               kera::DescriptorSetHandle) override
        {
        }
        void drawIndexed(kera::FrameHandle, uint32_t, uint32_t) override {}
        bool endFrame(kera::FrameHandle) override
        {
            return false;
        }
    };
}  // namespace

TEST(KeraReflectionContracts, BuildsValidatedVertexInputLayoutAndDescriptorUpdates)
{
    const kera::SlangReflectionMetadata reflection = makeTriangleReflection();
    const kera::VertexInputLayoutBuildResult result = build(makeValidBuilder(), reflection);

    ASSERT_TRUE(result.ok()) << result.errorMessage();
    ASSERT_EQ(result.layout().bindings.size(), 1u);
    ASSERT_EQ(result.layout().attributes.size(), 2u);
    EXPECT_EQ(result.layout().bindings.front().stride, sizeof(Vertex));
    EXPECT_EQ(result.layout().attributes[0].location, 0u);
    EXPECT_EQ(result.layout().attributes[0].offset, offsetof(Vertex, position));
    EXPECT_EQ(result.layout().attributes[1].location, 1u);
    EXPECT_EQ(result.layout().attributes[1].offset, offsetof(Vertex, color));

    FakeRenderer renderer;
    const kera::DescriptorSetHandle descriptor_set{0, 1};
    const kera::BufferHandle buffer{0, 1};
    const kera::TextureHandle texture{0, 1};
    const kera::SamplerHandle sampler{0, 1};
    const kera::DescriptorSetUpdate update = renderer.updateDescriptors(descriptor_set)
                                                 .uniform<Uniforms>("globalParams", buffer)
                                                 .sampledImage("sceneTexture", texture)
                                                 .sampler("sceneSampler", sampler);
    EXPECT_TRUE(update.ok());

    const kera::DescriptorSetUpdate failed_update =
        renderer.updateDescriptors(descriptor_set).sampledImage("globalParams", texture);
    EXPECT_FALSE(failed_update.ok());
    EXPECT_FALSE(failed_update.errorMessage().empty());
    EXPECT_FALSE(failed_update.report().ok());
    ASSERT_EQ(failed_update.report().issues.size(), 1u);
    EXPECT_EQ(failed_update.report().issues.front().code, kera::ERendererErrorCode::VALIDATION_FAILED);
    EXPECT_EQ(failed_update.report().issues.front().category, kera::ERendererValidationCategory::DESCRIPTOR);
    EXPECT_FALSE(failed_update.result().ok());
    EXPECT_EQ(failed_update.result().errorCode(), kera::ERendererErrorCode::VALIDATION_FAILED);
    EXPECT_FALSE(renderer.tryCreateGraphicsShaderProgram({}).ok());
    EXPECT_FALSE(renderer.tryCreateGraphicsPipeline({}).ok());
    EXPECT_FALSE(renderer.tryCreateDescriptorSet({0, 1}).ok());
}

TEST(KeraReflectionContracts, BuildRejectsInvalidVertexInputLayouts)
{
    const kera::SlangReflectionMetadata reflection = makeTriangleReflection();

    EXPECT_FALSE(build(kera::VertexInputLayoutBuilder{}
                           .vertexBinding<Vertex>(0)
                           .vertexBinding<Vertex>(0)
                           .field("position", 0, offsetof(Vertex, position), kera::EVertexFormat::FLOAT3)
                           .field("color", 0, offsetof(Vertex, color), kera::EVertexFormat::FLOAT3),
                       reflection)
                     .ok());

    EXPECT_FALSE(build(kera::VertexInputLayoutBuilder{}
                           .vertexBinding(0, 0)
                           .field("position", 0, offsetof(Vertex, position), kera::EVertexFormat::FLOAT3)
                           .field("color", 0, offsetof(Vertex, color), kera::EVertexFormat::FLOAT3),
                       reflection)
                     .ok());

    EXPECT_FALSE(build(kera::VertexInputLayoutBuilder{}
                           .vertexBinding<Vertex>(0)
                           .field("missingPosition", 0, offsetof(Vertex, position), kera::EVertexFormat::FLOAT3)
                           .field("color", 0, offsetof(Vertex, color), kera::EVertexFormat::FLOAT3),
                       reflection)
                     .ok());

    EXPECT_FALSE(build(kera::VertexInputLayoutBuilder{}
                           .vertexBinding<Vertex>(0)
                           .field("position", 1, offsetof(Vertex, position), kera::EVertexFormat::FLOAT3)
                           .field("color", 0, offsetof(Vertex, color), kera::EVertexFormat::FLOAT3),
                       reflection)
                     .ok());

    EXPECT_FALSE(build(kera::VertexInputLayoutBuilder{}
                           .vertexBinding<Vertex>(0)
                           .field("position", 0, offsetof(Vertex, position), kera::EVertexFormat::FLOAT2)
                           .field("color", 0, offsetof(Vertex, color), kera::EVertexFormat::FLOAT3),
                       reflection)
                     .ok());

    EXPECT_FALSE(build(kera::VertexInputLayoutBuilder{}
                           .vertexBinding<Vertex>(0)
                           .field("position", 0, offsetof(Vertex, position), kera::EVertexFormat::FLOAT3)
                           .field("position", 0, offsetof(Vertex, color), kera::EVertexFormat::FLOAT3),
                       reflection)
                     .ok());

    EXPECT_FALSE(build(kera::VertexInputLayoutBuilder{}
                           .debugName("Unused Field Test")
                           .vertexBinding<Vertex>(0)
                           .field("position", 0, offsetof(Vertex, position), kera::EVertexFormat::FLOAT3)
                           .field("color", 0, offsetof(Vertex, color), kera::EVertexFormat::FLOAT3)
                           .field("uv", 0, 0, kera::EVertexFormat::FLOAT2),
                       reflection)
                     .ok());
}

TEST(KeraReflectionContracts, BuildRejectsMissingOrAmbiguousVertexEntryReflection)
{
    kera::SlangReflectionMetadata missing_vertex_reflection = makeTriangleReflection();
    missing_vertex_reflection.entry_points.clear();
    EXPECT_FALSE(build(makeValidBuilder(), missing_vertex_reflection).ok());

    kera::SlangReflectionMetadata ambiguous_vertex_reflection = makeTriangleReflection();
    ambiguous_vertex_reflection.entry_points.push_back(ambiguous_vertex_reflection.entry_points.front());
    ambiguous_vertex_reflection.entry_points.back().name = "otherVertexMain";
    EXPECT_FALSE(build(makeValidBuilder(), ambiguous_vertex_reflection).ok());
}

TEST(KeraReflectionContracts, BuildExpandsMatrixInputs)
{
    const kera::SlangReflectionMetadata reflection = makeInstancedReflection();

    const kera::VertexInputLayoutBuildResult result =
        build(kera::VertexInputLayoutBuilder{}
                  .vertexBinding<Vertex>(0)
                  .vertexBinding<InstanceData>(1, kera::EVertexInputRate::INSTANCE)
                  .field("position", 0, offsetof(Vertex, position), kera::EVertexFormat::FLOAT3)
                  .field("color", 0, offsetof(Vertex, color), kera::EVertexFormat::FLOAT3)
                  .field("model_matrix", 1, offsetof(InstanceData, model_matrix), kera::EVertexFormat::FLOAT4),
              reflection);

    ASSERT_TRUE(result.ok()) << result.errorMessage();
    ASSERT_EQ(result.layout().attributes.size(), 6u);
    EXPECT_EQ(result.layout().attributes[2].location, 2u);
    EXPECT_EQ(result.layout().attributes[2].offset, offsetof(InstanceData, model_matrix));
    EXPECT_EQ(result.layout().attributes[5].location, 5u);
    EXPECT_EQ(result.layout().attributes[5].offset, offsetof(InstanceData, model_matrix) + sizeof(float) * 12);
}

TEST(KeraReflectionContracts, AmbiguousFieldsRequireParameterName)
{
    kera::SlangReflectionMetadata reflection{};
    reflection.entry_points.push_back({
        .name = "vertexMain",
        .stage = kera::EShaderType::VERTEX,
        .inputs =
            {
                makeInput("vertex", "position", "POSITION", 0, kera::EVertexFormat::FLOAT3),
                makeInput("instance", "position", "INSTANCE_POSITION", 1, kera::EVertexFormat::FLOAT3),
            },
    });

    EXPECT_FALSE(build(kera::VertexInputLayoutBuilder{}.vertexBinding<Vertex>(0).field(
                           "position", 0, offsetof(Vertex, position), kera::EVertexFormat::FLOAT3),
                       reflection)
                     .ok());

    EXPECT_TRUE(build(kera::VertexInputLayoutBuilder{}
                          .vertexBinding<Vertex>(0)
                          .fieldIn("vertex", "position", 0, offsetof(Vertex, position), kera::EVertexFormat::FLOAT3)
                          .fieldIn("instance", "position", 0, offsetof(Vertex, color), kera::EVertexFormat::FLOAT3),
                      reflection)
                    .ok());
}
