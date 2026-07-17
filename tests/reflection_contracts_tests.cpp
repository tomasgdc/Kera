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
        float modelMatrix[16];
    };

    struct Uniforms
    {
        float matrix[16];
    };

    kera::SlangReflectionInput makeInput(const std::string& parameterName, const std::string& fieldName,
                                         const std::string& semanticName, uint32_t location, kera::VertexFormat format,
                                         uint32_t locationCount = 1)
    {
        return {
            .name = fieldName,
            .parameterName = parameterName,
            .fieldName = fieldName,
            .semanticName = semanticName,
            .location = location,
            .locationCount = locationCount,
            .format = format,
            .hasFormat = true,
        };
    }

    kera::SlangReflectionMetadata makeTriangleReflection()
    {
        kera::SlangReflectionMetadata reflection{};
        reflection.entryPoints.push_back({
            .name = "vertexMain",
            .stage = kera::ShaderType::Vertex,
            .inputs =
                {
                    makeInput("vertex", "position", "POSITION", 0, kera::VertexFormat::Float3),
                    makeInput("vertex", "color", "COLOR", 1, kera::VertexFormat::Float3),
                },
        });
        reflection.bindings.push_back({
            .name = "globalParams",
            .kind = kera::SlangReflectionBindingKind::ConstantBuffer,
            .stage = kera::ShaderType::Vertex,
            .binding = 0,
            .space = 0,
            .count = 1,
            .typeName = "Uniforms",
            .uniformSize = sizeof(Uniforms),
        });
        return reflection;
    }

    kera::SlangReflectionMetadata makeInstancedReflection()
    {
        kera::SlangReflectionMetadata reflection = makeTriangleReflection();
        reflection.entryPoints.front().inputs.push_back(
            makeInput("instance", "modelMatrix", "TRANSFORM", 2, kera::VertexFormat::Float4, 4));
        return reflection;
    }

    kera::VertexInputLayoutBuilder makeValidBuilder()
    {
        return kera::VertexInputLayoutBuilder{}
            .debugName("Vertex Input Test")
            .vertexBinding<Vertex>(0)
            .field("position", 0, offsetof(Vertex, position), kera::VertexFormat::Float3)
            .field("color", 0, offsetof(Vertex, color), kera::VertexFormat::Float3);
    }

    kera::VertexInputLayoutBuildResult build(kera::VertexInputLayoutBuilder builder,
                                             const kera::SlangReflectionMetadata& reflection)
    {
        return std::move(builder).build(reflection);
    }

    class FakeRenderer final : public kera::IRenderer
    {
    public:
        kera::GraphicsBackend getBackend() const override
        {
            return kera::GraphicsBackend::Vulkan;
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
        int32_t uploadBatchBeginCount = 0;
        int32_t uploadBatchEndCount = 0;
        int32_t uploadBatchCancelCount = 0;
        bool uploadBatchBeginSucceeds = true;
        bool uploadBatchEndSucceeds = true;
        bool textureUploadsSuceed = true;

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
        void bindIndexBuffer(kera::FrameHandle, kera::BufferHandle, kera::IndexFormat, std::size_t) override {}
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
    const kera::DescriptorSetHandle descriptorSet{0, 1};
    const kera::BufferHandle buffer{0, 1};
    const kera::TextureHandle texture{0, 1};
    const kera::SamplerHandle sampler{0, 1};
    const kera::DescriptorSetUpdate update = renderer.updateDescriptors(descriptorSet)
                                                 .uniform<Uniforms>("globalParams", buffer)
                                                 .sampledImage("sceneTexture", texture)
                                                 .sampler("sceneSampler", sampler);
    EXPECT_TRUE(update.ok());

    const kera::DescriptorSetUpdate failedUpdate =
        renderer.updateDescriptors(descriptorSet).sampledImage("globalParams", texture);
    EXPECT_FALSE(failedUpdate.ok());
    EXPECT_FALSE(failedUpdate.errorMessage().empty());
    EXPECT_FALSE(failedUpdate.report().ok());
    ASSERT_EQ(failedUpdate.report().issues.size(), 1u);
    EXPECT_EQ(failedUpdate.report().issues.front().code, kera::RendererErrorCode::ValidationFailed);
    EXPECT_EQ(failedUpdate.report().issues.front().category, kera::RendererValidationCategory::Descriptor);
    EXPECT_FALSE(failedUpdate.result().ok());
    EXPECT_EQ(failedUpdate.result().errorCode(), kera::RendererErrorCode::ValidationFailed);
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
                           .field("position", 0, offsetof(Vertex, position), kera::VertexFormat::Float3)
                           .field("color", 0, offsetof(Vertex, color), kera::VertexFormat::Float3),
                       reflection)
                     .ok());

    EXPECT_FALSE(build(kera::VertexInputLayoutBuilder{}
                           .vertexBinding(0, 0)
                           .field("position", 0, offsetof(Vertex, position), kera::VertexFormat::Float3)
                           .field("color", 0, offsetof(Vertex, color), kera::VertexFormat::Float3),
                       reflection)
                     .ok());

    EXPECT_FALSE(build(kera::VertexInputLayoutBuilder{}
                           .vertexBinding<Vertex>(0)
                           .field("missingPosition", 0, offsetof(Vertex, position), kera::VertexFormat::Float3)
                           .field("color", 0, offsetof(Vertex, color), kera::VertexFormat::Float3),
                       reflection)
                     .ok());

    EXPECT_FALSE(build(kera::VertexInputLayoutBuilder{}
                           .vertexBinding<Vertex>(0)
                           .field("position", 1, offsetof(Vertex, position), kera::VertexFormat::Float3)
                           .field("color", 0, offsetof(Vertex, color), kera::VertexFormat::Float3),
                       reflection)
                     .ok());

    EXPECT_FALSE(build(kera::VertexInputLayoutBuilder{}
                           .vertexBinding<Vertex>(0)
                           .field("position", 0, offsetof(Vertex, position), kera::VertexFormat::Float2)
                           .field("color", 0, offsetof(Vertex, color), kera::VertexFormat::Float3),
                       reflection)
                     .ok());

    EXPECT_FALSE(build(kera::VertexInputLayoutBuilder{}
                           .vertexBinding<Vertex>(0)
                           .field("position", 0, offsetof(Vertex, position), kera::VertexFormat::Float3)
                           .field("position", 0, offsetof(Vertex, color), kera::VertexFormat::Float3),
                       reflection)
                     .ok());

    EXPECT_FALSE(build(kera::VertexInputLayoutBuilder{}
                           .debugName("Unused Field Test")
                           .vertexBinding<Vertex>(0)
                           .field("position", 0, offsetof(Vertex, position), kera::VertexFormat::Float3)
                           .field("color", 0, offsetof(Vertex, color), kera::VertexFormat::Float3)
                           .field("uv", 0, 0, kera::VertexFormat::Float2),
                       reflection)
                     .ok());
}

TEST(KeraReflectionContracts, BuildRejectsMissingOrAmbiguousVertexEntryReflection)
{
    kera::SlangReflectionMetadata missingVertexReflection = makeTriangleReflection();
    missingVertexReflection.entryPoints.clear();
    EXPECT_FALSE(build(makeValidBuilder(), missingVertexReflection).ok());

    kera::SlangReflectionMetadata ambiguousVertexReflection = makeTriangleReflection();
    ambiguousVertexReflection.entryPoints.push_back(ambiguousVertexReflection.entryPoints.front());
    ambiguousVertexReflection.entryPoints.back().name = "otherVertexMain";
    EXPECT_FALSE(build(makeValidBuilder(), ambiguousVertexReflection).ok());
}

TEST(KeraReflectionContracts, BuildExpandsMatrixInputs)
{
    const kera::SlangReflectionMetadata reflection = makeInstancedReflection();

    const kera::VertexInputLayoutBuildResult result =
        build(kera::VertexInputLayoutBuilder{}
                  .vertexBinding<Vertex>(0)
                  .vertexBinding<InstanceData>(1, kera::VertexInputRate::Instance)
                  .field("position", 0, offsetof(Vertex, position), kera::VertexFormat::Float3)
                  .field("color", 0, offsetof(Vertex, color), kera::VertexFormat::Float3)
                  .field("modelMatrix", 1, offsetof(InstanceData, modelMatrix), kera::VertexFormat::Float4),
              reflection);

    ASSERT_TRUE(result.ok()) << result.errorMessage();
    ASSERT_EQ(result.layout().attributes.size(), 6u);
    EXPECT_EQ(result.layout().attributes[2].location, 2u);
    EXPECT_EQ(result.layout().attributes[2].offset, offsetof(InstanceData, modelMatrix));
    EXPECT_EQ(result.layout().attributes[5].location, 5u);
    EXPECT_EQ(result.layout().attributes[5].offset, offsetof(InstanceData, modelMatrix) + sizeof(float) * 12);
}

TEST(KeraReflectionContracts, AmbiguousFieldsRequireParameterName)
{
    kera::SlangReflectionMetadata reflection{};
    reflection.entryPoints.push_back({
        .name = "vertexMain",
        .stage = kera::ShaderType::Vertex,
        .inputs =
            {
                makeInput("vertex", "position", "POSITION", 0, kera::VertexFormat::Float3),
                makeInput("instance", "position", "INSTANCE_POSITION", 1, kera::VertexFormat::Float3),
            },
    });

    EXPECT_FALSE(build(kera::VertexInputLayoutBuilder{}.vertexBinding<Vertex>(0).field(
                           "position", 0, offsetof(Vertex, position), kera::VertexFormat::Float3),
                       reflection)
                     .ok());

    EXPECT_TRUE(build(kera::VertexInputLayoutBuilder{}
                          .vertexBinding<Vertex>(0)
                          .fieldIn("vertex", "position", 0, offsetof(Vertex, position), kera::VertexFormat::Float3)
                          .fieldIn("instance", "position", 0, offsetof(Vertex, color), kera::VertexFormat::Float3),
                      reflection)
                    .ok());
}
