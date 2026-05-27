#include "kera/renderer/gltf_loader.h"

#include "kera/renderer/interfaces.h"
#include "kera/utilities/logger.h"

#if defined(_MSC_VER)
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
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

namespace kera
{
    namespace
    {
        RendererResult<GltfLoadedModel> failLoad(IRenderer& renderer, GltfLoadedModel& model,
                                                 RendererErrorCode code, std::string message)
        {
            destroyGltfModel(renderer, model);
            return RendererResult<GltfLoadedModel>::failure(code, std::move(message));
        }

        std::string makeDebugName(const GltfLoadDesc& desc, const char* suffix)
        {
            const std::string prefix = desc.debugName.empty() ? "glTF" : desc.debugName;
            return prefix + " " + suffix;
        }

        glm::vec3 normalizeOr(const glm::vec3& value, const glm::vec3& fallback)
        {
            const float lengthSquared = glm::dot(value, value);
            if (lengthSquared <= 0.000001f)
            {
                return fallback;
            }
            return value * (1.0f / std::sqrt(lengthSquared));
        }

        glm::vec3 fallbackTangentForNormal(const glm::vec3& normal)
        {
            const glm::vec3 axis = std::abs(normal.y) < 0.95f ? glm::vec3(0.0f, 1.0f, 0.0f)
                                                              : glm::vec3(1.0f, 0.0f, 0.0f);
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
                                    const tinygltf::BufferView*& bufferView, size_t& stride)
        {
            if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(model.bufferViews.size()))
            {
                return nullptr;
            }

            bufferView = &model.bufferViews[static_cast<size_t>(accessor.bufferView)];
            if (bufferView->buffer < 0 || bufferView->buffer >= static_cast<int>(model.buffers.size()))
            {
                return nullptr;
            }

            const tinygltf::Buffer& buffer = model.buffers[static_cast<size_t>(bufferView->buffer)];
            const size_t byteOffset = static_cast<size_t>(bufferView->byteOffset + accessor.byteOffset);
            if (byteOffset >= buffer.data.size())
            {
                return nullptr;
            }

            const int accessorStride = accessor.ByteStride(*bufferView);
            stride = accessorStride > 0 ? static_cast<size_t>(accessorStride) : 0;
            return buffer.data.data() + byteOffset;
        }

        bool readFloatVec3Accessor(const tinygltf::Model& model, const tinygltf::Accessor& accessor,
                                   std::vector<glm::vec3>& values)
        {
            if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC3)
            {
                return false;
            }

            const tinygltf::BufferView* bufferView = nullptr;
            size_t stride = 0;
            const uint8_t* data = accessorData(model, accessor, bufferView, stride);
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

            const tinygltf::BufferView* bufferView = nullptr;
            size_t stride = 0;
            const uint8_t* data = accessorData(model, accessor, bufferView, stride);
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

            const tinygltf::BufferView* bufferView = nullptr;
            size_t stride = 0;
            const uint8_t* data = accessorData(model, accessor, bufferView, stride);
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

            const tinygltf::BufferView* bufferView = nullptr;
            size_t stride = 0;
            const uint8_t* data = accessorData(model, accessor, bufferView, stride);
            if (!data)
            {
                return false;
            }

            size_t componentSize = 0;
            switch (accessor.componentType)
            {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    componentSize = sizeof(uint8_t);
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    componentSize = sizeof(uint16_t);
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    componentSize = sizeof(uint32_t);
                    break;
                default:
                    return false;
            }

            stride = stride == 0 ? componentSize : stride;
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

            const int nodeIndex = scene.nodes.front();
            if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size()))
            {
                return glm::mat4(1.0f);
            }

            const tinygltf::Node& node = model.nodes[static_cast<size_t>(nodeIndex)];
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

            const size_t pixelCount = static_cast<size_t>(image.width) * static_cast<size_t>(image.height);
            std::vector<uint8_t> rgba(pixelCount * 4);
            const size_t componentCount = static_cast<size_t>(image.component);
            for (size_t pixel = 0; pixel < pixelCount; ++pixel)
            {
                const uint8_t* src = image.image.data() + pixel * componentCount;
                uint8_t* dst = rgba.data() + pixel * 4;
                dst[0] = src[0];
                dst[1] = componentCount > 1 ? src[1] : src[0];
                dst[2] = componentCount > 2 ? src[2] : src[0];
                dst[3] = componentCount > 3 ? src[3] : 255;
            }
            return rgba;
        }

        bool getTextureSourceImage(const tinygltf::Model& model, int textureIndex, const tinygltf::Image*& image,
                                   std::string& imageName)
        {
            if (textureIndex < 0 || textureIndex >= static_cast<int>(model.textures.size()))
            {
                return false;
            }

            const tinygltf::Texture& texture = model.textures[static_cast<size_t>(textureIndex)];
            if (texture.source < 0 || texture.source >= static_cast<int>(model.images.size()))
            {
                return false;
            }

            image = &model.images[static_cast<size_t>(texture.source)];
            imageName = image->uri;
            return true;
        }

        bool createMaterialTexture(IRenderer& renderer, const tinygltf::Model& tinyModel, int textureIndex,
                                   TextureFormat format, const std::string& debugName,
                                   TextureHandle& textureHandle, std::string& textureName,
                                   uint32_t& textureMipLevels)
        {
            const tinygltf::Image* image = nullptr;
            if (!getTextureSourceImage(tinyModel, textureIndex, image, textureName))
            {
                return false;
            }

            std::vector<uint8_t> rgba = makeRgba8(*image);
            if (rgba.empty())
            {
                return false;
            }

            textureMipLevels = textureFullMipLevelCount(static_cast<uint32_t>(image->width),
                                                        static_cast<uint32_t>(image->height));
            textureHandle = renderer.createTexture({
                .width = static_cast<uint32_t>(image->width),
                .height = static_cast<uint32_t>(image->height),
                .format = format,
                .mipLevels = textureMipLevels,
                .generateMipmaps = textureMipLevels > 1,
                .sampled = true,
                .debugName = debugName,
            });
            return textureHandle.isValid() && renderer.uploadTexture(textureHandle, rgba.data(), rgba.size());
        }

        SamplerFilter gltfMagFilterToSamplerFilter(int filter)
        {
            return filter == TINYGLTF_TEXTURE_FILTER_NEAREST ? SamplerFilter::Nearest : SamplerFilter::Linear;
        }

        SamplerFilter gltfMinFilterToSamplerFilter(int filter)
        {
            switch (filter)
            {
                case TINYGLTF_TEXTURE_FILTER_NEAREST:
                case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
                case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
                    return SamplerFilter::Nearest;
                case TINYGLTF_TEXTURE_FILTER_LINEAR:
                case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
                case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
                default:
                    return SamplerFilter::Linear;
            }
        }

        SamplerMipFilter gltfMinFilterToMipFilter(int filter)
        {
            switch (filter)
            {
                case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
                case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
                    return SamplerMipFilter::Nearest;
                case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
                case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
                default:
                    return SamplerMipFilter::Linear;
            }
        }

        bool gltfMinFilterUsesMipmaps(int filter)
        {
            return filter < 0 || filter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST ||
                   filter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST ||
                   filter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR ||
                   filter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;
        }

        SamplerAddressMode gltfWrapToAddressMode(int wrap)
        {
            switch (wrap)
            {
                case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                    return SamplerAddressMode::ClampToEdge;
                case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
                    return SamplerAddressMode::MirroredRepeat;
                case TINYGLTF_TEXTURE_WRAP_REPEAT:
                default:
                    return SamplerAddressMode::Repeat;
            }
        }

        SamplerDesc createMaterialSamplerDesc(const tinygltf::Model& model, int textureIndex, uint32_t maxMipLevels,
                                              const std::string& debugName)
        {
            SamplerDesc desc{
                .addressModeU = SamplerAddressMode::Repeat,
                .addressModeV = SamplerAddressMode::Repeat,
                .debugName = debugName,
            };
            const uint32_t maxLod = maxMipLevels > 0 ? maxMipLevels - 1 : 0;

            if (textureIndex < 0 || textureIndex >= static_cast<int>(model.textures.size()))
            {
                desc.maxLod = static_cast<float>(maxLod);
                return desc;
            }

            const tinygltf::Texture& texture = model.textures[static_cast<size_t>(textureIndex)];
            if (texture.sampler < 0 || texture.sampler >= static_cast<int>(model.samplers.size()))
            {
                desc.maxLod = static_cast<float>(maxLod);
                return desc;
            }

            const tinygltf::Sampler& sampler = model.samplers[static_cast<size_t>(texture.sampler)];
            desc.magFilter = gltfMagFilterToSamplerFilter(sampler.magFilter);
            desc.minFilter = gltfMinFilterToSamplerFilter(sampler.minFilter);
            desc.mipFilter = gltfMinFilterToMipFilter(sampler.minFilter);
            desc.addressModeU = gltfWrapToAddressMode(sampler.wrapS);
            desc.addressModeV = gltfWrapToAddressMode(sampler.wrapT);
            desc.maxLod = gltfMinFilterUsesMipmaps(sampler.minFilter) ? static_cast<float>(maxLod) : 0.0f;
            return desc;
        }

        bool validateTriangleIndices(const std::vector<uint32_t>& indices, size_t vertexCount)
        {
            if (indices.size() % 3 != 0)
            {
                return false;
            }

            for (uint32_t index : indices)
            {
                if (index >= vertexCount)
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
            std::vector<glm::vec3> tangentSums(positions.size(), glm::vec3(0.0f));
            std::vector<glm::vec3> bitangentSums(positions.size(), glm::vec3(0.0f));

            for (size_t i = 0; i + 2 < indices.size(); i += 3)
            {
                const uint32_t i0 = indices[i + 0];
                const uint32_t i1 = indices[i + 1];
                const uint32_t i2 = indices[i + 2];

                const glm::vec3 edge1 = positions[i1] - positions[i0];
                const glm::vec3 edge2 = positions[i2] - positions[i0];
                const glm::vec2 deltaUv1 = uvs[i1] - uvs[i0];
                const glm::vec2 deltaUv2 = uvs[i2] - uvs[i0];
                const float determinant = deltaUv1.x * deltaUv2.y - deltaUv1.y * deltaUv2.x;
                if (std::abs(determinant) <= 0.000001f)
                {
                    continue;
                }

                const float reciprocal = 1.0f / determinant;
                const glm::vec3 tangent = (edge1 * deltaUv2.y - edge2 * deltaUv1.y) * reciprocal;
                const glm::vec3 bitangent = (edge2 * deltaUv1.x - edge1 * deltaUv2.x) * reciprocal;

                const float angle0 = cornerAngle(edge1, edge2);
                const float angle1 = cornerAngle(-edge1, positions[i2] - positions[i1]);
                const float angle2 = cornerAngle(-edge2, positions[i1] - positions[i2]);

                tangentSums[i0] += tangent * angle0;
                tangentSums[i1] += tangent * angle1;
                tangentSums[i2] += tangent * angle2;
                bitangentSums[i0] += bitangent * angle0;
                bitangentSums[i1] += bitangent * angle1;
                bitangentSums[i2] += bitangent * angle2;
            }

            tangents.resize(positions.size());
            for (size_t i = 0; i < positions.size(); ++i)
            {
                const glm::vec3 normal = normalizeOr(normals[i], glm::vec3(0.0f, 1.0f, 0.0f));
                glm::vec3 tangent = tangentSums[i] - normal * glm::dot(normal, tangentSums[i]);
                tangent = normalizeOr(tangent, fallbackTangentForNormal(normal));
                const float handedness = glm::dot(glm::cross(normal, tangent), bitangentSums[i]) < 0.0f ? -1.0f : 1.0f;
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
            factors.baseColor = glm::vec4(
                readFactor(material.pbrMetallicRoughness.baseColorFactor, 0, 1.0f),
                readFactor(material.pbrMetallicRoughness.baseColorFactor, 1, 1.0f),
                readFactor(material.pbrMetallicRoughness.baseColorFactor, 2, 1.0f),
                readFactor(material.pbrMetallicRoughness.baseColorFactor, 3, 1.0f));
            factors.emissive = glm::vec3(readFactor(material.emissiveFactor, 0, 0.0f),
                                         readFactor(material.emissiveFactor, 1, 0.0f),
                                         readFactor(material.emissiveFactor, 2, 0.0f));
            factors.metallic = static_cast<float>(material.pbrMetallicRoughness.metallicFactor);
            factors.roughness = static_cast<float>(material.pbrMetallicRoughness.roughnessFactor);
            factors.normalScale = static_cast<float>(material.normalTexture.scale);
            factors.occlusionStrength = static_cast<float>(material.occlusionTexture.strength);
            factors.alphaCutoff = static_cast<float>(material.alphaCutoff);
            if (material.alphaMode == "MASK")
            {
                factors.alphaMode = GltfAlphaMode::Mask;
            }
            else if (material.alphaMode == "BLEND")
            {
                factors.alphaMode = GltfAlphaMode::Blend;
            }
            else
            {
                factors.alphaMode = GltfAlphaMode::Opaque;
            }
            factors.doubleSided = material.doubleSided;
            return factors;
        }

    }  // namespace

    RendererResult<GltfLoadedModel> loadGltfModel(IRenderer& renderer, const GltfLoadDesc& desc)
    {
        GltfLoadedModel loadedModel;
        if (desc.path.empty())
        {
            return RendererResult<GltfLoadedModel>::failure(RendererErrorCode::ValidationFailed,
                                                            "glTF path is empty.");
        }

        tinygltf::TinyGLTF loader;
        tinygltf::Model tinyModel;
        std::string error;
        std::string warning;
        if (!loader.LoadASCIIFromFile(&tinyModel, &error, &warning, desc.path))
        {
            if (!warning.empty())
            {
                Logger::getInstance().warning("glTF loader warning: " + warning);
            }
            const std::string message = error.empty() ? "Failed to load glTF file: " + desc.path
                                                      : "Failed to load glTF file: " + error;
            return failLoad(renderer, loadedModel, RendererErrorCode::ValidationFailed, message);
        }
        if (!warning.empty())
        {
            Logger::getInstance().warning("glTF loader warning: " + warning);
        }

        if (tinyModel.meshes.empty() || tinyModel.meshes.front().primitives.empty())
        {
            return failLoad(renderer, loadedModel, RendererErrorCode::Unsupported,
                            "glTF file does not contain a mesh primitive.");
        }

        const tinygltf::Primitive& primitive = tinyModel.meshes.front().primitives.front();
        if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
        {
            return failLoad(renderer, loadedModel, RendererErrorCode::Unsupported,
                            "glTF loader currently supports only triangle-list primitives.");
        }

        const tinygltf::Accessor* positionAccessor = findAccessor(tinyModel, primitive, "POSITION");
        const tinygltf::Accessor* normalAccessor = findAccessor(tinyModel, primitive, "NORMAL");
        const tinygltf::Accessor* uvAccessor = findAccessor(tinyModel, primitive, "TEXCOORD_0");
        const tinygltf::Accessor* tangentAccessor = findAccessor(tinyModel, primitive, "TANGENT");
        if (!positionAccessor || !normalAccessor || !uvAccessor)
        {
            return failLoad(renderer, loadedModel, RendererErrorCode::Unsupported,
                            "glTF primitive is missing POSITION, NORMAL, or TEXCOORD_0.");
        }

        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> uvs;
        std::vector<uint32_t> indices;
        if (!readFloatVec3Accessor(tinyModel, *positionAccessor, positions) ||
            !readFloatVec3Accessor(tinyModel, *normalAccessor, normals) ||
            !readFloatVec2Accessor(tinyModel, *uvAccessor, uvs) || !readIndices(tinyModel, primitive, indices))
        {
            return failLoad(renderer, loadedModel, RendererErrorCode::Unsupported,
                            "glTF primitive contains an unsupported accessor format.");
        }

        if (positions.empty() || positions.size() != normals.size() || positions.size() != uvs.size() ||
            indices.empty() || !validateTriangleIndices(indices, positions.size()))
        {
            return failLoad(renderer, loadedModel, RendererErrorCode::ValidationFailed,
                            "glTF mesh attribute counts or triangle indices are invalid.");
        }

        std::vector<glm::vec4> tangents;
        if (tangentAccessor && !readFloatVec4Accessor(tinyModel, *tangentAccessor, tangents))
        {
            return failLoad(renderer, loadedModel, RendererErrorCode::Unsupported,
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

        loadedModel.vertexBuffer = renderer.createBuffer({
            .size = vertices.size() * sizeof(GltfVertex),
            .usage = BufferUsageKind::Vertex,
            .memoryAccess = MemoryAccess::CpuWrite,
            .debugName = makeDebugName(desc, "Vertex Buffer"),
        });
        loadedModel.indexBuffer = renderer.createBuffer({
            .size = indices.size() * sizeof(uint32_t),
            .usage = BufferUsageKind::Index,
            .memoryAccess = MemoryAccess::CpuWrite,
            .debugName = makeDebugName(desc, "Index Buffer"),
        });
        if (!loadedModel.vertexBuffer.isValid() || !loadedModel.indexBuffer.isValid() ||
            !renderer.uploadBuffer(loadedModel.vertexBuffer, vertices.data(), vertices.size() * sizeof(GltfVertex)) ||
            !renderer.uploadBuffer(loadedModel.indexBuffer, indices.data(), indices.size() * sizeof(uint32_t)))
        {
            return failLoad(renderer, loadedModel, RendererErrorCode::BackendFailure,
                            "Failed to create or upload glTF mesh buffers.");
        }

        loadedModel.indexFormat = IndexFormat::UInt32;
        loadedModel.indexCount = static_cast<uint32_t>(indices.size());
        loadedModel.transform = readFirstNodeTransform(tinyModel);

        if (primitive.material < 0 || primitive.material >= static_cast<int>(tinyModel.materials.size()))
        {
            return failLoad(renderer, loadedModel, RendererErrorCode::ValidationFailed,
                            "glTF primitive does not reference a valid material.");
        }

        const tinygltf::Material& material = tinyModel.materials[static_cast<size_t>(primitive.material)];
        loadedModel.materialFactors = readMaterialFactors(material);
        uint32_t maxMaterialMipLevels = 1;
        uint32_t baseColorMipLevels = 1;
        uint32_t metalRoughnessMipLevels = 1;
        uint32_t emissiveMipLevels = 1;
        uint32_t occlusionMipLevels = 1;
        uint32_t normalMipLevels = 1;
        const bool loadedAllTextures =
            createMaterialTexture(renderer, tinyModel, material.pbrMetallicRoughness.baseColorTexture.index,
                                  TextureFormat::RGBA8Srgb, makeDebugName(desc, "Base Color"),
                                  loadedModel.materialTextures.baseColor, loadedModel.textureNames.baseColor,
                                  baseColorMipLevels) &&
            createMaterialTexture(renderer, tinyModel, material.pbrMetallicRoughness.metallicRoughnessTexture.index,
                                  TextureFormat::RGBA8, makeDebugName(desc, "Metal Roughness"),
                                  loadedModel.materialTextures.metalRoughness,
                                  loadedModel.textureNames.metalRoughness, metalRoughnessMipLevels) &&
            createMaterialTexture(renderer, tinyModel, material.emissiveTexture.index, TextureFormat::RGBA8Srgb,
                                  makeDebugName(desc, "Emissive"), loadedModel.materialTextures.emissive,
                                  loadedModel.textureNames.emissive, emissiveMipLevels) &&
            createMaterialTexture(renderer, tinyModel, material.occlusionTexture.index,
                                  TextureFormat::RGBA8, makeDebugName(desc, "Occlusion"),
                                  loadedModel.materialTextures.occlusion, loadedModel.textureNames.occlusion,
                                  occlusionMipLevels) &&
            createMaterialTexture(renderer, tinyModel, material.normalTexture.index, TextureFormat::RGBA8,
                                  makeDebugName(desc, "Normal"), loadedModel.materialTextures.normal,
                                  loadedModel.textureNames.normal, normalMipLevels);
        if (!loadedAllTextures && desc.requireMaterialTextures)
        {
            return failLoad(renderer, loadedModel, RendererErrorCode::Unsupported,
                            "glTF material is missing a required PBR texture or uses an unsupported image format.");
        }
        maxMaterialMipLevels =
            std::max({baseColorMipLevels, metalRoughnessMipLevels, emissiveMipLevels, occlusionMipLevels,
                      normalMipLevels});

        loadedModel.materialSampler =
            renderer.createSampler(createMaterialSamplerDesc(tinyModel, material.pbrMetallicRoughness.baseColorTexture.index,
                                                            maxMaterialMipLevels,
                                                            makeDebugName(desc, "Material Sampler")));
        if (!loadedModel.materialSampler.isValid())
        {
            return failLoad(renderer, loadedModel, RendererErrorCode::BackendFailure,
                            "Failed to create glTF material sampler.");
        }

        return RendererResult<GltfLoadedModel>::success(std::move(loadedModel));
    }

    void destroyGltfModel(IRenderer& renderer, GltfLoadedModel& model)
    {
        if (model.materialTextures.normal.isValid())
        {
            renderer.destroyTexture(model.materialTextures.normal);
            model.materialTextures.normal = {};
        }
        if (model.materialTextures.occlusion.isValid())
        {
            renderer.destroyTexture(model.materialTextures.occlusion);
            model.materialTextures.occlusion = {};
        }
        if (model.materialTextures.emissive.isValid())
        {
            renderer.destroyTexture(model.materialTextures.emissive);
            model.materialTextures.emissive = {};
        }
        if (model.materialTextures.metalRoughness.isValid())
        {
            renderer.destroyTexture(model.materialTextures.metalRoughness);
            model.materialTextures.metalRoughness = {};
        }
        if (model.materialTextures.baseColor.isValid())
        {
            renderer.destroyTexture(model.materialTextures.baseColor);
            model.materialTextures.baseColor = {};
        }
        if (model.materialSampler.isValid())
        {
            renderer.destroySampler(model.materialSampler);
            model.materialSampler = {};
        }
        if (model.indexBuffer.isValid())
        {
            renderer.destroyBuffer(model.indexBuffer);
            model.indexBuffer = {};
        }
        if (model.vertexBuffer.isValid())
        {
            renderer.destroyBuffer(model.vertexBuffer);
            model.vertexBuffer = {};
        }

        model.indexCount = 0;
        model.transform = glm::mat4(1.0f);
        model.textureNames = {};
        model.materialFactors = {};
    }

}  // namespace kera
