#include "kera/renderer/command_buffer.h"
#include "kera/renderer/descriptor_contracts.h"
#include "kera/renderer/descriptors.h"
#include "kera/renderer/resource_registry.h"

#include <iostream>

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
    kera::CommandBuffer commandBuffer;
    expect(!commandBuffer.begin(), "uninitialized command buffer begin should fail");
    expect(!commandBuffer.end(), "uninitialized command buffer end should fail");
    expect(!commandBuffer.reset(), "uninitialized command buffer reset should fail");
    expect(!commandBuffer.markSubmitted(), "uninitialized command buffer submit marker should fail");
    commandBuffer.markCompleted();
    expect(!commandBuffer.isRecording(), "uninitialized command buffer should not report recording");
    expect(!commandBuffer.isPending(), "uninitialized command buffer should not report pending");

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

    reorderedDescriptorLayout.bindings[0].type = kera::DescriptorType::UniformBuffer;
    expect(!kera::descriptorSetLayoutsCompatible(descriptorLayout, reorderedDescriptorLayout),
           "descriptor layout compatibility should reject binding type changes");

    return g_failures == 0 ? 0 : 1;
}
