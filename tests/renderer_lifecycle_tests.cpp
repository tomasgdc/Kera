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
    const kera::ELogLevel previous_log_level = kera::Logger::getInstance().getLogLevel();
    kera::Logger::getInstance().setLogLevel(kera::ELogLevel::FATAL);

    kera::CommandBuffer command_buffer;
    EXPECT_FALSE(command_buffer.begin());
    EXPECT_FALSE(command_buffer.end());
    EXPECT_FALSE(command_buffer.reset());
    EXPECT_FALSE(command_buffer.markSubmitted());
    command_buffer.markCompleted();
    EXPECT_FALSE(command_buffer.isRecording());
    EXPECT_FALSE(command_buffer.isPending());

    kera::Logger::getInstance().setLogLevel(previous_log_level);
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
    const kera::DescriptorSetLayoutDesc descriptor_layout{
        .set = 0,
        .bindings =
            {
                {.binding = 0, .type = kera::EDescriptorType::UNIFORM_BUFFER, .stage = kera::EShaderStage::VERTEX},
                {.binding = 1, .type = kera::EDescriptorType::SAMPLED_IMAGE, .stage = kera::EShaderStage::FRAGMENT},
                {.binding = 2, .type = kera::EDescriptorType::SAMPLER, .stage = kera::EShaderStage::FRAGMENT},
            },
    };

    EXPECT_TRUE(kera::descriptorBindingAccepts(descriptor_layout, 0, kera::EDescriptorType::UNIFORM_BUFFER));
    EXPECT_FALSE(kera::descriptorBindingAccepts(descriptor_layout, 0, kera::EDescriptorType::SAMPLED_IMAGE));
    EXPECT_TRUE(kera::descriptorBindingAccepts(descriptor_layout, 1, kera::EDescriptorType::SAMPLED_IMAGE));
    EXPECT_TRUE(kera::descriptorBindingAccepts(descriptor_layout, 2, kera::EDescriptorType::SAMPLER));
    EXPECT_FALSE(kera::descriptorBindingAccepts(descriptor_layout, 3, kera::EDescriptorType::SAMPLER));

    kera::DescriptorSetLayoutDesc reordered_descriptor_layout{
        .set = 0,
        .bindings =
            {
                {.binding = 2, .type = kera::EDescriptorType::SAMPLER, .stage = kera::EShaderStage::FRAGMENT},
                {.binding = 1, .type = kera::EDescriptorType::SAMPLED_IMAGE, .stage = kera::EShaderStage::FRAGMENT},
                {.binding = 0, .type = kera::EDescriptorType::UNIFORM_BUFFER, .stage = kera::EShaderStage::VERTEX},
            },
    };
    EXPECT_TRUE(kera::descriptorSetLayoutsCompatible(descriptor_layout, reordered_descriptor_layout));

    reordered_descriptor_layout.bindings[0].type = kera::EDescriptorType::UNIFORM_BUFFER;
    EXPECT_FALSE(kera::descriptorSetLayoutsCompatible(descriptor_layout, reordered_descriptor_layout));
}

TEST(KeraRendererLifecycle, ResourceDescriptorsHaveStableDefaults)
{
    kera::GraphicsPipelineDesc default_pipeline_desc{};
    EXPECT_EQ(default_pipeline_desc.blend_mode, kera::EBlendModeKind::OPAQUE);

    kera::TextureDesc default_texture_desc{};
    EXPECT_EQ(default_texture_desc.mip_levels, 1u);
    EXPECT_FALSE(default_texture_desc.generate_mipmaps);
    EXPECT_EQ(kera::textureFullMipLevelCount(1, 1), 1u);
    EXPECT_EQ(kera::textureFullMipLevelCount(1024, 512), 11u);

    kera::SamplerDesc default_sampler_desc{};
    EXPECT_EQ(default_sampler_desc.mip_filter, kera::ESamplerMipFilter::LINEAR);
    EXPECT_EQ(default_sampler_desc.min_lod, 0.0f);
    EXPECT_EQ(default_sampler_desc.max_lod, 0.0f);
    EXPECT_EQ(default_sampler_desc.max_anisotropy, 1.0f);
    default_sampler_desc.address_mode_u = kera::ESamplerAddressMode::MIRRORED_REPEAT;
    EXPECT_EQ(default_sampler_desc.address_mode_u, kera::ESamplerAddressMode::MIRRORED_REPEAT);
}

TEST(KeraRendererLifecycle, RendererResultAndValidationReportPreserveErrors)
{
    const auto result =
        kera::RendererResult<void>::failure(kera::ERendererErrorCode::INVALID_STATE, "renderer state rejected");
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.errorCode(), kera::ERendererErrorCode::INVALID_STATE);
    EXPECT_FALSE(result.errorMessage().empty());

    kera::RendererValidationReport report;
    report.addIssue(kera::ERendererErrorCode::INVALID_HANDLE, kera::ERendererValidationCategory::DESCRIPTOR,
                    "invalid texture", 1, 2, "albedo");
    EXPECT_FALSE(report.ok());
    ASSERT_FALSE(report.issues.empty());
    EXPECT_EQ(report.issues.front().code, kera::ERendererErrorCode::INVALID_HANDLE);
    EXPECT_EQ(report.issues.front().category, kera::ERendererValidationCategory::DESCRIPTOR);
    EXPECT_EQ(kera::rendererValidationCategoryName(report.issues.front().category), std::string("Descriptor"));
    EXPECT_EQ(kera::textureFormatBytesPerPixel(kera::ETextureFormat::RGBA8), 4u);
    EXPECT_EQ(kera::textureFormatBytesPerPixel(kera::ETextureFormat::RGB_A8_SRGB), 4u);
    EXPECT_EQ(kera::textureFormatBytesPerPixel(kera::ETextureFormat::DEPTH32), 4u);
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
