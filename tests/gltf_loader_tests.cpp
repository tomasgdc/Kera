// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/gltf_loader.h"
#include "kera/renderer/interfaces.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
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
        explicit NullRenderer(bool create_resources = false) : m_create_resources(create_resources) {}

        std::vector<kera::BufferDesc> buffer_descs;
        std::vector<kera::TextureDesc> texture_descs;
        std::vector<kera::SamplerDesc> sampler_descs;
        std::vector<kera::GltfVertex> uploaded_vertices;
        int32_t upload_batch_begin_count = 0;
        int32_t upload_batch_end_count = 0;
        int32_t upload_batch_cancel_count = 0;
        int32_t destroyed_buffer_count = 0;
        int32_t destroyed_texture_count = 0;
        int32_t destroyed_sampler_count = 0;
        bool upload_batch_begin_succeeds = true;
        bool upload_batch_end_succeeds = true;
        bool texture_uploads_suceed = true;

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
            if (!m_create_resources)
            {
                return {};
            }
            buffer_descs.push_back(desc);
            return {m_next_buffer++, 1};
        }
        bool destroyBuffer(kera::BufferHandle) override
        {
            ++destroyed_buffer_count;
            return m_create_resources;
        }
        bool mapBuffer(kera::BufferHandle, void**) override
        {
            return false;
        }
        void unmapBuffer(kera::BufferHandle) override {}
        bool uploadBuffer(kera::BufferHandle, const void* data, std::size_t size, std::size_t) override
        {
            if (!m_create_resources || !data)
            {
                return false;
            }
            if (size >= sizeof(kera::GltfVertex) && size % sizeof(kera::GltfVertex) == 0 && uploaded_vertices.empty())
            {
                uploaded_vertices.resize(size / sizeof(kera::GltfVertex));
                std::memcpy(uploaded_vertices.data(), data, size);
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
        kera::TextureHandle createTexture(const kera::TextureDesc& desc) override
        {
            if (!m_create_resources)
            {
                return {};
            }
            texture_descs.push_back(desc);
            return {m_next_texture++, 1};
        }
        bool beginUploadBatch() override
        {
            ++upload_batch_begin_count;
            return upload_batch_begin_succeeds;
        }
        bool endUploadBatch() override
        {
            ++upload_batch_end_count;
            return upload_batch_end_succeeds;
        }
        void cancelUploadBatch() override
        {
            ++upload_batch_cancel_count;
        }
        bool uploadTexture(kera::TextureHandle, const void* data, std::size_t size) override
        {
            return m_create_resources && texture_uploads_suceed && data && size > 0;
        }
        bool uploadTextureSubresource(kera::TextureHandle texture, const kera::TexturePrepareUpload& upload)
        {
            return m_create_resources && upload.data && upload.size > 0 && upload.subresources &&
                   upload.subresource_count > 0;
        }
        bool destroyTexture(kera::TextureHandle) override
        {
            ++destroyed_texture_count;
            return m_create_resources;
        }
        kera::SamplerHandle createSampler(const kera::SamplerDesc& desc) override
        {
            if (!m_create_resources)
            {
                return {};
            }
            sampler_descs.push_back(desc);
            return {m_next_sampler++, 1};
        }
        bool destroySampler(kera::SamplerHandle) override
        {
            ++destroyed_sampler_count;
            return m_create_resources;
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

    private:
        bool m_create_resources = false;
        int32_t m_next_buffer = 0;
        int32_t m_next_texture = 0;
        int32_t m_next_sampler = 0;
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

    bool writeTangentGltfFixture(const std::string& base_path, bool authored_tangents)
    {
        std::vector<uint8_t> bytes;

        const float positions[] = {
            0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        };
        const float normals[] = {
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        };
        const float uvs[] = {
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        };
        const float tangents[] = {
            0.0f, 2.0f, 0.0f, -1.0f, 0.0f, 2.0f, 0.0f, -1.0f, 0.0f, 2.0f, 0.0f, -1.0f,
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
        if (authored_tangents)
        {
            for (float value : tangents)
            {
                appendFloat(bytes, value);
            }
        }
        const size_t index_byte_offset = bytes.size();
        for (uint16_t value : indices)
        {
            appendUInt16(bytes, value);
        }

        const std::string bin_path = base_path + ".bin";
        {
            std::ofstream bin(bin_path, std::ios::binary);
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

        const std::string gltf_path = base_path + ".gltf";
        std::ofstream gltf(gltf_path);
        if (!gltf)
        {
            return false;
        }

        gltf << "{\n"
             << "  \"asset\": {\"version\": \"2.0\"},\n"
             << "  \"buffers\": [{\"uri\": \"" << base_path << ".bin\", \"byteLength\": " << bytes.size() << "}],\n"
             << "  \"bufferViews\": [\n"
             << "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36, \"target\": 34962},\n"
             << "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 36, \"target\": 34962},\n"
             << "    {\"buffer\": 0, \"byteOffset\": 72, \"byteLength\": 24, \"target\": 34962}";
        if (authored_tangents)
        {
            gltf << ",\n"
                 << "    {\"buffer\": 0, \"byteOffset\": 96, \"byteLength\": 48, \"target\": 34962}";
        }
        gltf << ",\n"
             << "    {\"buffer\": 0, \"byteOffset\": " << index_byte_offset
             << ", \"byteLength\": 6, \"target\": 34963}\n"
             << "  ],\n"
             << "  \"accessors\": [\n"
             << "    {\"bufferView\": 0, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\"},\n"
             << "    {\"bufferView\": 1, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\"},\n"
             << "    {\"bufferView\": 2, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC2\"}";
        if (authored_tangents)
        {
            gltf << ",\n"
                 << "    {\"bufferView\": 3, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC4\"}";
        }
        const int index_accessor = authored_tangents ? 4 : 3;
        const int index_buffer_view = authored_tangents ? 4 : 3;
        gltf << ",\n"
             << "    {\"bufferView\": " << index_buffer_view
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
        if (authored_tangents)
        {
            gltf << ", \"TANGENT\": 3";
        }
        gltf << "}, \"indices\": " << index_accessor << ", \"material\": 0}]}],\n"
             << "  \"nodes\": [{\"mesh\": 0}],\n"
             << "  \"scenes\": [{\"nodes\": [0]}],\n"
             << "  \"scene\": 0\n"
             << "}\n";

        return static_cast<bool>(gltf);
    }

    bool writeMultiPrimitiveSceneFixture(const std::string& base_path)
    {
        std::vector<uint8_t> bytes;
        const float positions[] = {
            0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        };
        const float normals[] = {
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        };
        const float uvs[] = {
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
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
        for (uint16_t value : indices)
        {
            appendUInt16(bytes, value);
        }

        const std::string bin_path = base_path + ".bin";
        std::ofstream bin(bin_path, std::ios::binary);
        if (!bin)
        {
            return false;
        }
        bin.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!bin)
        {
            return false;
        }

        std::ofstream gltf(base_path + ".gltf");
        if (!gltf)
        {
            return false;
        }
        gltf << "{\n"
             << "  \"asset\": {\"version\": \"2.0\"},\n"
             << "  \"buffers\": [{\"uri\": \"" << base_path << ".bin\", \"byteLength\": " << bytes.size() << "}],\n"
             << "  \"bufferViews\": [\n"
             << "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36, \"target\": 34962},\n"
             << "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 36, \"target\": 34962},\n"
             << "    {\"buffer\": 0, \"byteOffset\": 72, \"byteLength\": 24, \"target\": 34962},\n"
             << "    {\"buffer\": 0, \"byteOffset\": 96, \"byteLength\": 6, \"target\": 34963}\n"
             << "  ],\n"
             << "  \"accessors\": [\n"
             << "    {\"bufferView\": 0, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\"},\n"
             << "    {\"bufferView\": 1, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\"},\n"
             << "    {\"bufferView\": 2, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC2\"},\n"
             << "    {\"bufferView\": 3, \"componentType\": 5123, \"count\": 3, \"type\": \"SCALAR\"}\n"
             << "  ],\n"
             << "  \"materials\": [{\"alphaMode\": \"MASK\", \"alphaCutoff\": 0.42, \"doubleSided\": true,\n"
             << "    \"pbrMetallicRoughness\": {\"baseColorFactor\": [0.2, 0.3, 0.4, 0.6],\n"
             << "      \"metallicFactor\": 0.25, \"roughnessFactor\": 0.75}}],\n"
             << "  \"meshes\": [{\"primitives\": [\n"
             << "    {\"attributes\": {\"POSITION\": 0, \"NORMAL\": 1, \"TEXCOORD_0\": 2}, \"indices\": 3, "
                "\"material\": 0},\n"
             << "    {\"attributes\": {\"POSITION\": 0, \"NORMAL\": 1, \"TEXCOORD_0\": 2}, \"indices\": 3, "
                "\"material\": 0}\n"
             << "  ]}],\n"
             << "  \"nodes\": [{\"translation\": [1, 0, 0], \"children\": [1]}, {\"translation\": [0, 2, 0], \"mesh\": "
                "0}],\n"
             << "  \"scenes\": [{\"nodes\": [0]}],\n"
             << "  \"scene\": 0\n"
             << "}\n";
        return static_cast<bool>(gltf);
    }

    enum class EStaticMaterialContractFixture
    {
        TEXCOORD_ONE,
        INCOMPATIBLE_SAMPLERS,
        NORMAL_ONLY,
    };

    bool writeStaticMaterialContractFixture(const std::string& base_path, EStaticMaterialContractFixture fixture)
    {
        std::vector<uint8_t> bytes;
        const float positions[] = {
            0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        };
        const float normals[] = {
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        };
        const float uvs[] = {
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
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
        for (uint16_t value : indices)
        {
            appendUInt16(bytes, value);
        }

        std::ofstream bin(base_path + ".bin", std::ios::binary);
        if (!bin)
        {
            return false;
        }
        bin.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!bin)
        {
            return false;
        }

        const bool normal_only = fixture == EStaticMaterialContractFixture::NORMAL_ONLY;
        const bool incompatible_samplers = fixture == EStaticMaterialContractFixture::INCOMPATIBLE_SAMPLERS;
        const int normal_texture_index = normal_only ? 0 : 1;
        const int normal_tex_coord = fixture == EStaticMaterialContractFixture::TEXCOORD_ONE ? 1 : 0;

        std::ofstream gltf(base_path + ".gltf");
        if (!gltf)
        {
            return false;
        }
        gltf
            << "{\n"
            << "  \"asset\": {\"version\": \"2.0\"},\n"
            << "  \"buffers\": [{\"uri\": \"" << base_path << ".bin\", \"byteLength\": " << bytes.size() << "}],\n"
            << "  \"bufferViews\": [\n"
            << "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36, \"target\": 34962},\n"
            << "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 36, \"target\": 34962},\n"
            << "    {\"buffer\": 0, \"byteOffset\": 72, \"byteLength\": 24, \"target\": 34962},\n"
            << "    {\"buffer\": 0, \"byteOffset\": 96, \"byteLength\": 6, \"target\": 34963}\n"
            << "  ],\n"
            << "  \"accessors\": [\n"
            << "    {\"bufferView\": 0, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\"},\n"
            << "    {\"bufferView\": 1, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\"},\n"
            << "    {\"bufferView\": 2, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC2\"},\n"
            << "    {\"bufferView\": 3, \"componentType\": 5123, \"count\": 3, \"type\": \"SCALAR\"}\n"
            << "  ],\n"
            << "  \"images\": [{\"uri\": \"data:image/png;base64,"
               "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAFgAI/ScL/6QAAAABJRU5ErkJggg==\"}],\n"
            << "  \"samplers\": [\n";
        if (incompatible_samplers)
        {
            gltf << "    {\"magFilter\": 9729, \"minFilter\": 9729, \"wrapS\": 10497, \"wrapT\": 10497},\n";
        }
        gltf << "    {\"magFilter\": 9728, \"minFilter\": 9728, \"wrapS\": 33071, \"wrapT\": 33648}\n"
             << "  ],\n"
             << "  \"textures\": [\n";
        if (!normal_only)
        {
            gltf << "    {\"sampler\": 0, \"source\": 0},\n";
        }
        gltf << "    {\"sampler\": " << (incompatible_samplers ? 1 : 0) << ", \"source\": 0}\n"
             << "  ],\n"
             << "  \"materials\": [{\n"
             << "    \"normalTexture\": {\"index\": " << normal_texture_index;
        if (normal_tex_coord != 0)
        {
            gltf << ", \"texCoord\": " << normal_tex_coord;
        }
        gltf << "},\n"
             << "    \"pbrMetallicRoughness\": {";
        if (!normal_only)
        {
            gltf << "\"baseColorTexture\": {\"index\": 0}";
        }
        gltf << "}\n"
             << "  }],\n"
             << "  \"meshes\": [{\"primitives\": [{\"attributes\": {\"POSITION\": 0, \"NORMAL\": 1, \"TEXCOORD_0\": "
                "2}, \"indices\": 3, \"material\": 0}]}],\n"
             << "  \"nodes\": [{\"mesh\": 0}],\n"
             << "  \"scenes\": [{\"nodes\": [0]}],\n"
             << "  \"scene\": 0\n"
             << "}\n";
        return static_cast<bool>(gltf);
    }
}  // namespace

TEST(KeraGltfLoader, ReportsValidationFailuresForMissingPaths)
{
    NullRenderer renderer;

    const kera::RendererResult<kera::GltfLoadedModel> empty_path =
        kera::loadGltfModel(renderer, {.path = "", .debug_name = "Empty"});
    EXPECT_FALSE(empty_path.ok());
    EXPECT_EQ(empty_path.errorCode(), kera::ERendererErrorCode::VALIDATION_FAILED);
    EXPECT_FALSE(empty_path.errorMessage().empty());

    const kera::RendererResult<kera::GltfLoadedModel> missing_file =
        kera::loadGltfModel(renderer, {.path = "missing_file_that_should_not_exist.gltf", .debug_name = "Missing"});
    EXPECT_FALSE(missing_file.ok());
    EXPECT_EQ(missing_file.errorCode(), kera::ERendererErrorCode::VALIDATION_FAILED);
    EXPECT_FALSE(missing_file.errorMessage().empty());
}

TEST(KeraGltfLoader, LoadsDamagedHelmetResourcesAndGeneratedTangents)
{
    NullRenderer resource_renderer(true);
    const std::string damaged_helmet_path =
        std::string(KERA_SOURCE_DIR) + "/samples/assets/gltf/DamagedHelmet/DamagedHelmet.gltf";

    kera::RendererResult<kera::GltfLoadedModel> damaged_helmet =
        kera::loadGltfModel(resource_renderer, {.path = damaged_helmet_path, .debug_name = "DamagedHelmet"});

    ASSERT_TRUE(damaged_helmet.ok());
    EXPECT_EQ(resource_renderer.upload_batch_begin_count, 1);
    EXPECT_EQ(resource_renderer.upload_batch_end_count, 1);
    EXPECT_EQ(resource_renderer.upload_batch_cancel_count, 0);
    EXPECT_EQ(damaged_helmet.value().index_count, 46356u);
    EXPECT_EQ(damaged_helmet.value().material_factors.base_color, glm::vec4(1.0f));
    EXPECT_EQ(damaged_helmet.value().material_factors.emissive, glm::vec3(1.0f));
    EXPECT_EQ(damaged_helmet.value().material_factors.metallic, 1.0f);
    EXPECT_EQ(damaged_helmet.value().material_factors.roughness, 1.0f);
    EXPECT_EQ(damaged_helmet.value().material_factors.normal_scale, 1.0f);
    EXPECT_EQ(damaged_helmet.value().material_factors.occlusion_strength, 1.0f);
    EXPECT_EQ(damaged_helmet.value().material_factors.alpha_cutoff, 0.5f);
    EXPECT_EQ(damaged_helmet.value().material_factors.alpha_mode, kera::EGltfAlphaMode::ALPHA_OPAQUE);
    EXPECT_FALSE(damaged_helmet.value().material_factors.double_sided);

    ASSERT_EQ(resource_renderer.texture_descs.size(), 5u);
    EXPECT_EQ(resource_renderer.texture_descs[0].format, kera::ETextureFormat::RGB_A8_SRGB);
    EXPECT_EQ(resource_renderer.texture_descs[1].format, kera::ETextureFormat::RGBA8);
    EXPECT_EQ(resource_renderer.texture_descs[2].format, kera::ETextureFormat::RGB_A8_SRGB);
    EXPECT_EQ(resource_renderer.texture_descs[3].format, kera::ETextureFormat::RGBA8);
    EXPECT_EQ(resource_renderer.texture_descs[4].format, kera::ETextureFormat::RGBA8);
    for (const kera::TextureDesc& texture_desc : resource_renderer.texture_descs)
    {
        EXPECT_TRUE(texture_desc.generate_mipmaps);
        EXPECT_EQ(texture_desc.mip_levels, kera::textureFullMipLevelCount(texture_desc.width, texture_desc.height));
        EXPECT_GT(texture_desc.mip_levels, 1u);
    }
    ASSERT_EQ(resource_renderer.sampler_descs.size(), 1u);
    EXPECT_EQ(resource_renderer.sampler_descs[0].min_filter, kera::ESamplerFilter::LINEAR);
    EXPECT_EQ(resource_renderer.sampler_descs[0].mag_filter, kera::ESamplerFilter::LINEAR);
    EXPECT_EQ(resource_renderer.sampler_descs[0].mip_filter, kera::ESamplerMipFilter::LINEAR);
    EXPECT_EQ(resource_renderer.sampler_descs[0].address_mode_u, kera::ESamplerAddressMode::REPEAT);
    EXPECT_EQ(resource_renderer.sampler_descs[0].address_mode_v, kera::ESamplerAddressMode::REPEAT);
    EXPECT_GT(resource_renderer.sampler_descs[0].max_lod, 1.0f);
    EXPECT_EQ(resource_renderer.sampler_descs[0].max_anisotropy, 1.0f);
    ASSERT_FALSE(resource_renderer.uploaded_vertices.empty());

    bool found_generated_tangent = false;
    for (const kera::GltfVertex& vertex : resource_renderer.uploaded_vertices)
    {
        const float tangent_length_squared = glm::dot(glm::vec3(vertex.tangent), glm::vec3(vertex.tangent));
        if (tangent_length_squared > 0.5f && std::abs(std::abs(vertex.tangent.w) - 1.0f) < 0.001f)
        {
            found_generated_tangent = true;
            break;
        }
    }
    EXPECT_TRUE(found_generated_tangent);
    kera::destroyGltfModel(resource_renderer, damaged_helmet.value());
}

TEST(KeraGltfLoader, LoadsAllDrawItemsFromDefaultScene)
{
    NullRenderer resource_renderer(true);
    const std::string damaged_helmet_path =
        std::string(KERA_SOURCE_DIR) + "/samples/assets/gltf/DamagedHelmet/DamagedHelmet.gltf";

    kera::RendererResult<kera::GltfLoadedScene> scene =
        kera::loadGltfScene(resource_renderer, {.path = damaged_helmet_path, .debug_name = "DamagedHelmet Scene"});

    ASSERT_TRUE(scene.ok());
    ASSERT_EQ(scene.value().draw_items.size(), 1u);
    EXPECT_EQ(scene.value().draw_items[0].index_count, 46356u);
    EXPECT_EQ(scene.value().draw_items[0].material_factors.alpha_mode, kera::EGltfAlphaMode::ALPHA_OPAQUE);
    kera::destroyGltfScene(resource_renderer, scene.value());
    EXPECT_TRUE(scene.value().draw_items.empty());
}

TEST(KeraGltfLoader, CachesSceneMaterialsUsesFallbackMapsAndDestroysSharedResourcesOnce)
{
    const std::string fixture_path = "multi_primitive_scene";
    ASSERT_TRUE(writeMultiPrimitiveSceneFixture(fixture_path));

    NullRenderer renderer(true);
    kera::RendererResult<kera::GltfLoadedScene> scene =
        kera::loadGltfScene(renderer, {.path = fixture_path + ".gltf", .debug_name = "Multi Primitive Scene"});

    ASSERT_TRUE(scene.ok());
    ASSERT_EQ(scene.value().draw_items.size(), 2u);
    EXPECT_EQ(renderer.upload_batch_begin_count, 1);
    EXPECT_EQ(renderer.upload_batch_end_count, 1);
    EXPECT_EQ(renderer.texture_descs.size(), 5u);
    EXPECT_EQ(renderer.sampler_descs.size(), 1u);

    const kera::GltfLoadedModel& first_draw = scene.value().draw_items[0];
    const kera::GltfLoadedModel& second_draw = scene.value().draw_items[1];
    EXPECT_EQ(first_draw.index_count, 3u);
    EXPECT_EQ(second_draw.index_count, 3u);
    EXPECT_EQ(first_draw.transform[3], glm::vec4(1.0f, 2.0f, 0.0f, 1.0f));
    EXPECT_EQ(second_draw.transform[3], glm::vec4(1.0f, 2.0f, 0.0f, 1.0f));
    EXPECT_EQ(first_draw.material_factors.alpha_mode, kera::EGltfAlphaMode::ALPHA_MASK);
    EXPECT_TRUE(first_draw.material_factors.double_sided);
    EXPECT_EQ(first_draw.texture_names.base_color, "fallback-base-color");
    EXPECT_EQ(first_draw.texture_names.metal_roughness, "fallback-metal-roughness");
    EXPECT_EQ(first_draw.texture_names.emissive, "fallback-emissive");
    EXPECT_EQ(first_draw.texture_names.occlusion, "fallback-occlusion");
    EXPECT_EQ(first_draw.texture_names.normal, "fallback-normal");
    EXPECT_EQ(first_draw.material_textures.base_color, second_draw.material_textures.base_color);
    EXPECT_EQ(first_draw.material_textures.metal_roughness, second_draw.material_textures.metal_roughness);
    EXPECT_EQ(first_draw.material_textures.emissive, second_draw.material_textures.emissive);
    EXPECT_EQ(first_draw.material_textures.occlusion, second_draw.material_textures.occlusion);
    EXPECT_EQ(first_draw.material_textures.normal, second_draw.material_textures.normal);
    EXPECT_EQ(first_draw.material_sampler, second_draw.material_sampler);

    kera::destroyGltfScene(renderer, scene.value());
    EXPECT_TRUE(scene.value().draw_items.empty());
    EXPECT_EQ(renderer.destroyed_buffer_count, 4);
    EXPECT_EQ(renderer.destroyed_texture_count, 5);
    EXPECT_EQ(renderer.destroyed_sampler_count, 1);
}

TEST(KeraGltfLoader, RejectsStaticMaterialUsingTexCoordOne)
{
    const std::string fixture_path = "static_material_texcoord_one";
    ASSERT_TRUE(writeStaticMaterialContractFixture(fixture_path, EStaticMaterialContractFixture::TEXCOORD_ONE));

    NullRenderer renderer(true);
    const kera::RendererResult<kera::GltfLoadedScene> scene =
        kera::loadGltfScene(renderer, {.path = fixture_path + ".gltf", .debug_name = "TexCoord One"});

    EXPECT_FALSE(scene.ok());
    EXPECT_EQ(scene.errorCode(), kera::ERendererErrorCode::UNSUPPORTED);
    EXPECT_NE(scene.errorMessage().find("TEXCOORD_1"), std::string::npos);
    EXPECT_TRUE(renderer.sampler_descs.empty());
    EXPECT_EQ(renderer.upload_batch_begin_count, 0);
}

TEST(KeraGltfLoader, RejectsStaticMaterialUsingIncompatibleSamplers)
{
    const std::string fixture_path = "static_material_incompatible_samplers";
    ASSERT_TRUE(
        writeStaticMaterialContractFixture(fixture_path, EStaticMaterialContractFixture::INCOMPATIBLE_SAMPLERS));

    NullRenderer renderer(true);
    const kera::RendererResult<kera::GltfLoadedScene> scene =
        kera::loadGltfScene(renderer, {.path = fixture_path + ".gltf", .debug_name = "Incompatible Samplers"});

    EXPECT_FALSE(scene.ok());
    EXPECT_EQ(scene.errorCode(), kera::ERendererErrorCode::UNSUPPORTED);
    EXPECT_NE(scene.errorMessage().find("incompatible samplers"), std::string::npos);
    EXPECT_TRUE(renderer.sampler_descs.empty());
    EXPECT_EQ(renderer.upload_batch_begin_count, 0);
}

TEST(KeraGltfLoader, UsesActiveTextureSamplerWhenStaticMaterialHasNoBaseColorTexture)
{
    const std::string fixture_path = "static_material_normal_only";
    ASSERT_TRUE(writeStaticMaterialContractFixture(fixture_path, EStaticMaterialContractFixture::NORMAL_ONLY));

    NullRenderer renderer(true);
    kera::RendererResult<kera::GltfLoadedScene> scene =
        kera::loadGltfScene(renderer, {.path = fixture_path + ".gltf", .debug_name = "Normal Only"});

    ASSERT_TRUE(scene.ok()) << scene.errorMessage();
    ASSERT_EQ(scene.value().draw_items.size(), 1u);
    EXPECT_TRUE(scene.value().draw_items[0].material_textures.normal.isValid());
    ASSERT_EQ(renderer.sampler_descs.size(), 1u);
    EXPECT_EQ(renderer.sampler_descs[0].min_filter, kera::ESamplerFilter::NEAREST);
    EXPECT_EQ(renderer.sampler_descs[0].mag_filter, kera::ESamplerFilter::NEAREST);
    EXPECT_EQ(renderer.sampler_descs[0].address_mode_u, kera::ESamplerAddressMode::CLAMP_TO_EDGE);
    EXPECT_EQ(renderer.sampler_descs[0].address_mode_v, kera::ESamplerAddressMode::MIRRORED_REPEAT);
    EXPECT_EQ(renderer.sampler_descs[0].max_lod, 0.0f);

    kera::destroyGltfScene(renderer, scene.value());
}

TEST(KeraGltfLoader, CancelsFailedMaterialTextureUploadBatches)
{
    const std::string damaged_helmet_path =
        std::string(KERA_SOURCE_DIR) + "/samples/assets/gltf/DamagedHelmet/DamagedHelmet.gltf";

    NullRenderer upload_failure_renderer(true);
    upload_failure_renderer.texture_uploads_suceed = false;
    const kera::RendererResult<kera::GltfLoadedModel> upload_failure =
        kera::loadGltfModel(upload_failure_renderer, {.path = damaged_helmet_path, .debug_name = "UploadFailure"});

    EXPECT_FALSE(upload_failure.ok());
    EXPECT_EQ(upload_failure_renderer.upload_batch_begin_count, 1);
    EXPECT_EQ(upload_failure_renderer.upload_batch_end_count, 0);
    EXPECT_EQ(upload_failure_renderer.upload_batch_cancel_count, 1);

    NullRenderer submit_failure_renderer(true);
    submit_failure_renderer.upload_batch_end_succeeds = false;
    const kera::RendererResult<kera::GltfLoadedModel> submit_failure =
        kera::loadGltfModel(submit_failure_renderer, {.path = damaged_helmet_path, .debug_name = "SubmitFailure"});

    EXPECT_FALSE(submit_failure.ok());
    EXPECT_EQ(submit_failure.errorCode(), kera::ERendererErrorCode::BACKEND_FAILURE);
    EXPECT_EQ(submit_failure_renderer.upload_batch_begin_count, 1);
    EXPECT_EQ(submit_failure_renderer.upload_batch_end_count, 1);
    EXPECT_EQ(submit_failure_renderer.upload_batch_cancel_count, 1);
}

TEST(KeraGltfLoader, PreservesAuthoredTangentsAndGeneratesFallbackTangents)
{
    const std::string authored_base_path = "authored_tangent_material";
    ASSERT_TRUE(writeTangentGltfFixture(authored_base_path, true));

    NullRenderer authored_renderer(true);
    kera::RendererResult<kera::GltfLoadedModel> authored_model =
        kera::loadGltfModel(authored_renderer, {
                                                   .path = authored_base_path + ".gltf",
                                                   .debug_name = "Authored Tangent Material",
                                                   .require_material_textures = false,
                                               });
    ASSERT_TRUE(authored_model.ok());
    EXPECT_EQ(authored_model.value().index_count, 3u);
    EXPECT_EQ(authored_model.value().material_factors.base_color, glm::vec4(0.2f, 0.3f, 0.4f, 0.6f));
    EXPECT_EQ(authored_model.value().material_factors.emissive, glm::vec3(0.0f));
    EXPECT_EQ(authored_model.value().material_factors.metallic, 0.25f);
    EXPECT_EQ(authored_model.value().material_factors.roughness, 0.75f);
    EXPECT_EQ(authored_model.value().material_factors.alpha_cutoff, 0.42f);
    EXPECT_EQ(authored_model.value().material_factors.alpha_mode, kera::EGltfAlphaMode::ALPHA_MASK);
    EXPECT_TRUE(authored_model.value().material_factors.double_sided);
    EXPECT_TRUE(authored_renderer.texture_descs.empty());
    ASSERT_EQ(authored_renderer.sampler_descs.size(), 1u);
    ASSERT_FALSE(authored_renderer.uploaded_vertices.empty());
    for (const kera::GltfVertex& vertex : authored_renderer.uploaded_vertices)
    {
        EXPECT_NEAR(vertex.tangent.x, 0.0f, 0.001f);
        EXPECT_NEAR(vertex.tangent.y, 1.0f, 0.001f);
        EXPECT_NEAR(vertex.tangent.z, 0.0f, 0.001f);
        EXPECT_NEAR(vertex.tangent.w, -1.0f, 0.001f);
    }
    kera::destroyGltfModel(authored_renderer, authored_model.value());

    const std::string generated_base_path = "generated_tangent_material";
    ASSERT_TRUE(writeTangentGltfFixture(generated_base_path, false));

    NullRenderer generated_renderer(true);
    kera::RendererResult<kera::GltfLoadedModel> generated_model =
        kera::loadGltfModel(generated_renderer, {
                                                    .path = generated_base_path + ".gltf",
                                                    .debug_name = "Generated Tangent Material",
                                                    .require_material_textures = false,
                                                });
    ASSERT_TRUE(generated_model.ok());
    ASSERT_FALSE(generated_renderer.uploaded_vertices.empty());
    for (const kera::GltfVertex& vertex : generated_renderer.uploaded_vertices)
    {
        EXPECT_NEAR(vertex.tangent.x, 1.0f, 0.001f);
        EXPECT_NEAR(vertex.tangent.y, 0.0f, 0.001f);
        EXPECT_NEAR(vertex.tangent.z, 0.0f, 0.001f);
        EXPECT_NEAR(vertex.tangent.w, 1.0f, 0.001f);
    }
    kera::destroyGltfModel(generated_renderer, generated_model.value());
}
