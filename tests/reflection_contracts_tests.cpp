#include "kera/renderer/interfaces.h"
#include "kera/renderer/reflection_contracts.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{
    struct Vertex
    {
        float position[3];
        float color[3];
    };

    struct Uniforms
    {
        float matrix[16];
    };

    kera::SlangReflectionMetadata makeReflection()
    {
        kera::SlangReflectionMetadata reflection{};
        reflection.entryPoints.push_back({
            .name = "vertexMain",
            .stage = kera::ShaderType::Vertex,
            .inputs =
                {
                    {
                        .name = "position",
                        .semanticName = "POSITION",
                        .location = 0,
                        .locationCount = 1,
                    },
                    {
                        .name = "color",
                        .semanticName = "COLOR",
                        .location = 1,
                        .locationCount = 1,
                    },
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

    kera::PipelineReflectionContract makeValidContract()
    {
        return kera::PipelineReflectionBuilder{}
            .debugName("Reflection Contract Test")
            .vertexEntry("vertexMain")
            .vertexBinding<Vertex>("meshVertex", 0)
            .semantic("POSITION", "meshVertex", offsetof(Vertex, position), kera::VertexFormat::Float3)
            .semantic("COLOR", "meshVertex", offsetof(Vertex, color), kera::VertexFormat::Float3)
            .uniform<Uniforms>("globalParams")
            .build();
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
        std::size_t getUniformRingBufferOffset(kera::BufferHandle, kera::FrameHandle) const override
        {
            return 0;
        }

        kera::TextureHandle createTexture(const kera::TextureDesc&) override
        {
            return {};
        }
        bool uploadTexture(kera::TextureHandle, const void*, std::size_t) override
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
        bool updateDescriptorSet(kera::DescriptorSetHandle, const std::string& name, kera::BufferHandle,
                                 std::size_t, std::size_t) override
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

int main()
{
    const kera::SlangReflectionMetadata reflection = makeReflection();
    const kera::PipelineReflectionContract validContract = makeValidContract();
    const kera::ReflectedPipelineContract validContractView = validContract.view();
    assert(validContractView.debugName == "Reflection Contract Test");
    assert(validContractView.vertexBindings.size() == 1);
    assert(validContractView.vertexBindings.front().stride == sizeof(Vertex));

    kera::VertexLayoutDesc layout{};
    assert(kera::appendValidatedReflectedPipelineContract(layout, &reflection, validContract));
    assert(layout.bindings.size() == 1);
    assert(layout.attributes.size() == 2);

    kera::VertexLayoutDesc unusedLayout{};
    const auto unusedContract = kera::PipelineReflectionBuilder{}
                                    .debugName("Unused Semantic Test")
                                    .vertexEntry("vertexMain")
                                    .vertexBinding<Vertex>("meshVertex", 0)
                                    .semantic("POSITION", "meshVertex", offsetof(Vertex, position),
                                              kera::VertexFormat::Float3)
                                    .semantic("COLOR", "meshVertex", offsetof(Vertex, color),
                                              kera::VertexFormat::Float3)
                                    .semantic("TEXCOORD0", "meshVertex", 0, kera::VertexFormat::Float2)
                                    .uniform<Uniforms>("globalParams")
                                    .build();
    assert(!kera::appendValidatedReflectedPipelineContract(unusedLayout, &reflection, unusedContract));

    kera::VertexLayoutDesc duplicateLayout{};
    const auto duplicateContract = kera::PipelineReflectionBuilder{}
                                       .vertexEntry("vertexMain")
                                       .vertexBinding<Vertex>("meshVertex", 0)
                                       .semantic("POSITION", "meshVertex", offsetof(Vertex, position),
                                                 kera::VertexFormat::Float3)
                                       .semantic("POSITION", "meshVertex", offsetof(Vertex, color),
                                                 kera::VertexFormat::Float3)
                                       .uniform<Uniforms>("globalParams")
                                       .build();
    assert(!kera::appendValidatedReflectedPipelineContract(duplicateLayout, &reflection, duplicateContract));

    FakeRenderer renderer;
    const kera::DescriptorSetHandle descriptorSet{0, 1};
    const kera::BufferHandle buffer{0, 1};
    const kera::TextureHandle texture{0, 1};
    const kera::SamplerHandle sampler{0, 1};
    const kera::DescriptorSetUpdate update = renderer.updateDescriptors(descriptorSet)
               .uniform<Uniforms>("globalParams", buffer)
               .sampledImage("sceneTexture", texture)
               .sampler("sceneSampler", sampler);
    assert(update.ok());
    const kera::DescriptorSetUpdate failedUpdate =
        renderer.updateDescriptors(descriptorSet).sampledImage("globalParams", texture);
    assert(!failedUpdate.ok());
    assert(!failedUpdate.errorMessage().empty());
    assert(!failedUpdate.report().ok());
    assert(failedUpdate.report().issues.size() == 1);
    assert(failedUpdate.report().issues.front().code == kera::RendererErrorCode::ValidationFailed);
    assert(failedUpdate.report().issues.front().category == kera::RendererValidationCategory::Descriptor);
    assert(!failedUpdate.result().ok());
    assert(failedUpdate.result().errorCode() == kera::RendererErrorCode::ValidationFailed);
    assert(!renderer.tryCreateGraphicsShaderProgram({}).ok());
    assert(!renderer.tryCreateGraphicsPipeline({}).ok());
    assert(!renderer.tryCreateDescriptorSet({0, 1}).ok());

    return 0;
}
