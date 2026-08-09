// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/gltf_loader.h"

#include "kera/renderer/interfaces.h"
#include "kera/utilities/logger.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#elif defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#ifndef TINYGLTF_NOEXCEPTION
#define TINYGLTF_NOEXCEPTION
#endif
#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include "tiny_gltf.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace kera
{
    namespace
    {
        RendererResult<GltfLoadedModel> failLoad(IRenderer& renderer, GltfLoadedModel& model, ERendererErrorCode code,
                                                 std::string message)
        {
            destroyGltfModel(renderer, model);
            return RendererResult<GltfLoadedModel>::failure(code, std::move(message));
        }

        class UploadBatchGuard
        {
        public:
            explicit UploadBatchGuard(IRenderer& renderer) : m_renderer(renderer), m_active(renderer.beginUploadBatch())
            {
            }

            ~UploadBatchGuard()
            {
                if (m_active)
                {
                    m_renderer.cancelUploadBatch();
                }
            }

            bool started() const
            {
                return m_active;
            }

            bool finish()
            {
                if (!m_active)
                {
                    return false;
                }

                const bool finished = m_renderer.endUploadBatch();
                if (!finished)
                {
                    m_renderer.cancelUploadBatch();
                }
                m_active = false;
                return finished;
            }

            void cancel()
            {
                if (m_active)
                {
                    m_renderer.cancelUploadBatch();
                    m_active = false;
                }
            }

        private:
            IRenderer& m_renderer;
            bool m_active = false;
        };

        std::string makeDebugName(const GltfLoadDesc& desc, const char* suffix)
        {
            const std::string prefix = desc.debug_name.empty() ? "glTF" : desc.debug_name;
            return prefix + " " + suffix;
        }

        glm::vec3 normalizeOr(const glm::vec3& value, const glm::vec3& fallback)
        {
            const float length_squared = glm::dot(value, value);
            if (length_squared <= 0.000001f)
            {
                return fallback;
            }
            return value * (1.0f / std::sqrt(length_squared));
        }

        glm::vec3 fallbackTangentForNormal(const glm::vec3& normal)
        {
            const glm::vec3 axis =
                std::abs(normal.y) < 0.95f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
            return normalizeOr(glm::cross(axis, normal), glm::vec3(1.0f, 0.0f, 0.0f));
        }

        const tinygltf::Accessor* findAccessor(const tinygltf::Model& model, const tinygltf::Primitive& primitive,
                                               const char* semantic)
        {
            const auto attribute = primitive.attributes.find(semantic);
            if (attribute == primitive.attributes.end() || attribute->second < 0 ||
                attribute->second >= static_cast<int>(model.accessors.size()))
            {
                return nullptr;
            }

            return &model.accessors[static_cast<size_t>(attribute->second)];
        }

        const uint8_t* accessorData(const tinygltf::Model& model, const tinygltf::Accessor& accessor,
                                    const tinygltf::BufferView*& buffer_view, size_t& stride)
        {
            if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(model.bufferViews.size()))
            {
                return nullptr;
            }

            buffer_view = &model.bufferViews[static_cast<size_t>(accessor.bufferView)];
            if (buffer_view->buffer < 0 || buffer_view->buffer >= static_cast<int>(model.buffers.size()))
            {
                return nullptr;
            }

            const tinygltf::Buffer& buffer = model.buffers[static_cast<size_t>(buffer_view->buffer)];
            const size_t byte_offset = static_cast<size_t>(buffer_view->byteOffset + accessor.byteOffset);
            if (byte_offset >= buffer.data.size())
            {
                return nullptr;
            }

            const int accessor_stride = accessor.ByteStride(*buffer_view);
            stride = accessor_stride > 0 ? static_cast<size_t>(accessor_stride) : 0;
            return buffer.data.data() + byte_offset;
        }

        bool readFloatVec3Accessor(const tinygltf::Model& model, const tinygltf::Accessor& accessor,
                                   std::vector<glm::vec3>& values)
        {
            if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC3)
            {
                return false;
            }

            const tinygltf::BufferView* buffer_view = nullptr;
            size_t stride = 0;
            const uint8_t* data = accessorData(model, accessor, buffer_view, stride);
            if (!data)
            {
                return false;
            }

            stride = stride == 0 ? sizeof(float) * 3 : stride;
            values.resize(accessor.count);
            for (size_t i = 0; i < accessor.count; ++i)
            {
                const float* src = reinterpret_cast<const float*>(data + i * stride);
                values[i] = glm::vec3(src[0], src[1], src[2]);
            }
            return true;
        }

        bool readFloatVec2Accessor(const tinygltf::Model& model, const tinygltf::Accessor& accessor,
                                   std::vector<glm::vec2>& values)
        {
            if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC2)
            {
                return false;
            }

            const tinygltf::BufferView* buffer_view = nullptr;
            size_t stride = 0;
            const uint8_t* data = accessorData(model, accessor, buffer_view, stride);
            if (!data)
            {
                return false;
            }

            stride = stride == 0 ? sizeof(float) * 2 : stride;
            values.resize(accessor.count);
            for (size_t i = 0; i < accessor.count; ++i)
            {
                const float* src = reinterpret_cast<const float*>(data + i * stride);
                values[i] = glm::vec2(src[0], src[1]);
            }
            return true;
        }

        bool readFloatVec4Accessor(const tinygltf::Model& model, const tinygltf::Accessor& accessor,
                                   std::vector<glm::vec4>& values)
        {
            if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC4)
            {
                return false;
            }

            const tinygltf::BufferView* buffer_view = nullptr;
            size_t stride = 0;
            const uint8_t* data = accessorData(model, accessor, buffer_view, stride);
            if (!data)
            {
                return false;
            }

            stride = stride == 0 ? sizeof(float) * 4 : stride;
            values.resize(accessor.count);
            for (size_t i = 0; i < accessor.count; ++i)
            {
                const float* src = reinterpret_cast<const float*>(data + i * stride);
                const glm::vec3 tangent = normalizeOr(glm::vec3(src[0], src[1], src[2]), glm::vec3(1.0f, 0.0f, 0.0f));
                values[i] = glm::vec4(tangent, src[3] < 0.0f ? -1.0f : 1.0f);
            }
            return true;
        }

        bool readIndices(const tinygltf::Model& model, const tinygltf::Primitive& primitive,
                         std::vector<uint32_t>& indices)
        {
            if (primitive.indices < 0 || primitive.indices >= static_cast<int>(model.accessors.size()))
            {
                return false;
            }

            const tinygltf::Accessor& accessor = model.accessors[static_cast<size_t>(primitive.indices)];
            if (accessor.type != TINYGLTF_TYPE_SCALAR)
            {
                return false;
            }

            const tinygltf::BufferView* buffer_view = nullptr;
            size_t stride = 0;
            const uint8_t* data = accessorData(model, accessor, buffer_view, stride);
            if (!data)
            {
                return false;
            }

            size_t component_size = 0;
            switch (accessor.componentType)
            {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    component_size = sizeof(uint8_t);
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    component_size = sizeof(uint16_t);
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    component_size = sizeof(uint32_t);
                    break;
                default:
                    return false;
            }

            stride = stride == 0 ? component_size : stride;
            indices.resize(accessor.count);
            for (size_t i = 0; i < accessor.count; ++i)
            {
                const uint8_t* src = data + i * stride;
                if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                {
                    indices[i] = *src;
                }
                else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                {
                    uint16_t value = 0;
                    std::memcpy(&value, src, sizeof(value));
                    indices[i] = value;
                }
                else
                {
                    uint32_t value = 0;
                    std::memcpy(&value, src, sizeof(value));
                    indices[i] = value;
                }
            }
            return true;
        }

        glm::mat4 readFirstNodeTransform(const tinygltf::Model& model)
        {
            if (model.scenes.empty() || model.defaultScene < 0 ||
                model.defaultScene >= static_cast<int>(model.scenes.size()))
            {
                return glm::mat4(1.0f);
            }

            const tinygltf::Scene& scene = model.scenes[static_cast<size_t>(model.defaultScene)];
            if (scene.nodes.empty())
            {
                return glm::mat4(1.0f);
            }

            const int node_index = scene.nodes.front();
            if (node_index < 0 || node_index >= static_cast<int>(model.nodes.size()))
            {
                return glm::mat4(1.0f);
            }

            const tinygltf::Node& node = model.nodes[static_cast<size_t>(node_index)];
            if (node.matrix.size() == 16)
            {
                glm::mat4 matrix(1.0f);
                for (size_t column = 0; column < 4; ++column)
                {
                    for (size_t row = 0; row < 4; ++row)
                    {
                        matrix[static_cast<glm::length_t>(column)][static_cast<glm::length_t>(row)] =
                            static_cast<float>(node.matrix[column * 4 + row]);
                    }
                }
                return matrix;
            }

            glm::mat4 transform(1.0f);
            if (node.translation.size() == 3)
            {
                transform[3][0] = static_cast<float>(node.translation[0]);
                transform[3][1] = static_cast<float>(node.translation[1]);
                transform[3][2] = static_cast<float>(node.translation[2]);
            }
            if (node.rotation.size() == 4)
            {
                const glm::quat rotation(static_cast<float>(node.rotation[3]), static_cast<float>(node.rotation[0]),
                                         static_cast<float>(node.rotation[1]), static_cast<float>(node.rotation[2]));
                transform *= glm::mat4_cast(rotation);
            }
            if (node.scale.size() == 3)
            {
                glm::mat4 scale(1.0f);
                scale[0][0] = static_cast<float>(node.scale[0]);
                scale[1][1] = static_cast<float>(node.scale[1]);
                scale[2][2] = static_cast<float>(node.scale[2]);
                transform *= scale;
            }

            return transform;
        }

        std::vector<uint8_t> makeRgba8(const tinygltf::Image& image)
        {
            if (image.width <= 0 || image.height <= 0 || image.bits != 8 ||
                image.pixel_type != TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE || image.component <= 0)
            {
                return {};
            }

            const size_t pixel_count = static_cast<size_t>(image.width) * static_cast<size_t>(image.height);
            std::vector<uint8_t> rgba(pixel_count * 4);
            const size_t component_count = static_cast<size_t>(image.component);
            for (size_t pixel = 0; pixel < pixel_count; ++pixel)
            {
                const uint8_t* src = image.image.data() + pixel * component_count;
                uint8_t* dst = rgba.data() + pixel * 4;
                dst[0] = src[0];
                dst[1] = component_count > 1 ? src[1] : src[0];
                dst[2] = component_count > 2 ? src[2] : src[0];
                dst[3] = component_count > 3 ? src[3] : 255;
            }
            return rgba;
        }

        bool getTextureSourceImage(const tinygltf::Model& model, int texture_index, const tinygltf::Image*& image,
                                   std::string& image_name)
        {
            if (texture_index < 0 || texture_index >= static_cast<int>(model.textures.size()))
            {
                return false;
            }

            const tinygltf::Texture& texture = model.textures[static_cast<size_t>(texture_index)];
            if (texture.source < 0 || texture.source >= static_cast<int>(model.images.size()))
            {
                return false;
            }

            image = &model.images[static_cast<size_t>(texture.source)];
            image_name = image->uri;
            return true;
        }

        bool createMaterialTexture(IRenderer& renderer, const tinygltf::Model& tiny_model, int texture_index,
                                   ETextureFormat format, const std::string& debug_name, TextureHandle& texture_handle,
                                   std::string& texture_name, uint32_t& texture_mip_levels)
        {
            const tinygltf::Image* image = nullptr;
            if (!getTextureSourceImage(tiny_model, texture_index, image, texture_name))
            {
                return false;
            }

            std::vector<uint8_t> rgba = makeRgba8(*image);
            if (rgba.empty())
            {
                return false;
            }

            texture_mip_levels =
                textureFullMipLevelCount(static_cast<uint32_t>(image->width), static_cast<uint32_t>(image->height));
            texture_handle = renderer.createTexture({
                .width = static_cast<uint32_t>(image->width),
                .height = static_cast<uint32_t>(image->height),
                .format = format,
                .mip_levels = texture_mip_levels,
                .generate_mipmaps = texture_mip_levels > 1,
                .sampled = true,
                .debug_name = debug_name,
            });
            return texture_handle.isValid() && renderer.uploadTexture(texture_handle, rgba.data(), rgba.size());
        }

        bool createFallbackMaterialTexture(IRenderer& renderer, ETextureFormat format,
                                           const std::array<uint8_t, 4>& rgba, const std::string& debug_name,
                                           const char* fallback_name, TextureHandle& texture_handle,
                                           std::string& texture_name, uint32_t& texture_mip_levels)
        {
            texture_mip_levels = 1;
            texture_name = fallback_name;
            texture_handle = renderer.createTexture({
                .width = 1,
                .height = 1,
                .format = format,
                .mip_levels = 1,
                .generate_mipmaps = false,
                .sampled = true,
                .debug_name = debug_name,
            });
            return texture_handle.isValid() && renderer.uploadTexture(texture_handle, rgba.data(), rgba.size());
        }

        bool createSceneMaterialTexture(IRenderer& renderer, const tinygltf::Model& tiny_model, int texture_index,
                                        ETextureFormat format, const std::array<uint8_t, 4>& fallback_rgba,
                                        const std::string& debug_name, const char* fallback_name,
                                        TextureHandle& texture_handle, std::string& texture_name,
                                        uint32_t& texture_mip_levels)
        {
            if (texture_index >= 0)
            {
                return createMaterialTexture(renderer, tiny_model, texture_index, format, debug_name, texture_handle,
                                             texture_name, texture_mip_levels);
            }

            return createFallbackMaterialTexture(renderer, format, fallback_rgba, debug_name, fallback_name,
                                                 texture_handle, texture_name, texture_mip_levels);
        }

        ESamplerFilter gltfMagFilterToESamplerFilter(int filter)
        {
            return filter == TINYGLTF_TEXTURE_FILTER_NEAREST ? ESamplerFilter::NEAREST : ESamplerFilter::LINEAR;
        }

        ESamplerFilter gltfMinFilterToESamplerFilter(int filter)
        {
            switch (filter)
            {
                case TINYGLTF_TEXTURE_FILTER_NEAREST:
                case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
                case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
                    return ESamplerFilter::NEAREST;
                case TINYGLTF_TEXTURE_FILTER_LINEAR:
                case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
                case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
                default:
                    return ESamplerFilter::LINEAR;
            }
        }

        ESamplerMipFilter gltfMinFilterToMipFilter(int filter)
        {
            switch (filter)
            {
                case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
                case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
                    return ESamplerMipFilter::NEAREST;
                case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
                case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
                default:
                    return ESamplerMipFilter::LINEAR;
            }
        }

        bool gltfMinFilterUsesMipmaps(int filter)
        {
            return filter < 0 || filter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST ||
                   filter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST ||
                   filter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR ||
                   filter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;
        }

        ESamplerAddressMode gltfWrapToAddressMode(int wrap)
        {
            switch (wrap)
            {
                case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                    return ESamplerAddressMode::CLAMP_TO_EDGE;
                case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
                    return ESamplerAddressMode::MIRRORED_REPEAT;
                case TINYGLTF_TEXTURE_WRAP_REPEAT:
                default:
                    return ESamplerAddressMode::REPEAT;
            }
        }

        SamplerDesc createMaterialSamplerDesc(const tinygltf::Model& model, int texture_index, uint32_t max_mip_levels,
                                              const std::string& debug_name)
        {
            SamplerDesc desc{
                .address_mode_u = ESamplerAddressMode::REPEAT,
                .address_mode_v = ESamplerAddressMode::REPEAT,
                .debug_name = debug_name,
            };
            const uint32_t max_lod = max_mip_levels > 0 ? max_mip_levels - 1 : 0;

            if (texture_index < 0 || texture_index >= static_cast<int>(model.textures.size()))
            {
                desc.max_lod = static_cast<float>(max_lod);
                return desc;
            }

            const tinygltf::Texture& texture = model.textures[static_cast<size_t>(texture_index)];
            if (texture.sampler < 0 || texture.sampler >= static_cast<int>(model.samplers.size()))
            {
                desc.max_lod = static_cast<float>(max_lod);
                return desc;
            }

            const tinygltf::Sampler& sampler = model.samplers[static_cast<size_t>(texture.sampler)];
            desc.mag_filter = gltfMagFilterToESamplerFilter(sampler.magFilter);
            desc.min_filter = gltfMinFilterToESamplerFilter(sampler.minFilter);
            desc.mip_filter = gltfMinFilterToMipFilter(sampler.minFilter);
            desc.address_mode_u = gltfWrapToAddressMode(sampler.wrapS);
            desc.address_mode_v = gltfWrapToAddressMode(sampler.wrapT);
            desc.max_lod = gltfMinFilterUsesMipmaps(sampler.minFilter) ? static_cast<float>(max_lod) : 0.0f;
            return desc;
        }

        struct MaterialTextureBinding
        {
            const char* label = nullptr;
            int texture_index = -1;
            int tex_coord = 0;
            const tinygltf::ExtensionMap* extensions = nullptr;
        };

        bool matchingStaticMaterialSamplers(const SamplerDesc& lhs, const SamplerDesc& rhs)
        {
            return lhs.min_filter == rhs.min_filter && lhs.mag_filter == rhs.mag_filter &&
                   lhs.mip_filter == rhs.mip_filter && lhs.address_mode_u == rhs.address_mode_u &&
                   lhs.address_mode_v == rhs.address_mode_v && lhs.max_lod == rhs.max_lod;
        }

        bool selectStaticMaterialSamplerTexture(const tinygltf::Model& model, const tinygltf::Material& material,
                                                int& sampler_texture_index, std::string& error_message)
        {
            const std::array<MaterialTextureBinding, 5> bindings = {{
                {"base color texture", material.pbrMetallicRoughness.baseColorTexture.index,
                 material.pbrMetallicRoughness.baseColorTexture.texCoord,
                 &material.pbrMetallicRoughness.baseColorTexture.extensions},
                {"metallic-roughness texture", material.pbrMetallicRoughness.metallicRoughnessTexture.index,
                 material.pbrMetallicRoughness.metallicRoughnessTexture.texCoord,
                 &material.pbrMetallicRoughness.metallicRoughnessTexture.extensions},
                {"emissive texture", material.emissiveTexture.index, material.emissiveTexture.texCoord,
                 &material.emissiveTexture.extensions},
                {"occlusion texture", material.occlusionTexture.index, material.occlusionTexture.texCoord,
                 &material.occlusionTexture.extensions},
                {"normal texture", material.normalTexture.index, material.normalTexture.texCoord,
                 &material.normalTexture.extensions},
            }};

            sampler_texture_index = -1;
            const char* sampler_label = nullptr;
            SamplerDesc sampler_desc{};
            bool has_sampler = false;
            for (const MaterialTextureBinding& binding : bindings)
            {
                if (binding.texture_index == -1)
                {
                    continue;
                }
                if (binding.texture_index < -1 || binding.texture_index >= static_cast<int>(model.textures.size()))
                {
                    error_message =
                        "glTF material " + std::string(binding.label) + " references an invalid texture index.";
                    return false;
                }
                if (binding.tex_coord != 0)
                {
                    error_message = "glTF material " + std::string(binding.label) + " uses TEXCOORD_" +
                                    std::to_string(binding.tex_coord) +
                                    ", but the static loader supports only TEXCOORD_0.";
                    return false;
                }
                if (binding.extensions &&
                    binding.extensions->find("KHR_texture_transform") != binding.extensions->end())
                {
                    error_message = "glTF material " + std::string(binding.label) +
                                    " uses KHR_texture_transform, which the static loader does not support.";
                    return false;
                }

                const tinygltf::Texture& texture = model.textures[static_cast<size_t>(binding.texture_index)];
                if (texture.sampler < -1 || texture.sampler >= static_cast<int>(model.samplers.size()))
                {
                    error_message =
                        "glTF material " + std::string(binding.label) + " references an invalid sampler index.";
                    return false;
                }

                // A material currently exposes a single sampler, so compare the effective state,
                // including whether minification permits mip levels, rather than sampler indices.
                const SamplerDesc candidate = createMaterialSamplerDesc(model, binding.texture_index, 2, std::string{});
                if (!has_sampler)
                {
                    sampler_texture_index = binding.texture_index;
                    sampler_label = binding.label;
                    sampler_desc = candidate;
                    has_sampler = true;
                }
                else if (!matchingStaticMaterialSamplers(sampler_desc, candidate))
                {
                    error_message = "glTF material uses incompatible samplers for " + std::string(sampler_label) +
                                    " and " + binding.label + "; the static loader supports one sampler per material.";
                    return false;
                }
            }
            return true;
        }

        GltfMaterialFactors readMaterialFactors(const tinygltf::Material& material);

        RendererResult<GltfLoadedModel> loadSceneMaterial(IRenderer& renderer, const tinygltf::Model& tiny_model,
                                                          int material_index, const GltfLoadDesc& desc)
        {
            GltfLoadedModel material_resources;
            if (material_index < 0 || material_index >= static_cast<int>(tiny_model.materials.size()))
            {
                return RendererResult<GltfLoadedModel>::failure(ERendererErrorCode::VALIDATION_FAILED,
                                                                "glTF scene primitive references an invalid material.");
            }

            const tinygltf::Material& material = tiny_model.materials[static_cast<size_t>(material_index)];
            material_resources.material_factors = readMaterialFactors(material);
            if (material_resources.material_factors.alpha_mode == EGltfAlphaMode::ALPHA_BLEND)
            {
                return RendererResult<GltfLoadedModel>::failure(
                    ERendererErrorCode::UNSUPPORTED,
                    "glTF scene material uses alpha blending, which the static scene loader does not support.");
            }

            int sampler_texture_index = -1;
            std::string material_support_error;
            if (!selectStaticMaterialSamplerTexture(tiny_model, material, sampler_texture_index,
                                                    material_support_error))
            {
                return RendererResult<GltfLoadedModel>::failure(ERendererErrorCode::UNSUPPORTED,
                                                                std::move(material_support_error));
            }

            uint32_t base_color_mip_levels = 1;
            uint32_t metal_roughness_mip_levels = 1;
            uint32_t emissive_mip_levels = 1;
            uint32_t occlusion_mip_levels = 1;
            uint32_t normal_mip_levels = 1;
            UploadBatchGuard upload_batch(renderer);
            if (!upload_batch.started())
            {
                return failLoad(renderer, material_resources, ERendererErrorCode::BACKEND_FAILURE,
                                "Failed to start glTF scene texture upload batch.");
            }

            const bool loaded_all_textures =
                createSceneMaterialTexture(renderer, tiny_model, material.pbrMetallicRoughness.baseColorTexture.index,
                                           ETextureFormat::RGB_A8_SRGB, {255, 255, 255, 255},
                                           makeDebugName(desc, "Scene Base Color"), "fallback-base-color",
                                           material_resources.material_textures.base_color,
                                           material_resources.texture_names.base_color, base_color_mip_levels) &&
                createSceneMaterialTexture(
                    renderer, tiny_model, material.pbrMetallicRoughness.metallicRoughnessTexture.index,
                    ETextureFormat::RGBA8, {0, 255, 0, 255}, makeDebugName(desc, "Scene Metal Roughness"),
                    "fallback-metal-roughness", material_resources.material_textures.metal_roughness,
                    material_resources.texture_names.metal_roughness, metal_roughness_mip_levels) &&
                createSceneMaterialTexture(renderer, tiny_model, material.emissiveTexture.index,
                                           ETextureFormat::RGB_A8_SRGB, {0, 0, 0, 255},
                                           makeDebugName(desc, "Scene Emissive"), "fallback-emissive",
                                           material_resources.material_textures.emissive,
                                           material_resources.texture_names.emissive, emissive_mip_levels) &&
                createSceneMaterialTexture(renderer, tiny_model, material.occlusionTexture.index, ETextureFormat::RGBA8,
                                           {255, 255, 255, 255}, makeDebugName(desc, "Scene Occlusion"),
                                           "fallback-occlusion", material_resources.material_textures.occlusion,
                                           material_resources.texture_names.occlusion, occlusion_mip_levels) &&
                createSceneMaterialTexture(renderer, tiny_model, material.normalTexture.index, ETextureFormat::RGBA8,
                                           {128, 128, 255, 255}, makeDebugName(desc, "Scene Normal"), "fallback-normal",
                                           material_resources.material_textures.normal,
                                           material_resources.texture_names.normal, normal_mip_levels);
            if (!loaded_all_textures)
            {
                upload_batch.cancel();
                return failLoad(renderer, material_resources, ERendererErrorCode::UNSUPPORTED,
                                "glTF scene material uses an unsupported image format or texture reference.");
            }
            if (!upload_batch.finish())
            {
                return failLoad(renderer, material_resources, ERendererErrorCode::BACKEND_FAILURE,
                                "Failed to submit glTF scene texture upload batch.");
            }

            const uint32_t max_material_mip_levels =
                std::max({base_color_mip_levels, metal_roughness_mip_levels, emissive_mip_levels, occlusion_mip_levels,
                          normal_mip_levels});
            material_resources.material_sampler = renderer.createSampler(
                createMaterialSamplerDesc(tiny_model, sampler_texture_index, max_material_mip_levels,
                                          makeDebugName(desc, "Scene Material Sampler")));
            if (!material_resources.material_sampler.isValid())
            {
                return failLoad(renderer, material_resources, ERendererErrorCode::BACKEND_FAILURE,
                                "Failed to create glTF scene material sampler.");
            }

            return RendererResult<GltfLoadedModel>::success(std::move(material_resources));
        }

        bool validateTriangleIndices(const std::vector<uint32_t>& indices, size_t vertex_count)
        {
            if (indices.size() % 3 != 0)
            {
                return false;
            }

            for (uint32_t index : indices)
            {
                if (index >= vertex_count)
                {
                    return false;
                }
            }
            return true;
        }

        float cornerAngle(const glm::vec3& from, const glm::vec3& to)
        {
            const glm::vec3 a = normalizeOr(from, glm::vec3(1.0f, 0.0f, 0.0f));
            const glm::vec3 b = normalizeOr(to, glm::vec3(1.0f, 0.0f, 0.0f));
            return std::acos(std::clamp(glm::dot(a, b), -1.0f, 1.0f));
        }

        void generateTangents(const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals,
                              const std::vector<glm::vec2>& uvs, const std::vector<uint32_t>& indices,
                              std::vector<glm::vec4>& tangents)
        {
            std::vector<glm::vec3> tangent_sums(positions.size(), glm::vec3(0.0f));
            std::vector<glm::vec3> bitangent_sums(positions.size(), glm::vec3(0.0f));

            for (size_t i = 0; i + 2 < indices.size(); i += 3)
            {
                const uint32_t i0 = indices[i + 0];
                const uint32_t i1 = indices[i + 1];
                const uint32_t i2 = indices[i + 2];

                const glm::vec3 edge1 = positions[i1] - positions[i0];
                const glm::vec3 edge2 = positions[i2] - positions[i0];
                const glm::vec2 delta_uv1 = uvs[i1] - uvs[i0];
                const glm::vec2 delta_uv2 = uvs[i2] - uvs[i0];
                const float determinant = delta_uv1.x * delta_uv2.y - delta_uv1.y * delta_uv2.x;
                if (std::abs(determinant) <= 0.000001f)
                {
                    continue;
                }

                const float reciprocal = 1.0f / determinant;
                const glm::vec3 tangent = (edge1 * delta_uv2.y - edge2 * delta_uv1.y) * reciprocal;
                const glm::vec3 bitangent = (edge2 * delta_uv1.x - edge1 * delta_uv2.x) * reciprocal;

                const float angle0 = cornerAngle(edge1, edge2);
                const float angle1 = cornerAngle(-edge1, positions[i2] - positions[i1]);
                const float angle2 = cornerAngle(-edge2, positions[i1] - positions[i2]);

                tangent_sums[i0] += tangent * angle0;
                tangent_sums[i1] += tangent * angle1;
                tangent_sums[i2] += tangent * angle2;
                bitangent_sums[i0] += bitangent * angle0;
                bitangent_sums[i1] += bitangent * angle1;
                bitangent_sums[i2] += bitangent * angle2;
            }

            tangents.resize(positions.size());
            for (size_t i = 0; i < positions.size(); ++i)
            {
                const glm::vec3 normal = normalizeOr(normals[i], glm::vec3(0.0f, 1.0f, 0.0f));
                glm::vec3 tangent = tangent_sums[i] - normal * glm::dot(normal, tangent_sums[i]);
                tangent = normalizeOr(tangent, fallbackTangentForNormal(normal));
                const float handedness = glm::dot(glm::cross(normal, tangent), bitangent_sums[i]) < 0.0f ? -1.0f : 1.0f;
                tangents[i] = glm::vec4(tangent, handedness);
            }
        }

        float readFactor(const std::vector<double>& values, size_t index, float fallback)
        {
            return index < values.size() ? static_cast<float>(values[index]) : fallback;
        }

        GltfMaterialFactors readMaterialFactors(const tinygltf::Material& material)
        {
            GltfMaterialFactors factors;
            factors.base_color = glm::vec4(readFactor(material.pbrMetallicRoughness.baseColorFactor, 0, 1.0f),
                                           readFactor(material.pbrMetallicRoughness.baseColorFactor, 1, 1.0f),
                                           readFactor(material.pbrMetallicRoughness.baseColorFactor, 2, 1.0f),
                                           readFactor(material.pbrMetallicRoughness.baseColorFactor, 3, 1.0f));
            factors.emissive =
                glm::vec3(readFactor(material.emissiveFactor, 0, 0.0f), readFactor(material.emissiveFactor, 1, 0.0f),
                          readFactor(material.emissiveFactor, 2, 0.0f));
            factors.metallic = static_cast<float>(material.pbrMetallicRoughness.metallicFactor);
            factors.roughness = static_cast<float>(material.pbrMetallicRoughness.roughnessFactor);
            factors.normal_scale = static_cast<float>(material.normalTexture.scale);
            factors.occlusion_strength = static_cast<float>(material.occlusionTexture.strength);
            factors.alpha_cutoff = static_cast<float>(material.alphaCutoff);
            if (material.alphaMode == "MASK")
            {
                factors.alpha_mode = EGltfAlphaMode::ALPHA_MASK;
            }
            else if (material.alphaMode == "BLEND")
            {
                factors.alpha_mode = EGltfAlphaMode::ALPHA_BLEND;
            }
            else
            {
                factors.alpha_mode = EGltfAlphaMode::ALPHA_OPAQUE;
            }
            factors.double_sided = material.doubleSided;
            return factors;
        }

    }  // namespace

    RendererResult<GltfLoadedModel> loadGltfModel(IRenderer& renderer, const GltfLoadDesc& desc)
    {
        GltfLoadedModel loaded_model;
        if (desc.path.empty())
        {
            return RendererResult<GltfLoadedModel>::failure(ERendererErrorCode::VALIDATION_FAILED,
                                                            "glTF path is empty.");
        }

        tinygltf::TinyGLTF loader;
        tinygltf::Model tiny_model;
        std::string error;
        std::string warning;
        if (!loader.LoadASCIIFromFile(&tiny_model, &error, &warning, desc.path))
        {
            if (!warning.empty())
            {
                Logger::getInstance().warning("glTF loader warning: " + warning);
            }
            const std::string message =
                error.empty() ? "Failed to load glTF file: " + desc.path : "Failed to load glTF file: " + error;
            return failLoad(renderer, loaded_model, ERendererErrorCode::VALIDATION_FAILED, message);
        }
        if (!warning.empty())
        {
            Logger::getInstance().warning("glTF loader warning: " + warning);
        }

        if (tiny_model.meshes.empty() || tiny_model.meshes.front().primitives.empty())
        {
            return failLoad(renderer, loaded_model, ERendererErrorCode::UNSUPPORTED,
                            "glTF file does not contain a mesh primitive.");
        }

        const tinygltf::Primitive& primitive = tiny_model.meshes.front().primitives.front();
        if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
        {
            return failLoad(renderer, loaded_model, ERendererErrorCode::UNSUPPORTED,
                            "glTF loader currently supports only triangle-list primitives.");
        }

        const tinygltf::Accessor* position_accessor = findAccessor(tiny_model, primitive, "POSITION");
        const tinygltf::Accessor* normal_accessor = findAccessor(tiny_model, primitive, "NORMAL");
        const tinygltf::Accessor* uv_accessor = findAccessor(tiny_model, primitive, "TEXCOORD_0");
        const tinygltf::Accessor* tangent_accessor = findAccessor(tiny_model, primitive, "TANGENT");
        if (!position_accessor || !normal_accessor || !uv_accessor)
        {
            return failLoad(renderer, loaded_model, ERendererErrorCode::UNSUPPORTED,
                            "glTF primitive is missing POSITION, NORMAL, or TEXCOORD_0.");
        }

        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> uvs;
        std::vector<uint32_t> indices;
        if (!readFloatVec3Accessor(tiny_model, *position_accessor, positions) ||
            !readFloatVec3Accessor(tiny_model, *normal_accessor, normals) ||
            !readFloatVec2Accessor(tiny_model, *uv_accessor, uvs) || !readIndices(tiny_model, primitive, indices))
        {
            return failLoad(renderer, loaded_model, ERendererErrorCode::UNSUPPORTED,
                            "glTF primitive contains an unsupported accessor format.");
        }

        if (positions.empty() || positions.size() != normals.size() || positions.size() != uvs.size() ||
            indices.empty() || !validateTriangleIndices(indices, positions.size()))
        {
            return failLoad(renderer, loaded_model, ERendererErrorCode::VALIDATION_FAILED,
                            "glTF mesh attribute counts or triangle indices are invalid.");
        }

        std::vector<glm::vec4> tangents;
        if (tangent_accessor && !readFloatVec4Accessor(tiny_model, *tangent_accessor, tangents))
        {
            return failLoad(renderer, loaded_model, ERendererErrorCode::UNSUPPORTED,
                            "glTF primitive contains an unsupported TANGENT accessor format.");
        }
        if (tangents.size() != positions.size())
        {
            generateTangents(positions, normals, uvs, indices, tangents);
        }

        std::vector<GltfVertex> vertices(positions.size());
        for (size_t i = 0; i < vertices.size(); ++i)
        {
            vertices[i] = {positions[i], normals[i], uvs[i], tangents[i]};
        }

        loaded_model.vertex_buffer = renderer.createBuffer({
            .size = vertices.size() * sizeof(GltfVertex),
            .usage = EBufferUsageKind::VERTEX,
            .memory_access = EMemoryAccess::CPU_WRITE,
            .debug_name = makeDebugName(desc, "Vertex Buffer"),
        });
        loaded_model.index_buffer = renderer.createBuffer({
            .size = indices.size() * sizeof(uint32_t),
            .usage = EBufferUsageKind::INDEX,
            .memory_access = EMemoryAccess::CPU_WRITE,
            .debug_name = makeDebugName(desc, "Index Buffer"),
        });
        if (!loaded_model.vertex_buffer.isValid() || !loaded_model.index_buffer.isValid() ||
            !renderer.uploadBuffer(loaded_model.vertex_buffer, vertices.data(), vertices.size() * sizeof(GltfVertex)) ||
            !renderer.uploadBuffer(loaded_model.index_buffer, indices.data(), indices.size() * sizeof(uint32_t)))
        {
            return failLoad(renderer, loaded_model, ERendererErrorCode::BACKEND_FAILURE,
                            "Failed to create or upload glTF mesh buffers.");
        }

        loaded_model.index_format = EIndexFormat::U_INT32;
        loaded_model.index_count = static_cast<uint32_t>(indices.size());
        loaded_model.transform = readFirstNodeTransform(tiny_model);

        if (primitive.material < 0 || primitive.material >= static_cast<int>(tiny_model.materials.size()))
        {
            return failLoad(renderer, loaded_model, ERendererErrorCode::VALIDATION_FAILED,
                            "glTF primitive does not reference a valid material.");
        }

        const tinygltf::Material& material = tiny_model.materials[static_cast<size_t>(primitive.material)];
        loaded_model.material_factors = readMaterialFactors(material);

        int sampler_texture_index = -1;
        std::string material_support_error;
        if (!selectStaticMaterialSamplerTexture(tiny_model, material, sampler_texture_index, material_support_error))
        {
            return failLoad(renderer, loaded_model, ERendererErrorCode::UNSUPPORTED, std::move(material_support_error));
        }

        uint32_t max_material_mip_levels = 1;
        uint32_t base_color_mip_levels = 1;
        uint32_t metal_roughness_mip_levels = 1;
        uint32_t emissive_mip_levels = 1;
        uint32_t occlusion_mip_levels = 1;
        uint32_t normal_mip_levels = 1;
        UploadBatchGuard upload_batch(renderer);
        if (!upload_batch.started())
        {
            return failLoad(renderer, loaded_model, ERendererErrorCode::BACKEND_FAILURE,
                            "Failed to upload glTF material textures upload batch");
        }

        const bool loaded_all_textures =
            createMaterialTexture(renderer, tiny_model, material.pbrMetallicRoughness.baseColorTexture.index,
                                  ETextureFormat::RGB_A8_SRGB, makeDebugName(desc, "Base Color"),
                                  loaded_model.material_textures.base_color, loaded_model.texture_names.base_color,
                                  base_color_mip_levels) &&
            createMaterialTexture(renderer, tiny_model, material.pbrMetallicRoughness.metallicRoughnessTexture.index,
                                  ETextureFormat::RGBA8, makeDebugName(desc, "Metal Roughness"),
                                  loaded_model.material_textures.metal_roughness,
                                  loaded_model.texture_names.metal_roughness, metal_roughness_mip_levels) &&
            createMaterialTexture(renderer, tiny_model, material.emissiveTexture.index, ETextureFormat::RGB_A8_SRGB,
                                  makeDebugName(desc, "Emissive"), loaded_model.material_textures.emissive,
                                  loaded_model.texture_names.emissive, emissive_mip_levels) &&
            createMaterialTexture(renderer, tiny_model, material.occlusionTexture.index, ETextureFormat::RGBA8,
                                  makeDebugName(desc, "Occlusion"), loaded_model.material_textures.occlusion,
                                  loaded_model.texture_names.occlusion, occlusion_mip_levels) &&
            createMaterialTexture(renderer, tiny_model, material.normalTexture.index, ETextureFormat::RGBA8,
                                  makeDebugName(desc, "Normal"), loaded_model.material_textures.normal,
                                  loaded_model.texture_names.normal, normal_mip_levels);
        if (!loaded_all_textures && desc.require_material_textures)
        {
            upload_batch.cancel();
            return failLoad(renderer, loaded_model, ERendererErrorCode::UNSUPPORTED,
                            "glTF material is missing a required PBR texture or uses an unsupported image format.");
        }

        if (!upload_batch.finish())
        {
            return failLoad(renderer, loaded_model, ERendererErrorCode::BACKEND_FAILURE,
                            "Failed to submit glTF material textures upload batch.");
        }

        max_material_mip_levels = std::max({base_color_mip_levels, metal_roughness_mip_levels, emissive_mip_levels,
                                            occlusion_mip_levels, normal_mip_levels});

        loaded_model.material_sampler = renderer.createSampler(createMaterialSamplerDesc(
            tiny_model, sampler_texture_index, max_material_mip_levels, makeDebugName(desc, "Material Sampler")));
        if (!loaded_model.material_sampler.isValid())
        {
            return failLoad(renderer, loaded_model, ERendererErrorCode::BACKEND_FAILURE,
                            "Failed to create glTF material sampler.");
        }

        return RendererResult<GltfLoadedModel>::success(std::move(loaded_model));
    }

    RendererResult<GltfLoadedScene> loadGltfScene(IRenderer& renderer, const GltfLoadDesc& desc)
    {
        GltfLoadedScene scene;
        if (desc.path.empty())
        {
            return RendererResult<GltfLoadedScene>::failure(ERendererErrorCode::VALIDATION_FAILED,
                                                            "glTF path is empty.");
        }

        tinygltf::TinyGLTF loader;
        tinygltf::Model tiny_model;
        std::string error;
        std::string warning;
        if (!loader.LoadASCIIFromFile(&tiny_model, &error, &warning, desc.path))
        {
            return RendererResult<GltfLoadedScene>::failure(
                ERendererErrorCode::VALIDATION_FAILED,
                error.empty() ? "Failed to load glTF file: " + desc.path : "Failed to load glTF file: " + error);
        }
        if (tiny_model.scenes.empty())
        {
            return RendererResult<GltfLoadedScene>::failure(ERendererErrorCode::UNSUPPORTED,
                                                            "glTF file does not contain a scene.");
        }

        std::vector<GltfLoadedModel> material_cache(tiny_model.materials.size());
        std::vector<bool> material_loaded(tiny_model.materials.size(), false);

        const int scene_index = tiny_model.defaultScene >= 0 ? tiny_model.defaultScene : 0;
        if (scene_index >= static_cast<int>(tiny_model.scenes.size()))
        {
            return RendererResult<GltfLoadedScene>::failure(ERendererErrorCode::VALIDATION_FAILED,
                                                            "glTF default scene is invalid.");
        }

        struct PendingNode
        {
            int index;
            glm::mat4 parent_transform;
        };
        std::vector<PendingNode> pending;
        for (int node_index : tiny_model.scenes[static_cast<size_t>(scene_index)].nodes)
        {
            pending.push_back({node_index, glm::mat4(1.0f)});
        }

        while (!pending.empty())
        {
            const PendingNode pending_node = pending.back();
            pending.pop_back();
            if (pending_node.index < 0 || pending_node.index >= static_cast<int>(tiny_model.nodes.size()))
            {
                destroyGltfScene(renderer, scene);
                return RendererResult<GltfLoadedScene>::failure(ERendererErrorCode::VALIDATION_FAILED,
                                                                "glTF scene references an invalid node.");
            }

            const tinygltf::Node& node = tiny_model.nodes[static_cast<size_t>(pending_node.index)];
            glm::mat4 local_transform(1.0f);
            if (node.matrix.size() == 16)
            {
                for (size_t column = 0; column < 4; ++column)
                {
                    for (size_t row = 0; row < 4; ++row)
                    {
                        local_transform[static_cast<glm::length_t>(column)][static_cast<glm::length_t>(row)] =
                            static_cast<float>(node.matrix[column * 4 + row]);
                    }
                }
            }
            else
            {
                if (node.translation.size() == 3)
                {
                    local_transform[3][0] = static_cast<float>(node.translation[0]);
                    local_transform[3][1] = static_cast<float>(node.translation[1]);
                    local_transform[3][2] = static_cast<float>(node.translation[2]);
                }
                if (node.rotation.size() == 4)
                {
                    const glm::quat rotation(static_cast<float>(node.rotation[3]), static_cast<float>(node.rotation[0]),
                                             static_cast<float>(node.rotation[1]),
                                             static_cast<float>(node.rotation[2]));
                    local_transform *= glm::mat4_cast(rotation);
                }
                if (node.scale.size() == 3)
                {
                    glm::mat4 scale(1.0f);
                    scale[0][0] = static_cast<float>(node.scale[0]);
                    scale[1][1] = static_cast<float>(node.scale[1]);
                    scale[2][2] = static_cast<float>(node.scale[2]);
                    local_transform *= scale;
                }
            }
            const glm::mat4 world_transform = pending_node.parent_transform * local_transform;
            for (int child : node.children)
            {
                pending.push_back({child, world_transform});
            }

            if (node.mesh < 0)
            {
                continue;
            }
            if (node.mesh >= static_cast<int>(tiny_model.meshes.size()))
            {
                destroyGltfScene(renderer, scene);
                return RendererResult<GltfLoadedScene>::failure(ERendererErrorCode::VALIDATION_FAILED,
                                                                "glTF node references an invalid mesh.");
            }

            const tinygltf::Mesh& mesh = tiny_model.meshes[static_cast<size_t>(node.mesh)];
            for (const tinygltf::Primitive& primitive : mesh.primitives)
            {
                if (primitive.mode != TINYGLTF_MODE_TRIANGLES || primitive.material < 0 ||
                    primitive.material >= static_cast<int>(tiny_model.materials.size()))
                {
                    destroyGltfScene(renderer, scene);
                    return RendererResult<GltfLoadedScene>::failure(
                        ERendererErrorCode::UNSUPPORTED,
                        "glTF scene contains an unsupported primitive or material reference.");
                }

                const tinygltf::Accessor* position_accessor = findAccessor(tiny_model, primitive, "POSITION");
                const tinygltf::Accessor* normal_accessor = findAccessor(tiny_model, primitive, "NORMAL");
                const tinygltf::Accessor* uv_accessor = findAccessor(tiny_model, primitive, "TEXCOORD_0");
                const tinygltf::Accessor* tangent_accessor = findAccessor(tiny_model, primitive, "TANGENT");
                std::vector<glm::vec3> positions;
                std::vector<glm::vec3> normals;
                std::vector<glm::vec2> uvs;
                std::vector<uint32_t> indices;
                std::vector<glm::vec4> tangents;
                if (!position_accessor || !normal_accessor || !uv_accessor ||
                    !readFloatVec3Accessor(tiny_model, *position_accessor, positions) ||
                    !readFloatVec3Accessor(tiny_model, *normal_accessor, normals) ||
                    !readFloatVec2Accessor(tiny_model, *uv_accessor, uvs) ||
                    !readIndices(tiny_model, primitive, indices) || positions.empty() ||
                    positions.size() != normals.size() || positions.size() != uvs.size() || indices.empty() ||
                    !validateTriangleIndices(indices, positions.size()) ||
                    (tangent_accessor && !readFloatVec4Accessor(tiny_model, *tangent_accessor, tangents)))
                {
                    destroyGltfScene(renderer, scene);
                    return RendererResult<GltfLoadedScene>::failure(ERendererErrorCode::UNSUPPORTED,
                                                                    "glTF scene contains unsupported mesh data.");
                }
                if (tangents.size() != positions.size())
                {
                    generateTangents(positions, normals, uvs, indices, tangents);
                }

                GltfLoadedModel draw;
                std::vector<GltfVertex> vertices(positions.size());
                for (size_t i = 0; i < vertices.size(); ++i)
                {
                    vertices[i] = {positions[i], normals[i], uvs[i], tangents[i]};
                }
                draw.vertex_buffer =
                    renderer.createBuffer({vertices.size() * sizeof(GltfVertex), EBufferUsageKind::VERTEX,
                                           EMemoryAccess::CPU_WRITE, makeDebugName(desc, "Scene Vertex Buffer")});
                draw.index_buffer =
                    renderer.createBuffer({indices.size() * sizeof(uint32_t), EBufferUsageKind::INDEX,
                                           EMemoryAccess::CPU_WRITE, makeDebugName(desc, "Scene Index Buffer")});
                if (!draw.vertex_buffer.isValid() || !draw.index_buffer.isValid() ||
                    !renderer.uploadBuffer(draw.vertex_buffer, vertices.data(), vertices.size() * sizeof(GltfVertex)) ||
                    !renderer.uploadBuffer(draw.index_buffer, indices.data(), indices.size() * sizeof(uint32_t)))
                {
                    destroyGltfModel(renderer, draw);
                    destroyGltfScene(renderer, scene);
                    return RendererResult<GltfLoadedScene>::failure(ERendererErrorCode::BACKEND_FAILURE,
                                                                    "Failed to create or upload glTF scene buffers.");
                }
                draw.index_format = EIndexFormat::U_INT32;
                draw.index_count = static_cast<uint32_t>(indices.size());
                draw.transform = world_transform;
                const size_t material_index = static_cast<size_t>(primitive.material);
                if (!material_loaded[material_index])
                {
                    RendererResult<GltfLoadedModel> material_result =
                        loadSceneMaterial(renderer, tiny_model, primitive.material, desc);
                    if (!material_result)
                    {
                        destroyGltfModel(renderer, draw);
                        destroyGltfScene(renderer, scene);
                        return RendererResult<GltfLoadedScene>::failure(material_result.errorCode(),
                                                                        material_result.errorMessage());
                    }
                    material_cache[material_index] = std::move(material_result.value());
                    material_loaded[material_index] = true;
                }

                const GltfLoadedModel& cached_material = material_cache[material_index];
                draw.material_textures = cached_material.material_textures;
                draw.material_sampler = cached_material.material_sampler;
                draw.texture_names = cached_material.texture_names;
                draw.material_factors = cached_material.material_factors;
                scene.draw_items.push_back(std::move(draw));
            }
        }

        if (scene.draw_items.empty())
        {
            return RendererResult<GltfLoadedScene>::failure(ERendererErrorCode::UNSUPPORTED,
                                                            "glTF scene does not contain triangle primitives.");
        }
        return RendererResult<GltfLoadedScene>::success(std::move(scene));
    }

    void destroyGltfModel(IRenderer& renderer, GltfLoadedModel& model)
    {
        if (model.material_textures.normal.isValid())
        {
            renderer.destroyTexture(model.material_textures.normal);
            model.material_textures.normal = {};
        }
        if (model.material_textures.occlusion.isValid())
        {
            renderer.destroyTexture(model.material_textures.occlusion);
            model.material_textures.occlusion = {};
        }
        if (model.material_textures.emissive.isValid())
        {
            renderer.destroyTexture(model.material_textures.emissive);
            model.material_textures.emissive = {};
        }
        if (model.material_textures.metal_roughness.isValid())
        {
            renderer.destroyTexture(model.material_textures.metal_roughness);
            model.material_textures.metal_roughness = {};
        }
        if (model.material_textures.base_color.isValid())
        {
            renderer.destroyTexture(model.material_textures.base_color);
            model.material_textures.base_color = {};
        }
        if (model.material_sampler.isValid())
        {
            renderer.destroySampler(model.material_sampler);
            model.material_sampler = {};
        }
        if (model.index_buffer.isValid())
        {
            renderer.destroyBuffer(model.index_buffer);
            model.index_buffer = {};
        }
        if (model.vertex_buffer.isValid())
        {
            renderer.destroyBuffer(model.vertex_buffer);
            model.vertex_buffer = {};
        }

        model.index_count = 0;
        model.transform = glm::mat4(1.0f);
        model.texture_names = {};
        model.material_factors = {};
    }

    void destroyGltfScene(IRenderer& renderer, GltfLoadedScene& scene)
    {
        std::vector<TextureHandle> textures;
        std::vector<SamplerHandle> samplers;
        auto collect_texture = [&textures](TextureHandle texture)
        {
            if (texture.isValid() && std::find(textures.begin(), textures.end(), texture) == textures.end())
            {
                textures.push_back(texture);
            }
        };
        auto collect_sampler = [&samplers](SamplerHandle sampler)
        {
            if (sampler.isValid() && std::find(samplers.begin(), samplers.end(), sampler) == samplers.end())
            {
                samplers.push_back(sampler);
            }
        };

        for (GltfLoadedModel& draw : scene.draw_items)
        {
            collect_texture(draw.material_textures.base_color);
            collect_texture(draw.material_textures.metal_roughness);
            collect_texture(draw.material_textures.emissive);
            collect_texture(draw.material_textures.occlusion);
            collect_texture(draw.material_textures.normal);
            collect_sampler(draw.material_sampler);

            if (draw.index_buffer.isValid())
            {
                renderer.destroyBuffer(draw.index_buffer);
            }
            if (draw.vertex_buffer.isValid())
            {
                renderer.destroyBuffer(draw.vertex_buffer);
            }
            draw = {};
        }

        for (TextureHandle texture : textures)
        {
            renderer.destroyTexture(texture);
        }
        for (SamplerHandle sampler : samplers)
        {
            renderer.destroySampler(sampler);
        }
        scene.draw_items.clear();
    }

}  // namespace kera
