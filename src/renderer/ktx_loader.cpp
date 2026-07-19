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
    constexpr uint8_t kKtxIndetifier[12] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};
    constexpr uint32_t kKtxEndianLittle = 0x04030201;
    constexpr uint32_t kGlRgb10A2LikePackedFloat = 0x8C3A;
    constexpr uint32_t kKtxFaceCountCube = 6;

#pragma pack(push, 1)
    struct KtxHeader
    {
        uint8_t identifier[12];
        uint32_t endianness;
        uint32_t gl_type;
        uint32_t gl_type_size;
        uint32_t gl_format;
        uint32_t gl_internal_format;
        uint32_t gl_base_internal_format;
        uint32_t pixel_width;
        uint32_t pixel_height;
        uint32_t pixel_depth;
        uint32_t number_of_array_elements;
        uint32_t number_of_faces;
        uint32_t number_of_mipmap_levels;
        uint32_t bytes_of_key_value_data;
    };
#pragma pack(pop)

    struct LoadedKtxCube
    {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t mip_levels = 1;
        ETextureFormat format = ETextureFormat::B10_G11_R11_UFLOAT;

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

        if (std::memcmp(header.identifier, kKtxIndetifier, sizeof(kKtxIndetifier)) != 0)
        {
            error = "KTX file has invalid identifier: " + path.string();
            return false;
        }

        if (header.endianness != kKtxEndianLittle)
        {
            error = "KTX file has unsupported endianness: " + path.string();
            return false;
        }

        if (header.number_of_faces != kKtxFaceCountCube)
        {
            error = "KTX file is not a cube map: " + path.string();
            return false;
        }

        if (header.pixel_depth != 0 || header.number_of_array_elements != 0)
        {
            error = "Only non-array 2D cubemaps are supported: " + path.string();
            return false;
        }

        if (header.gl_internal_format != kGlRgb10A2LikePackedFloat)
        {
            error = "KTX file has unsupported internal format: " + path.string();
            return false;
        }

        const uint32_t mip_levels = header.number_of_mipmap_levels == 0 ? 1u : header.number_of_mipmap_levels;
        size_t offset = sizeof(KtxHeader);
        offset += header.bytes_of_key_value_data;

        std::vector<TextureSubresourceUpload> subresources;
        subresources.reserve(static_cast<size_t>(mip_levels) * kKtxFaceCountCube);

        for (uint32_t mip = 0; mip < mip_levels; ++mip)
        {
            if (offset + sizeof(uint32_t) > bytes.size())
            {
                error = "Unexpected end of KTX beofre mip image size: " + path.string();
                return false;
            }

            uint32_t image_size = 0;
            std::memcpy(&image_size, bytes.data() + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);

            const uint32_t mip_width = std::max(1u, header.pixel_width >> mip);
            const uint32_t mip_height = std::max(1u, header.pixel_height >> mip);
            const size_t face_size = image_size;

            for (uint32_t face = 0; face < kKtxFaceCountCube; ++face)
            {
                if (offset + face_size > bytes.size())
                {
                    error = "Unexpected end of KTX face payload: " + path.string();
                    return false;
                }

                TextureSubresourceUpload subresource{.mip_level = mip,
                                                     .array_layer = face,
                                                     .width = mip_width,
                                                     .height = mip_height,
                                                     .offset = offset,
                                                     .size = face_size};

                subresources.emplace_back(std::move(subresource));
                offset += face_size;
                offset = alignTo4(offset);
            }
        }

        out.width = header.pixel_width;
        out.height = header.pixel_height;
        out.mip_levels = mip_levels;
        out.format = ETextureFormat::B10_G11_R11_UFLOAT;
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
            const std::size_t commen_start = line.find("//");
            if (commen_start != std::string::npos)
            {
                line.resize(commen_start);
            }

            for (char& c : line)
            {
                if (c == '(' || c == ')' || c == ',' || c == ';')
                {
                    c = ' ';
                }
            }

            std::stringstream toekn_stream(line);
            float value = 0.0f;
            while (toekn_stream >> value)
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
                                                      const std::string& debug_name)
    {
        TextureDesc desc{};
        desc.width = ktx.width;
        desc.height = ktx.height;
        desc.mip_levels = ktx.mip_levels;
        desc.format = ktx.format;
        desc.dimension = ETextureDimension::TEXTURE_CUBE;
        desc.generate_mipmaps = false;
        desc.sampled = true;
        desc.debug_name = debug_name;
        TextureHandle texture = renderer.createTexture(desc);
        if (!texture.isValid())
        {
            return RendererResult<TextureHandle>::failure("Failed to create texture for KTX cube: " + debug_name);
        }

        TexturePrepareUpload upload{};
        upload.data = ktx.bytes.data();
        upload.size = ktx.bytes.size();
        upload.subresources = ktx.subresources.data();
        upload.subresource_count = ktx.subresources.size();

        if (!renderer.uploadTextureSubresource(texture, upload))
        {
            renderer.destroyTexture(texture);
            return RendererResult<TextureHandle>::failure("Failed to upload KTX cubemap: " + debug_name);
        }
        return RendererResult<TextureHandle>::success(texture);
    }

    RendererResult<IblEnvironment> loadIblEnvironment(IRenderer& renderer, const IblEnvironmentLoadDesc& desc)
    {
        LoadedKtxCube ibl_cube;
        LoadedKtxCube skybox_cube;
        std::string error;

        if (!loadKtxCube(desc.ibl_ktx_path, ibl_cube, error))
        {
            return RendererResult<IblEnvironment>::failure(error);
        }

        if (!loadKtxCube(desc.skybox_ktx_path, skybox_cube, error))
        {
            return RendererResult<IblEnvironment>::failure(error);
        }

        IblEnvironment environment{};
        environment.ibl_miplevels = ibl_cube.mip_levels;
        environment.skybox_miplevels = skybox_cube.mip_levels;

        if (!loadSphericalHarmonics(desc.spherical_harmonics_path, environment.spherical_harmonics, error))
        {
            return RendererResult<IblEnvironment>::failure(error);
        }

        auto ibl_texture = createAndUploadCube(renderer, ibl_cube, desc.debug_name + " IBL");
        if (!ibl_texture.ok())
        {
            return RendererResult<IblEnvironment>::failure(ibl_texture.errorMessage());
        }

        environment.ibl_texture = ibl_texture.value();

        auto skybox_texture = createAndUploadCube(renderer, skybox_cube, desc.debug_name + " Skybox");
        if (!skybox_texture.ok())
        {
            renderer.destroyTexture(environment.ibl_texture);
            return RendererResult<IblEnvironment>::failure(skybox_texture.errorMessage());
        }

        environment.skybox_texture = skybox_texture.value();

        SamplerDesc sampler_desc{};
        sampler_desc.max_lod = static_cast<float>(ibl_cube.mip_levels - 1);
        sampler_desc.debug_name = "IBL Cubemap Sampler";

        auto sampler = renderer.createSampler(sampler_desc);
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

        if (environment.skybox_texture.isValid())
        {
            renderer.destroyTexture(environment.skybox_texture);
            environment.skybox_texture = {};
        }

        if (environment.ibl_texture.isValid())
        {
            renderer.destroyTexture(environment.ibl_texture);
            environment.ibl_texture = {};
        }

        environment = {};
    }
}
