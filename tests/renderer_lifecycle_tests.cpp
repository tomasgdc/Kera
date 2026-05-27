#include "kera/renderer/backend/vulkan/layout_utils.h"
#include "kera/renderer/command_buffer.h"
#include "kera/renderer/descriptor_contracts.h"
#include "kera/renderer/descriptors.h"
#include "kera/renderer/resource_registry.h"
#include "kera/renderer/result.h"
#include "kera/utilities/logger.h"

#include <iostream>
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

    int g_failures = 0;

    void expect(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            ++g_failures;
        }
    }
}

int main()
{
    const kera::LogLevel previousLogLevel = kera::Logger::getInstance().getLogLevel();
    kera::Logger::getInstance().setLogLevel(kera::LogLevel::Fatal);

    kera::CommandBuffer commandBuffer;
    expect(!commandBuffer.begin(), "uninitialized command buffer begin should fail");
    expect(!commandBuffer.end(), "uninitialized command buffer end should fail");
    expect(!commandBuffer.reset(), "uninitialized command buffer reset should fail");
    expect(!commandBuffer.markSubmitted(), "uninitialized command buffer submit marker should fail");
    commandBuffer.markCompleted();
    expect(!commandBuffer.isRecording(), "uninitialized command buffer should not report recording");
    expect(!commandBuffer.isPending(), "uninitialized command buffer should not report pending");

    kera::Logger::getInstance().setLogLevel(previousLogLevel);

    kera::ResourceRegistry<TestResource, TestHandle> registry;
    TestHandle handle = registry.emplace(TestResource{42});
    expect(handle.isValid(), "emplaced resource handle should be valid");
    expect(registry.activeCount() == 1, "registry should report one active resource");
    expect(registry.get(handle) != nullptr, "registry should resolve active handle");
    expect(registry.remove(handle), "registry should remove active handle");
    expect(registry.get(handle) == nullptr, "registry should reject stale handle");
    expect(registry.activeCount() == 0, "registry should report zero active resources after remove");

    const kera::DescriptorSetLayoutDesc descriptorLayout{
        .set = 0,
        .bindings =
            {
                {.binding = 0, .type = kera::DescriptorType::UniformBuffer, .stage = kera::ShaderStage::Vertex},
                {.binding = 1, .type = kera::DescriptorType::SampledImage, .stage = kera::ShaderStage::Fragment},
                {.binding = 2, .type = kera::DescriptorType::Sampler, .stage = kera::ShaderStage::Fragment},
            },
    };

    expect(kera::descriptorBindingAccepts(descriptorLayout, 0, kera::DescriptorType::UniformBuffer),
           "uniform buffer binding should accept uniform buffer updates");
    expect(!kera::descriptorBindingAccepts(descriptorLayout, 0, kera::DescriptorType::SampledImage),
           "uniform buffer binding should reject sampled image updates");
    expect(kera::descriptorBindingAccepts(descriptorLayout, 1, kera::DescriptorType::SampledImage),
           "sampled image binding should accept sampled image updates");
    expect(kera::descriptorBindingAccepts(descriptorLayout, 2, kera::DescriptorType::Sampler),
           "sampler binding should accept sampler updates");
    expect(!kera::descriptorBindingAccepts(descriptorLayout, 3, kera::DescriptorType::Sampler),
           "missing binding should reject updates");

    kera::DescriptorSetLayoutDesc reorderedDescriptorLayout{
        .set = 0,
        .bindings =
            {
                {.binding = 2, .type = kera::DescriptorType::Sampler, .stage = kera::ShaderStage::Fragment},
                {.binding = 1, .type = kera::DescriptorType::SampledImage, .stage = kera::ShaderStage::Fragment},
                {.binding = 0, .type = kera::DescriptorType::UniformBuffer, .stage = kera::ShaderStage::Vertex},
            },
    };
    expect(kera::descriptorSetLayoutsCompatible(descriptorLayout, reorderedDescriptorLayout),
           "descriptor layout compatibility should not depend on binding order");

    kera::GraphicsPipelineDesc defaultPipelineDesc{};
    expect(defaultPipelineDesc.blendMode == kera::BlendModeKind::Opaque,
           "graphics pipeline blend mode should default to opaque");

    kera::TextureDesc defaultTextureDesc{};
    expect(defaultTextureDesc.mipLevels == 1, "texture mip levels should default to one");
    expect(!defaultTextureDesc.generateMipmaps, "texture mip generation should default off");
    expect(kera::textureFullMipLevelCount(1, 1) == 1, "1x1 textures should have one full mip level");
    expect(kera::textureFullMipLevelCount(1024, 512) == 11,
           "full mip count should follow the largest texture dimension");

    kera::SamplerDesc defaultSamplerDesc{};
    expect(defaultSamplerDesc.mipFilter == kera::SamplerMipFilter::Linear,
           "sampler mip filter should default to linear");
    expect(defaultSamplerDesc.minLod == 0.0f, "sampler min LOD should default to zero");
    expect(defaultSamplerDesc.maxLod == 0.0f, "sampler max LOD should default to zero");
    expect(defaultSamplerDesc.maxAnisotropy == 1.0f, "sampler anisotropy should default off");
    defaultSamplerDesc.addressModeU = kera::SamplerAddressMode::MirroredRepeat;
    expect(defaultSamplerDesc.addressModeU == kera::SamplerAddressMode::MirroredRepeat,
           "sampler address mode should expose mirrored repeat for glTF samplers");

    reorderedDescriptorLayout.bindings[0].type = kera::DescriptorType::UniformBuffer;
    expect(!kera::descriptorSetLayoutsCompatible(descriptorLayout, reorderedDescriptorLayout),
           "descriptor layout compatibility should reject binding type changes");

    const auto result =
        kera::RendererResult<void>::failure(kera::RendererErrorCode::InvalidState, "renderer state rejected");
    expect(!result.ok(), "failed renderer result should not report ok");
    expect(result.errorCode() == kera::RendererErrorCode::InvalidState,
           "renderer result should preserve typed error code");
    expect(!result.errorMessage().empty(), "renderer result should preserve error message");

    kera::RendererValidationReport report;
    report.addIssue(kera::RendererErrorCode::InvalidHandle, kera::RendererValidationCategory::Descriptor,
                    "invalid texture", 1, 2, "albedo");
    expect(!report.ok(), "validation report with issues should not report ok");
    expect(report.issues.front().code == kera::RendererErrorCode::InvalidHandle,
           "validation report should preserve typed issue code");
    expect(report.issues.front().category == kera::RendererValidationCategory::Descriptor,
           "validation report should preserve issue category");
    expect(kera::rendererValidationCategoryName(report.issues.front().category) == std::string("Descriptor"),
           "validation category name should be stable");
    expect(kera::textureFormatBytesPerPixel(kera::TextureFormat::RGBA8) == 4,
           "RGBA8 texture format should report four bytes per pixel");
    expect(kera::textureFormatBytesPerPixel(kera::TextureFormat::RGBA8Srgb) == 4,
           "RGBA8Srgb texture format should report four bytes per pixel");
    expect(kera::textureFormatBytesPerPixel(kera::TextureFormat::Depth32) == 4,
           "Depth32 texture format should report four bytes per pixel");

    expect(
        kera::vulkanStageMaskForImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) == VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        "texture upload transfer-dst layout should map to transfer stage");
    expect(kera::vulkanAccessMaskForImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) == VK_ACCESS_2_TRANSFER_WRITE_BIT,
           "texture upload transfer-dst layout should map to transfer write access");
    expect(kera::vulkanStageMaskForImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) ==
               VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
           "texture upload shader-read layout should map to fragment shader stage");
    expect(kera::vulkanAccessMaskForImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) ==
               VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
           "texture upload shader-read layout should map to sampled-read access");

    return g_failures == 0 ? 0 : 1;
}
