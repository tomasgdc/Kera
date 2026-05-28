// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/backend/vulkan/layout_utils.h"
#include "kera/renderer/command_buffer.h"
#include "kera/renderer/descriptor_contracts.h"
#include "kera/renderer/descriptors.h"
#include "kera/renderer/resource_registry.h"
#include "kera/renderer/result.h"
#include "kera/utilities/logger.h"

#include <gtest/gtest.h>

#include <string>

namespace
{
    struct TestHandleTag
    {
    };

    using TestHandle = kera::Handle<TestHandleTag>;

    struct TestResource
    {
        int value = 0;
    };

}

TEST(KeraRendererLifecycle, RejectsUninitializedCommandBufferOperations)
{
    const kera::LogLevel previousLogLevel = kera::Logger::getInstance().getLogLevel();
    kera::Logger::getInstance().setLogLevel(kera::LogLevel::Fatal);

    kera::CommandBuffer commandBuffer;
    EXPECT_FALSE(commandBuffer.begin());
    EXPECT_FALSE(commandBuffer.end());
    EXPECT_FALSE(commandBuffer.reset());
    EXPECT_FALSE(commandBuffer.markSubmitted());
    commandBuffer.markCompleted();
    EXPECT_FALSE(commandBuffer.isRecording());
    EXPECT_FALSE(commandBuffer.isPending());

    kera::Logger::getInstance().setLogLevel(previousLogLevel);
}

TEST(KeraRendererLifecycle, ResourceRegistryRejectsStaleHandles)
{
    kera::ResourceRegistry<TestResource, TestHandle> registry;
    TestHandle handle = registry.emplace(TestResource{42});
    EXPECT_TRUE(handle.isValid());
    EXPECT_EQ(registry.activeCount(), 1u);
    EXPECT_NE(registry.get(handle), nullptr);
    EXPECT_TRUE(registry.remove(handle));
    EXPECT_EQ(registry.get(handle), nullptr);
    EXPECT_EQ(registry.activeCount(), 0u);
}

TEST(KeraRendererLifecycle, DescriptorLayoutsMatchByBindingNumber)
{
    const kera::DescriptorSetLayoutDesc descriptorLayout{
        .set = 0,
        .bindings =
            {
                {.binding = 0, .type = kera::DescriptorType::UniformBuffer, .stage = kera::ShaderStage::Vertex},
                {.binding = 1, .type = kera::DescriptorType::SampledImage, .stage = kera::ShaderStage::Fragment},
                {.binding = 2, .type = kera::DescriptorType::Sampler, .stage = kera::ShaderStage::Fragment},
            },
    };

    EXPECT_TRUE(kera::descriptorBindingAccepts(descriptorLayout, 0, kera::DescriptorType::UniformBuffer));
    EXPECT_FALSE(kera::descriptorBindingAccepts(descriptorLayout, 0, kera::DescriptorType::SampledImage));
    EXPECT_TRUE(kera::descriptorBindingAccepts(descriptorLayout, 1, kera::DescriptorType::SampledImage));
    EXPECT_TRUE(kera::descriptorBindingAccepts(descriptorLayout, 2, kera::DescriptorType::Sampler));
    EXPECT_FALSE(kera::descriptorBindingAccepts(descriptorLayout, 3, kera::DescriptorType::Sampler));

    kera::DescriptorSetLayoutDesc reorderedDescriptorLayout{
        .set = 0,
        .bindings =
            {
                {.binding = 2, .type = kera::DescriptorType::Sampler, .stage = kera::ShaderStage::Fragment},
                {.binding = 1, .type = kera::DescriptorType::SampledImage, .stage = kera::ShaderStage::Fragment},
                {.binding = 0, .type = kera::DescriptorType::UniformBuffer, .stage = kera::ShaderStage::Vertex},
            },
    };
    EXPECT_TRUE(kera::descriptorSetLayoutsCompatible(descriptorLayout, reorderedDescriptorLayout));

    reorderedDescriptorLayout.bindings[0].type = kera::DescriptorType::UniformBuffer;
    EXPECT_FALSE(kera::descriptorSetLayoutsCompatible(descriptorLayout, reorderedDescriptorLayout));
}

TEST(KeraRendererLifecycle, ResourceDescriptorsHaveStableDefaults)
{
    kera::GraphicsPipelineDesc defaultPipelineDesc{};
    EXPECT_EQ(defaultPipelineDesc.blendMode, kera::BlendModeKind::Opaque);

    kera::TextureDesc defaultTextureDesc{};
    EXPECT_EQ(defaultTextureDesc.mipLevels, 1u);
    EXPECT_FALSE(defaultTextureDesc.generateMipmaps);
    EXPECT_EQ(kera::textureFullMipLevelCount(1, 1), 1u);
    EXPECT_EQ(kera::textureFullMipLevelCount(1024, 512), 11u);

    kera::SamplerDesc defaultSamplerDesc{};
    EXPECT_EQ(defaultSamplerDesc.mipFilter, kera::SamplerMipFilter::Linear);
    EXPECT_EQ(defaultSamplerDesc.minLod, 0.0f);
    EXPECT_EQ(defaultSamplerDesc.maxLod, 0.0f);
    EXPECT_EQ(defaultSamplerDesc.maxAnisotropy, 1.0f);
    defaultSamplerDesc.addressModeU = kera::SamplerAddressMode::MirroredRepeat;
    EXPECT_EQ(defaultSamplerDesc.addressModeU, kera::SamplerAddressMode::MirroredRepeat);
}

TEST(KeraRendererLifecycle, RendererResultAndValidationReportPreserveErrors)
{
    const auto result =
        kera::RendererResult<void>::failure(kera::RendererErrorCode::InvalidState, "renderer state rejected");
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.errorCode(), kera::RendererErrorCode::InvalidState);
    EXPECT_FALSE(result.errorMessage().empty());

    kera::RendererValidationReport report;
    report.addIssue(kera::RendererErrorCode::InvalidHandle, kera::RendererValidationCategory::Descriptor,
                    "invalid texture", 1, 2, "albedo");
    EXPECT_FALSE(report.ok());
    ASSERT_FALSE(report.issues.empty());
    EXPECT_EQ(report.issues.front().code, kera::RendererErrorCode::InvalidHandle);
    EXPECT_EQ(report.issues.front().category, kera::RendererValidationCategory::Descriptor);
    EXPECT_EQ(kera::rendererValidationCategoryName(report.issues.front().category), std::string("Descriptor"));
    EXPECT_EQ(kera::textureFormatBytesPerPixel(kera::TextureFormat::RGBA8), 4u);
    EXPECT_EQ(kera::textureFormatBytesPerPixel(kera::TextureFormat::RGBA8Srgb), 4u);
    EXPECT_EQ(kera::textureFormatBytesPerPixel(kera::TextureFormat::Depth32), 4u);
}

TEST(KeraRendererLifecycle, VulkanImageLayoutMasksUseSync2Stages)
{
    EXPECT_EQ(kera::vulkanStageMaskForImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
              VK_PIPELINE_STAGE_2_TRANSFER_BIT);
    EXPECT_EQ(kera::vulkanAccessMaskForImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
              VK_ACCESS_2_TRANSFER_WRITE_BIT);
    EXPECT_EQ(kera::vulkanStageMaskForImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
              VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
    EXPECT_EQ(kera::vulkanAccessMaskForImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
              VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
}
