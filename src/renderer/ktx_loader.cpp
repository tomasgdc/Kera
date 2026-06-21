// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/ktx_loader.h"

#include "kera/renderer/interfaces.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace kera
{
    constexpr uint8_t KtxIndetifier[12] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};
    constexpr uint32_t KtxEndianLittle = 0x04030201;
    constexpr uint32_t GlRgb10A2LikePackedFloat = 0x8C3A;
    constexpr uint32_t KtxFaceCountCube = 6;

#pragma pack(push, 1)
    struct KtxHeader
    {
        uint8_t identifier[12];
        uint32_t endianness;
        uint32_t glType;
        uint32_t glTypeSize;
        uint32_t glFormat;
        uint32_t glInternalFormat;
        uint32_t glBaseInternalFormat;
        uint32_t pixelWidth;
        uint32_t pixelHeight;
        uint32_t pixelDepth;
        uint32_t numberOfArrayElements;
        uint32_t numberOfFaces;
        uint32_t numberOfMipmapLevels;
        uint32_t bytesOfKeyValueData;
    };
#pragma pack(pop)

    struct LoadedKtxCube
    {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t mipLevels = 1;
        TextureFormat format = TextureFormat::B10G11R11Ufloat;

        std::vector<uint8_t> bytes;
        std::vector<TextureSubresourceUpload> subresources;
    };

    size_t alignTo4(size_t value)
    {
        return (value + 3u) & ~size_t(3u);
    }

    bool readWholeFile(const std::filesystem::path& path, std::vector<uint8_t>& out)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            return false;
        }

        file.seekg(0, std::ios::end);
        const auto size = file.tellg();
        file.seekg(0, std::ios::beg);

        out.resize(static_cast<size_t>(size));
        file.read(reinterpret_cast<char*>(out.data()), size);
        return file.good();
    }

    bool loadKtxCube(const std::filesystem::path& path, LoadedKtxCube& out, std::string& error)
    {
        std::vector<uint8_t> bytes;
        if (!readWholeFile(path, bytes))
        {
            error = "Failed to read KTX file: " + path.string();
            return false;
        }

        if (bytes.size() < sizeof(KtxHeader))
        {
            error = "KTX file is too small: " + path.string();
            return false;
        }

        KtxHeader header{};
        std::memcpy(&header, bytes.data(), sizeof(KtxHeader));

        if (std::memcmp(header.identifier, KtxIndetifier, sizeof(KtxIndetifier)) != 0)
        {
            error = "KTX file has invalid identifier: " + path.string();
            return false;
        }

        if (header.endianness != KtxEndianLittle)
        {
            error = "KTX file has unsupported endianness: " + path.string();
            return false;
        }

        if (header.numberOfFaces != KtxFaceCountCube)
        {
            error = "KTX file is not a cube map: " + path.string();
            return false;
        }

        if (header.pixelDepth != 0 || header.numberOfArrayElements != 0)
        {
            error = "Only non-array 2D cubemaps are supported: " + path.string();
            return false;
        }

        if (header.glInternalFormat != GlRgb10A2LikePackedFloat)
        {
            error = "KTX file has unsupported internal format: " + path.string();
            return false;
        }

        const uint32_t mipLevels = header.numberOfMipmapLevels == 0 ? 1u : header.numberOfMipmapLevels;
        size_t offset = sizeof(KtxHeader);
        offset += header.bytesOfKeyValueData;

        std::vector<TextureSubresourceUpload> subresources;
        subresources.reserve(static_cast<size_t>(mipLevels) * KtxFaceCountCube);

        for (uint32_t mip = 0; mip < mipLevels; ++mip)
        {
            if (offset + sizeof(uint32_t) > bytes.size())
            {
                error = "Unexpected end of KTX beofre mip image size: " + path.string();
                return false;
            }

            uint32_t imageSize = 0;
            std::memcpy(&imageSize, bytes.data() + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);

            const uint32_t mipWidth = std::max(1u, header.pixelWidth >> mip);
            const uint32_t mipHeight = std::max(1u, header.pixelHeight >> mip);
            const size_t faceSize = imageSize;

            for (uint32_t face = 0; face < KtxFaceCountCube; ++face)
            {
                if (offset + faceSize > bytes.size())
                {
                    error = "Unexpected end of KTX face payload: " + path.string();
                    return false;
                }

                TextureSubresourceUpload subresource{.mipLevel = mip,
                                                     .arrayLayer = face,
                                                     .width = mipWidth,
                                                     .height = mipHeight,
                                                     .offset = offset,
                                                     .size = faceSize};

                subresources.emplace_back(std::move(subresource));
                offset += faceSize;
                offset = alignTo4(offset);
            }
        }

        out.width = header.pixelWidth;
        out.height = header.pixelHeight;
        out.mipLevels = mipLevels;
        out.format = TextureFormat::B10G11R11Ufloat;
        out.bytes = std::move(bytes);
        out.subresources = std::move(subresources);
        return true;
    }

    bool loadSphericalHarmonics(const std::filesystem::path& path, IblSphericalHarmonics& out, std::string& error)
    {
        std::ifstream file(path);
        if (!file)
        {
            error = "Failed to open spherical harmonics file: " + path.string();
            return false;
        }

        std::vector<float> values;
        std::string line;

        while (std::getline(file, line))
        {
            const std::size_t commenStart = line.find("//");
            if (commenStart != std::string::npos)
            {
                line.resize(commenStart);
            }

            for (char& c : line)
            {
                if (c == '(' || c == ')' || c == ',' || c == ';')
                {
                    c = ' ';
                }
            }

            std::stringstream toeknStream(line);
            float value = 0.0f;
            while (toeknStream >> value)
            {
                values.push_back(value);
            }
        }

        if (values.size() < 27)
        {
            error = "Spherical harmonics file does not contain enough coefficients: " + path.string();
            return false;
        }

        for (uint32_t i = 0; i < 9; ++i)
        {
            out.coefficients[i][0] = values[i * 3 + 0];
            out.coefficients[i][1] = values[i * 3 + 1];
            out.coefficients[i][2] = values[i * 3 + 2];
            out.coefficients[i][3] = 0.0f;
        }

        return true;
    }

    RendererResult<TextureHandle> createAndUploadCube(IRenderer& renderer, const LoadedKtxCube& ktx,
                                                      const std::string& debugName)
    {
        TextureDesc desc{};
        desc.width = ktx.width;
        desc.height = ktx.height;
        desc.mipLevels = ktx.mipLevels;
        desc.format = ktx.format;
        desc.dimension = TextureDimension::TextureCube;
        desc.generateMipmaps = false;
        desc.sampled = true;
        desc.debugName = debugName;
        TextureHandle texture = renderer.createTexture(desc);
        if (!texture.isValid())
        {
            return RendererResult<TextureHandle>::failure("Failed to create texture for KTX cube: " + debugName);
        }

        TexturePrepareUpload upload{};
        upload.data = ktx.bytes.data();
        upload.size = ktx.bytes.size();
        upload.subresources = ktx.subresources.data();
        upload.subresourceCount = ktx.subresources.size();

        if (!renderer.uploadTextureSubresource(texture, upload))
        {
            renderer.destroyTexture(texture);
            return RendererResult<TextureHandle>::failure("Failed to upload KTX cubemap: " + debugName);
        }
        return RendererResult<TextureHandle>::success(texture);
    }

    RendererResult<IblEnvironment> loadIblEnvironment(IRenderer& renderer, const IblEnvironmentLoadDesc& desc)
    {
        LoadedKtxCube iblCube;
        LoadedKtxCube SkyboxCube;
        std::string error;

        if (!loadKtxCube(desc.iblKtxPath, iblCube, error))
        {
            return RendererResult<IblEnvironment>::failure(error);
        }

        if (!loadKtxCube(desc.skyboxKtxPath, SkyboxCube, error))
        {
            return RendererResult<IblEnvironment>::failure(error);
        }

        IblEnvironment environment{};
        environment.iblMiplevels = iblCube.mipLevels;
        environment.skyboxMiplevels = SkyboxCube.mipLevels;

        if (!loadSphericalHarmonics(desc.sphericalHarmonicsPath, environment.sphericalHarmonics, error))
        {
            return RendererResult<IblEnvironment>::failure(error);
        }

        auto iblTexture = createAndUploadCube(renderer, iblCube, desc.debugName + " IBL");
        if (!iblTexture.ok())
        {
            return RendererResult<IblEnvironment>::failure(iblTexture.errorMessage());
        }

        environment.iblTexture = iblTexture.value();

        auto skyboxTexture = createAndUploadCube(renderer, SkyboxCube, desc.debugName + " Skybox");
        if (!skyboxTexture.ok())
        {
            renderer.destroyTexture(environment.iblTexture);
            return RendererResult<IblEnvironment>::failure(skyboxTexture.errorMessage());
        }

        environment.skyboxTexture = skyboxTexture.value();

        SamplerDesc samplerDesc{};
        samplerDesc.maxLod = static_cast<float>(iblCube.mipLevels - 1);
        samplerDesc.debugName = "IBL Cubemap Sampler";

        auto sampler = renderer.createSampler(samplerDesc);
        if (!sampler.isValid())
        {
            destroyIblEnvironment(renderer, environment);
            return RendererResult<IblEnvironment>::failure("Failed to create sampler for IBL environment.");
        }

        environment.sampler = sampler;
        return RendererResult<IblEnvironment>::success(environment);
    }

    void destroyIblEnvironment(IRenderer& renderer, IblEnvironment& environment)
    {
        if (environment.sampler.isValid())
        {
            renderer.destroySampler(environment.sampler);
            environment.sampler = {};
        }

        if (environment.skyboxTexture.isValid())
        {
            renderer.destroyTexture(environment.skyboxTexture);
            environment.skyboxTexture = {};
        }

        if (environment.iblTexture.isValid())
        {
            renderer.destroyTexture(environment.iblTexture);
            environment.iblTexture = {};
        }

        environment = {};
    }
}
