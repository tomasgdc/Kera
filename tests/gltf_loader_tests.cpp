#include "kera/renderer/gltf_loader.h"
#include "kera/renderer/interfaces.h"

#include <cassert>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace
{
#if defined(_CPPUNWIND) || defined(__EXCEPTIONS)
#error "Kera tests must compile with C++ exceptions disabled."
#endif

    class NullRenderer final : public kera::IRenderer
    {
    public:
        explicit NullRenderer(bool createResources = false) : m_createResources(createResources) {}

        std::vector<kera::BufferDesc> bufferDescs;
        std::vector<kera::TextureDesc> textureDescs;
        std::vector<kera::SamplerDesc> samplerDescs;
        std::vector<kera::GltfVertex> uploadedVertices;

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
            return false;
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

        kera::BufferHandle createBuffer(const kera::BufferDesc& desc) override
        {
            if (!m_createResources)
            {
                return {};
            }
            bufferDescs.push_back(desc);
            return {m_nextBuffer++, 1};
        }
        bool destroyBuffer(kera::BufferHandle) override
        {
            return m_createResources;
        }
        bool mapBuffer(kera::BufferHandle, void**) override
        {
            return false;
        }
        void unmapBuffer(kera::BufferHandle) override {}
        bool uploadBuffer(kera::BufferHandle, const void* data, std::size_t size, std::size_t) override
        {
            if (!m_createResources || !data)
            {
                return false;
            }
            if (size >= sizeof(kera::GltfVertex) && size % sizeof(kera::GltfVertex) == 0 &&
                uploadedVertices.empty())
            {
                uploadedVertices.resize(size / sizeof(kera::GltfVertex));
                std::memcpy(uploadedVertices.data(), data, size);
            }
            return true;
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

        kera::TextureHandle createTexture(const kera::TextureDesc& desc) override
        {
            if (!m_createResources)
            {
                return {};
            }
            textureDescs.push_back(desc);
            return {m_nextTexture++, 1};
        }
        bool uploadTexture(kera::TextureHandle, const void* data, std::size_t size) override
        {
            return m_createResources && data && size > 0;
        }
        bool destroyTexture(kera::TextureHandle) override
        {
            return m_createResources;
        }
        kera::SamplerHandle createSampler(const kera::SamplerDesc& desc) override
        {
            if (!m_createResources)
            {
                return {};
            }
            samplerDescs.push_back(desc);
            return {m_nextSampler++, 1};
        }
        bool destroySampler(kera::SamplerHandle) override
        {
            return m_createResources;
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
        bool updateDescriptorSet(kera::DescriptorSetHandle, const std::string&, kera::BufferHandle, std::size_t,
                                 std::size_t) override
        {
            return false;
        }
        bool updateDescriptorSet(kera::DescriptorSetHandle, const std::string&, kera::TextureHandle) override
        {
            return false;
        }
        bool updateDescriptorSet(kera::DescriptorSetHandle, const std::string&, kera::SamplerHandle) override
        {
            return false;
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

    private:
        bool m_createResources = false;
        int32_t m_nextBuffer = 0;
        int32_t m_nextTexture = 0;
        int32_t m_nextSampler = 0;
    };

    template <typename T>
    void appendValue(std::vector<uint8_t>& bytes, const T& value)
    {
        const uint8_t* raw = reinterpret_cast<const uint8_t*>(&value);
        bytes.insert(bytes.end(), raw, raw + sizeof(T));
    }

    void appendFloat(std::vector<uint8_t>& bytes, float value)
    {
        appendValue(bytes, value);
    }

    void appendUInt16(std::vector<uint8_t>& bytes, uint16_t value)
    {
        appendValue(bytes, value);
    }

    bool writeTangentGltfFixture(const std::string& basePath, bool authoredTangents)
    {
        std::vector<uint8_t> bytes;

        const float positions[] = {
            0.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
        };
        const float normals[] = {
            0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 1.0f,
        };
        const float uvs[] = {
            0.0f, 0.0f,
            1.0f, 0.0f,
            0.0f, 1.0f,
        };
        const float tangents[] = {
            0.0f, 2.0f, 0.0f, -1.0f,
            0.0f, 2.0f, 0.0f, -1.0f,
            0.0f, 2.0f, 0.0f, -1.0f,
        };
        const uint16_t indices[] = {0, 1, 2};

        for (float value : positions)
        {
            appendFloat(bytes, value);
        }
        for (float value : normals)
        {
            appendFloat(bytes, value);
        }
        for (float value : uvs)
        {
            appendFloat(bytes, value);
        }
        if (authoredTangents)
        {
            for (float value : tangents)
            {
                appendFloat(bytes, value);
            }
        }
        const size_t indexByteOffset = bytes.size();
        for (uint16_t value : indices)
        {
            appendUInt16(bytes, value);
        }

        const std::string binPath = basePath + ".bin";
        {
            std::ofstream bin(binPath, std::ios::binary);
            if (!bin)
            {
                return false;
            }
            bin.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            if (!bin)
            {
                return false;
            }
        }

        const std::string gltfPath = basePath + ".gltf";
        std::ofstream gltf(gltfPath);
        if (!gltf)
        {
            return false;
        }

        gltf << "{\n"
             << "  \"asset\": {\"version\": \"2.0\"},\n"
             << "  \"buffers\": [{\"uri\": \"" << basePath << ".bin\", \"byteLength\": " << bytes.size()
             << "}],\n"
             << "  \"bufferViews\": [\n"
             << "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36, \"target\": 34962},\n"
             << "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 36, \"target\": 34962},\n"
             << "    {\"buffer\": 0, \"byteOffset\": 72, \"byteLength\": 24, \"target\": 34962}";
        if (authoredTangents)
        {
            gltf << ",\n"
                 << "    {\"buffer\": 0, \"byteOffset\": 96, \"byteLength\": 48, \"target\": 34962}";
        }
        gltf << ",\n"
             << "    {\"buffer\": 0, \"byteOffset\": " << indexByteOffset
             << ", \"byteLength\": 6, \"target\": 34963}\n"
             << "  ],\n"
             << "  \"accessors\": [\n"
             << "    {\"bufferView\": 0, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\"},\n"
             << "    {\"bufferView\": 1, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\"},\n"
             << "    {\"bufferView\": 2, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC2\"}";
        if (authoredTangents)
        {
            gltf << ",\n"
                 << "    {\"bufferView\": 3, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC4\"}";
        }
        const int indexAccessor = authoredTangents ? 4 : 3;
        const int indexBufferView = authoredTangents ? 4 : 3;
        gltf << ",\n"
             << "    {\"bufferView\": " << indexBufferView
             << ", \"componentType\": 5123, \"count\": 3, \"type\": \"SCALAR\"}\n"
             << "  ],\n"
             << "  \"materials\": [{\n"
             << "    \"alphaMode\": \"MASK\",\n"
             << "    \"alphaCutoff\": 0.42,\n"
             << "    \"doubleSided\": true,\n"
             << "    \"pbrMetallicRoughness\": {\n"
             << "      \"baseColorFactor\": [0.2, 0.3, 0.4, 0.6],\n"
             << "      \"metallicFactor\": 0.25,\n"
             << "      \"roughnessFactor\": 0.75\n"
             << "    }\n"
             << "  }],\n"
             << "  \"meshes\": [{\"primitives\": [{\"attributes\": {\"POSITION\": 0, \"NORMAL\": 1, \"TEXCOORD_0\": 2";
        if (authoredTangents)
        {
            gltf << ", \"TANGENT\": 3";
        }
        gltf << "}, \"indices\": " << indexAccessor << ", \"material\": 0}]}],\n"
             << "  \"nodes\": [{\"mesh\": 0}],\n"
             << "  \"scenes\": [{\"nodes\": [0]}],\n"
             << "  \"scene\": 0\n"
             << "}\n";

        return static_cast<bool>(gltf);
    }
}  // namespace

int main()
{
    NullRenderer renderer;

    const kera::RendererResult<kera::GltfLoadedModel> emptyPath =
        kera::loadGltfModel(renderer, {.path = "", .debugName = "Empty"});
    assert(!emptyPath.ok());
    assert(emptyPath.errorCode() == kera::RendererErrorCode::ValidationFailed);
    assert(!emptyPath.errorMessage().empty());

    const kera::RendererResult<kera::GltfLoadedModel> missingFile =
        kera::loadGltfModel(renderer, {.path = "missing_file_that_should_not_exist.gltf", .debugName = "Missing"});
    assert(!missingFile.ok());
    assert(missingFile.errorCode() == kera::RendererErrorCode::ValidationFailed);
    assert(!missingFile.errorMessage().empty());

    NullRenderer resourceRenderer(true);
    const std::string damagedHelmetPath =
        std::string(KERA_SOURCE_DIR) + "/samples/assets/gltf/DamagedHelmet/DamagedHelmet.gltf";
    kera::RendererResult<kera::GltfLoadedModel> damagedHelmet =
        kera::loadGltfModel(resourceRenderer, {.path = damagedHelmetPath, .debugName = "DamagedHelmet"});
    assert(damagedHelmet.ok());
    assert(damagedHelmet.value().indexCount == 46356);
    assert(damagedHelmet.value().materialFactors.baseColor == glm::vec4(1.0f));
    assert(damagedHelmet.value().materialFactors.emissive == glm::vec3(1.0f));
    assert(damagedHelmet.value().materialFactors.metallic == 1.0f);
    assert(damagedHelmet.value().materialFactors.roughness == 1.0f);
    assert(damagedHelmet.value().materialFactors.normalScale == 1.0f);
    assert(damagedHelmet.value().materialFactors.occlusionStrength == 1.0f);
    assert(damagedHelmet.value().materialFactors.alphaCutoff == 0.5f);
    assert(damagedHelmet.value().materialFactors.alphaMode == kera::GltfAlphaMode::Opaque);
    assert(!damagedHelmet.value().materialFactors.doubleSided);

    assert(resourceRenderer.textureDescs.size() == 5);
    assert(resourceRenderer.textureDescs[0].format == kera::TextureFormat::RGBA8Srgb);
    assert(resourceRenderer.textureDescs[1].format == kera::TextureFormat::RGBA8);
    assert(resourceRenderer.textureDescs[2].format == kera::TextureFormat::RGBA8Srgb);
    assert(resourceRenderer.textureDescs[3].format == kera::TextureFormat::RGBA8);
    assert(resourceRenderer.textureDescs[4].format == kera::TextureFormat::RGBA8);
    for (const kera::TextureDesc& textureDesc : resourceRenderer.textureDescs)
    {
        assert(textureDesc.generateMipmaps);
        assert(textureDesc.mipLevels == kera::textureFullMipLevelCount(textureDesc.width, textureDesc.height));
        assert(textureDesc.mipLevels > 1);
    }
    assert(resourceRenderer.samplerDescs.size() == 1);
    assert(resourceRenderer.samplerDescs[0].minFilter == kera::SamplerFilter::Linear);
    assert(resourceRenderer.samplerDescs[0].magFilter == kera::SamplerFilter::Linear);
    assert(resourceRenderer.samplerDescs[0].mipFilter == kera::SamplerMipFilter::Linear);
    assert(resourceRenderer.samplerDescs[0].addressModeU == kera::SamplerAddressMode::Repeat);
    assert(resourceRenderer.samplerDescs[0].addressModeV == kera::SamplerAddressMode::Repeat);
    assert(resourceRenderer.samplerDescs[0].maxLod > 1.0f);
    assert(resourceRenderer.samplerDescs[0].maxAnisotropy == 1.0f);
    assert(!resourceRenderer.uploadedVertices.empty());

    bool foundGeneratedTangent = false;
    for (const kera::GltfVertex& vertex : resourceRenderer.uploadedVertices)
    {
        const float tangentLengthSquared = glm::dot(glm::vec3(vertex.tangent), glm::vec3(vertex.tangent));
        if (tangentLengthSquared > 0.5f && std::abs(std::abs(vertex.tangent.w) - 1.0f) < 0.001f)
        {
            foundGeneratedTangent = true;
            break;
        }
    }
    assert(foundGeneratedTangent);
    kera::destroyGltfModel(resourceRenderer, damagedHelmet.value());

    const std::string authoredBasePath = "authored_tangent_material";
    assert(writeTangentGltfFixture(authoredBasePath, true));

    NullRenderer authoredRenderer(true);
    kera::RendererResult<kera::GltfLoadedModel> authoredModel =
        kera::loadGltfModel(authoredRenderer, {
                                                 .path = authoredBasePath + ".gltf",
                                                 .debugName = "Authored Tangent Material",
                                                 .requireMaterialTextures = false,
                                             });
    assert(authoredModel.ok());
    assert(authoredModel.value().indexCount == 3);
    assert(authoredModel.value().materialFactors.baseColor == glm::vec4(0.2f, 0.3f, 0.4f, 0.6f));
    assert(authoredModel.value().materialFactors.emissive == glm::vec3(0.0f));
    assert(authoredModel.value().materialFactors.metallic == 0.25f);
    assert(authoredModel.value().materialFactors.roughness == 0.75f);
    assert(authoredModel.value().materialFactors.alphaCutoff == 0.42f);
    assert(authoredModel.value().materialFactors.alphaMode == kera::GltfAlphaMode::Mask);
    assert(authoredModel.value().materialFactors.doubleSided);
    assert(authoredRenderer.textureDescs.empty());
    assert(authoredRenderer.samplerDescs.size() == 1);
    assert(!authoredRenderer.uploadedVertices.empty());
    for (const kera::GltfVertex& vertex : authoredRenderer.uploadedVertices)
    {
        assert(std::abs(vertex.tangent.x - 0.0f) < 0.001f);
        assert(std::abs(vertex.tangent.y - 1.0f) < 0.001f);
        assert(std::abs(vertex.tangent.z - 0.0f) < 0.001f);
        assert(std::abs(vertex.tangent.w + 1.0f) < 0.001f);
    }
    kera::destroyGltfModel(authoredRenderer, authoredModel.value());

    const std::string generatedBasePath = "generated_tangent_material";
    assert(writeTangentGltfFixture(generatedBasePath, false));

    NullRenderer generatedRenderer(true);
    kera::RendererResult<kera::GltfLoadedModel> generatedModel =
        kera::loadGltfModel(generatedRenderer, {
                                                  .path = generatedBasePath + ".gltf",
                                                  .debugName = "Generated Tangent Material",
                                                  .requireMaterialTextures = false,
                                              });
    assert(generatedModel.ok());
    assert(!generatedRenderer.uploadedVertices.empty());
    for (const kera::GltfVertex& vertex : generatedRenderer.uploadedVertices)
    {
        assert(std::abs(vertex.tangent.x - 1.0f) < 0.001f);
        assert(std::abs(vertex.tangent.y - 0.0f) < 0.001f);
        assert(std::abs(vertex.tangent.z - 0.0f) < 0.001f);
        assert(std::abs(vertex.tangent.w - 1.0f) < 0.001f);
    }
    kera::destroyGltfModel(generatedRenderer, generatedModel.value());

    return 0;
}
