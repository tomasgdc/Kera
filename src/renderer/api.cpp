// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/abi.h"
#include "kera/renderer/backend/vulkan/vulkan_renderer.h"
#include "kera/renderer/gltf_loader.h"
#include "kera/renderer/interfaces.h"
#include "kera/renderer/ktx_loader.h"
#include "kera/renderer/reflection_contracts.h"
#include "kera/utilities/logger.h"

#include <memory>
#include <string>
#include <vector>

struct KeraRenderer
{
    std::unique_ptr<kera::IRenderer> renderer;
    mutable std::vector<KeraRendererValidationIssue> validationIssues;
    mutable std::vector<std::string> validationMessages;
    mutable std::vector<std::string> validationNames;
};

namespace
{
    std::string toString(KeraStringView view)
    {
        return view.data ? std::string(view.data, view.size) : std::string{};
    }

    KeraStringView toView(const std::string& value)
    {
        return {value.data(), value.size()};
    }

    template <typename HandleT>
    HandleT fromKera(KeraHandle handle)
    {
        return HandleT{handle.index, handle.generation};
    }

    template <typename HandleT>
    KeraHandle toKera(HandleT handle)
    {
        return {handle.m_index, handle.m_generation};
    }

    kera::BufferUsageKind fromKera(KeraBufferUsageKind value)
    {
        return static_cast<kera::BufferUsageKind>(value);
    }

    kera::MemoryAccess fromKera(KeraMemoryAccess value)
    {
        return static_cast<kera::MemoryAccess>(value);
    }

    kera::IndexFormat fromKera(KeraIndexFormat value)
    {
        return static_cast<kera::IndexFormat>(value);
    }

    kera::VertexFormat fromKera(KeraVertexFormat value)
    {
        return static_cast<kera::VertexFormat>(value);
    }

    kera::VertexInputRate fromKera(KeraVertexInputRate value)
    {
        return static_cast<kera::VertexInputRate>(value);
    }

    kera::DescriptorType fromKera(KeraDescriptorType value)
    {
        return static_cast<kera::DescriptorType>(value);
    }

    kera::TextureFormat fromKera(KeraTextureFormat value)
    {
        return static_cast<kera::TextureFormat>(value);
    }

    kera::SamplerFilter fromKera(KeraSamplerFilter value)
    {
        return static_cast<kera::SamplerFilter>(value);
    }

    kera::SamplerMipFilter fromKera(KeraSamplerMipFilter value)
    {
        return static_cast<kera::SamplerMipFilter>(value);
    }

    kera::SamplerAddressMode fromKera(KeraSamplerAddressMode value)
    {
        return static_cast<kera::SamplerAddressMode>(value);
    }

    kera::PrimitiveTopologyKind fromKera(KeraPrimitiveTopologyKind value)
    {
        return static_cast<kera::PrimitiveTopologyKind>(value);
    }

    kera::CullModeKind fromKera(KeraCullModeKind value)
    {
        return static_cast<kera::CullModeKind>(value);
    }

    kera::FrontFaceKind fromKera(KeraFrontFaceKind value)
    {
        return static_cast<kera::FrontFaceKind>(value);
    }

    kera::BlendModeKind fromKera(KeraBlendModeKind value)
    {
        return static_cast<kera::BlendModeKind>(value);
    }

    KeraIndexFormat toKera(kera::IndexFormat value)
    {
        return static_cast<KeraIndexFormat>(value);
    }

    KeraGltfAlphaMode toKera(kera::GltfAlphaMode value)
    {
        return static_cast<KeraGltfAlphaMode>(value);
    }

    kera::GltfAlphaMode fromKera(KeraGltfAlphaMode value)
    {
        return static_cast<kera::GltfAlphaMode>(value);
    }

    kera::Extent2D fromKera(KeraExtent2D value)
    {
        return {value.width, value.height};
    }

    KeraExtent2D toKera(kera::Extent2D value)
    {
        return {value.width, value.height};
    }

    kera::ClearColorValue fromKera(KeraClearColorValue value)
    {
        return {value.r, value.g, value.b, value.a};
    }

    kera::BufferDesc fromKera(const KeraBufferDesc& desc)
    {
        return {
            .size = desc.size,
            .usage = fromKera(desc.usage),
            .memoryAccess = fromKera(desc.memoryAccess),
            .debugName = toString(desc.debugName),
        };
    }

    kera::TextureDesc fromKera(const KeraTextureDesc& desc)
    {
        return {
            .width = desc.width,
            .height = desc.height,
            .format = fromKera(desc.format),
            .mipLevels = desc.mipLevels == 0 ? 1u : desc.mipLevels,
            .generateMipmaps = desc.generateMipmaps != 0,
            .renderTarget = desc.renderTarget != 0,
            .sampled = desc.sampled != 0,
            .depthStencil = desc.depthStencil != 0,
            .debugName = toString(desc.debugName),
        };
    }

    kera::SamplerDesc fromKera(const KeraSamplerDesc& desc)
    {
        return {
            .minFilter = fromKera(desc.minFilter),
            .magFilter = fromKera(desc.magFilter),
            .mipFilter = fromKera(desc.mipFilter),
            .addressModeU = fromKera(desc.addressModeU),
            .addressModeV = fromKera(desc.addressModeV),
            .minLod = desc.minLod,
            .maxLod = desc.maxLod,
            .maxAnisotropy = desc.maxAnisotropy == 0.0f ? 1.0f : desc.maxAnisotropy,
            .debugName = toString(desc.debugName),
        };
    }

    kera::RenderTargetDesc fromKera(const KeraRenderTargetDesc& desc)
    {
        return {
            .colorTexture = fromKera<kera::TextureHandle>(desc.colorTexture),
            .depthTexture = fromKera<kera::TextureHandle>(desc.depthTexture),
            .debugName = toString(desc.debugName),
        };
    }

    std::vector<kera::ReflectedVertexBindingDesc> fromKeraVertexBindings(const KeraVertexInputLayout& layout)
    {
        std::vector<kera::ReflectedVertexBindingDesc> bindings;
        bindings.reserve(layout.bindingCount);
        for (size_t i = 0; i < layout.bindingCount; ++i)
        {
            const KeraVertexInputBindingDesc& binding = layout.bindings[i];
            bindings.push_back({
                .binding = binding.binding,
                .stride = binding.stride,
                .inputRate = fromKera(binding.inputRate),
            });
        }
        return bindings;
    }

    std::vector<kera::ReflectedVertexFieldDesc> fromKeraVertexFields(const KeraVertexInputLayout& layout)
    {
        std::vector<kera::ReflectedVertexFieldDesc> fields;
        fields.reserve(layout.fieldCount);
        for (size_t i = 0; i < layout.fieldCount; ++i)
        {
            const KeraVertexInputFieldDesc& field = layout.fields[i];
            fields.push_back({
                .parameterName = toString(field.parameterName),
                .fieldName = toString(field.fieldName),
                .binding = field.binding,
                .offset = field.offset,
                .format = fromKera(field.format),
            });
        }
        return fields;
    }

    kera::GraphicsPipelineCreateDesc fromKera(const KeraGraphicsPipelineCreateDesc& desc)
    {
        return {
            .shaderProgram = fromKera<kera::ShaderProgramHandle>(desc.shaderProgram),
            .renderTarget = fromKera<kera::RenderTargetHandle>(desc.renderTarget),
            .topology = fromKera(desc.topology),
            .cullMode = fromKera(desc.cullMode),
            .frontFace = fromKera(desc.frontFace),
            .blendMode = fromKera(desc.blendMode),
            .depthTest = desc.depthTest != 0,
            .depthWrite = desc.depthWrite != 0,
            .vertexBindings = fromKeraVertexBindings(desc.vertexInput),
            .vertexFields = fromKeraVertexFields(desc.vertexInput),
            .debugName = toString(desc.debugName),
        };
    }

    KeraRendererValidationReport storeValidationReport(const KeraRenderer* renderer,
                                                       const kera::RendererValidationReport& report)
    {
        renderer->validationIssues.clear();
        renderer->validationMessages.clear();
        renderer->validationNames.clear();

        renderer->validationIssues.reserve(report.issues.size());
        renderer->validationMessages.reserve(report.issues.size());
        renderer->validationNames.reserve(report.issues.size());

        for (const kera::RendererValidationIssue& issue : report.issues)
        {
            renderer->validationMessages.push_back(issue.message);
            renderer->validationNames.push_back(issue.name);
            renderer->validationIssues.push_back({
                .message = toView(renderer->validationMessages.back()),
                .name = toView(renderer->validationNames.back()),
                .set = issue.set,
                .binding = issue.binding,
            });
        }

        return {
            .issues = renderer->validationIssues.data(),
            .issueCount = renderer->validationIssues.size(),
        };
    }

    KeraRendererStats toKera(const kera::RendererStats& stats)
    {
        return {
            .resources =
                {
                    .shaderModules = stats.resources.shaderModules,
                    .shaderPrograms = stats.resources.shaderPrograms,
                    .buffers = stats.resources.buffers,
                    .textures = stats.resources.textures,
                    .samplers = stats.resources.samplers,
                    .renderTargets = stats.resources.renderTargets,
                    .graphicsPipelines = stats.resources.graphicsPipelines,
                    .descriptorSets = stats.resources.descriptorSets,
                    .frames = stats.resources.frames,
                },
            .drawCallsThisFrame = stats.drawCallsThisFrame,
            .pipelinesBoundThisFrame = stats.pipelinesBoundThisFrame,
            .descriptorSetsBoundThisFrame = stats.descriptorSetsBoundThisFrame,
            .vertexBuffersBoundThisFrame = stats.vertexBuffersBoundThisFrame,
            .indexBuffersBoundThisFrame = stats.indexBuffersBoundThisFrame,
            .bufferUploadsThisFrame = stats.bufferUploadsThisFrame,
            .textureUploadsThisFrame = stats.textureUploadsThisFrame,
            .validationIssuesThisFrame = stats.validationIssuesThisFrame,
            .frameIndex = stats.frameIndex,
        };
    }

    KeraGltfLoadedModel toKera(const kera::GltfLoadedModel& model)
    {
        KeraGltfLoadedModel out{};
        out.vertexBuffer = toKera(model.vertexBuffer);
        out.indexBuffer = toKera(model.indexBuffer);
        out.materialTextures.baseColor = toKera(model.materialTextures.baseColor);
        out.materialTextures.metalRoughness = toKera(model.materialTextures.metalRoughness);
        out.materialTextures.emissive = toKera(model.materialTextures.emissive);
        out.materialTextures.occlusion = toKera(model.materialTextures.occlusion);
        out.materialTextures.normal = toKera(model.materialTextures.normal);
        out.materialSampler = toKera(model.materialSampler);
        out.indexFormat = toKera(model.indexFormat);
        out.indexCount = model.indexCount;
        const float* transform = &model.transform[0][0];
        for (size_t i = 0; i < 16; ++i)
        {
            out.transform[i] = transform[i];
        }
        out.materialFactors.baseColor[0] = model.materialFactors.baseColor.r;
        out.materialFactors.baseColor[1] = model.materialFactors.baseColor.g;
        out.materialFactors.baseColor[2] = model.materialFactors.baseColor.b;
        out.materialFactors.baseColor[3] = model.materialFactors.baseColor.a;
        out.materialFactors.emissive[0] = model.materialFactors.emissive.x;
        out.materialFactors.emissive[1] = model.materialFactors.emissive.y;
        out.materialFactors.emissive[2] = model.materialFactors.emissive.z;
        out.materialFactors.metallic = model.materialFactors.metallic;
        out.materialFactors.roughness = model.materialFactors.roughness;
        out.materialFactors.normalScale = model.materialFactors.normalScale;
        out.materialFactors.occlusionStrength = model.materialFactors.occlusionStrength;
        out.materialFactors.alphaCutoff = model.materialFactors.alphaCutoff;
        out.materialFactors.alphaMode = toKera(model.materialFactors.alphaMode);
        out.materialFactors.doubleSided = model.materialFactors.doubleSided ? 1u : 0u;
        return out;
    }

    kera::GltfLoadedModel fromKeraModel(const KeraGltfLoadedModel& model)
    {
        kera::GltfLoadedModel out{};
        out.vertexBuffer = fromKera<kera::BufferHandle>(model.vertexBuffer);
        out.indexBuffer = fromKera<kera::BufferHandle>(model.indexBuffer);
        out.materialTextures.baseColor = fromKera<kera::TextureHandle>(model.materialTextures.baseColor);
        out.materialTextures.metalRoughness = fromKera<kera::TextureHandle>(model.materialTextures.metalRoughness);
        out.materialTextures.emissive = fromKera<kera::TextureHandle>(model.materialTextures.emissive);
        out.materialTextures.occlusion = fromKera<kera::TextureHandle>(model.materialTextures.occlusion);
        out.materialTextures.normal = fromKera<kera::TextureHandle>(model.materialTextures.normal);
        out.materialSampler = fromKera<kera::SamplerHandle>(model.materialSampler);
        return out;
    }

    KeraIblSphericalHarmonics toKera(const kera::IblSphericalHarmonics& value)
    {
        KeraIblSphericalHarmonics result{};
        std::memcpy(result.coefficients, value.coefficients, sizeof(result.coefficients));
        return result;
    }

    KeraIblEnvironment toKera(const kera::IblEnvironment& value)
    {
        return {
            .iblTexture = toKera(value.iblTexture),
            .skyboxTexture = toKera(value.skyboxTexture),
            .sampler = toKera(value.sampler),
            .sphericalHarmonics = toKera(value.sphericalHarmonics),
            .iblMipLevels = value.iblMiplevels,
            .skyboxMipLevels = value.skyboxMiplevels,
        };
    }

    kera::IblSphericalHarmonics fromKera(const KeraIblSphericalHarmonics& value)
    {
        kera::IblSphericalHarmonics result{};
        std::memcpy(result.coefficients, value.coefficients, sizeof(result.coefficients));
        return result;
    }

    kera::IblEnvironment fromKera(const KeraIblEnvironment& value)
    {
        return {
            .iblTexture = fromKera<kera::TextureHandle>(value.iblTexture),
            .skyboxTexture = fromKera<kera::TextureHandle>(value.skyboxTexture),
            .sampler = fromKera<kera::SamplerHandle>(value.sampler),
            .sphericalHarmonics = fromKera(value.sphericalHarmonics),
            .iblMiplevels = value.iblMipLevels,
            .skyboxMiplevels = value.skyboxMipLevels,
        };
    }

    KeraUniformRingBufferInfo toKera(const kera::UniformRingBufferInfo& value)
    {
        return {
            .elementSize = value.elementSize,
            .slotStride = value.slotStride,
            .slotCount = value.slotCount,
        };
    }

    kera::UniformRingBufferInfo fromKera(const KeraUniformRingBufferInfo& value)
    {
        return {
            .elementSize = value.elementSize,
            .slotStride = value.slotStride,
            .slotCount = value.slotCount,
        };
    }

    KeraRenderer* createRenderer(const KeraRendererCreateDesc* desc)
    {
        if (!desc || !desc->sdlWindow || desc->backend != KERA_GRAPHICS_BACKEND_VULKAN)
        {
            return nullptr;
        }

        auto renderer = std::make_unique<kera::VulkanRenderer>();
        if (!renderer->initialize(static_cast<SDL_Window*>(desc->sdlWindow), desc->width, desc->height))
        {
            return nullptr;
        }

        KeraRenderer* wrapper = new KeraRenderer{};
        wrapper->renderer = std::move(renderer);
        return wrapper;
    }

    void destroy(KeraRenderer* renderer)
    {
        if (!renderer)
        {
            return;
        }
        if (renderer->renderer)
        {
            renderer->renderer->shutdown();
        }
        delete renderer;
    }

    void shutdown(KeraRenderer* renderer)
    {
        if (renderer && renderer->renderer) renderer->renderer->shutdown();
    }

    KeraGraphicsBackend getBackend(const KeraRenderer*)
    {
        return KERA_GRAPHICS_BACKEND_VULKAN;
    }

    KeraExtent2D getDrawableExtent(const KeraRenderer* renderer)
    {
        return renderer && renderer->renderer ? toKera(renderer->renderer->getDrawableExtent()) : KeraExtent2D{};
    }

    KeraRendererStats getStats(const KeraRenderer* renderer)
    {
        return renderer && renderer->renderer ? toKera(renderer->renderer->getStats()) : KeraRendererStats{};
    }

    int resize(KeraRenderer* renderer, KeraExtent2D extent)
    {
        return renderer && renderer->renderer && renderer->renderer->resize(fromKera(extent)) ? 1 : 0;
    }

    int initializeUi(KeraRenderer* renderer)
    {
        return renderer && renderer->renderer && renderer->renderer->initializeUi() ? 1 : 0;
    }

    void shutdownUi(KeraRenderer* renderer)
    {
        if (renderer && renderer->renderer) renderer->renderer->shutdownUi();
    }

    void handleUiEvent(KeraRenderer* renderer, const void* sdlEvent)
    {
        if (renderer && renderer->renderer && sdlEvent)
        {
            renderer->renderer->handleUiEvent(*static_cast<const SDL_Event*>(sdlEvent));
        }
    }

    void beginUi(KeraRenderer* renderer)
    {
        if (renderer && renderer->renderer) renderer->renderer->beginUi();
    }

    void endUi(KeraRenderer* renderer)
    {
        if (renderer && renderer->renderer) renderer->renderer->endUi();
    }

    void renderUi(KeraRenderer* renderer, KeraFrameHandle frame)
    {
        if (renderer && renderer->renderer) renderer->renderer->renderUi(fromKera<kera::FrameHandle>(frame));
    }

    int isUiAvailable(const KeraRenderer* renderer)
    {
        return renderer && renderer->renderer && renderer->renderer->isUiAvailable() ? 1 : 0;
    }

    KeraShaderProgramHandle createGraphicsShaderProgram(KeraRenderer* renderer,
                                                        const KeraGraphicsShaderProgramDesc* desc)
    {
        if (!renderer || !renderer->renderer || !desc) return {};
        return toKera(renderer->renderer->createGraphicsShaderProgram({
            .path = toString(desc->path),
            .vertexEntryPoint = toString(desc->vertexEntryPoint),
            .fragmentEntryPoint = toString(desc->fragmentEntryPoint),
            .source = static_cast<kera::ShaderSourceKind>(desc->source),
            .debugName = toString(desc->debugName),
        }));
    }

    int destroyShaderProgram(KeraRenderer* renderer, KeraShaderProgramHandle program)
    {
        return renderer && renderer->renderer &&
                       renderer->renderer->destroyShaderProgram(fromKera<kera::ShaderProgramHandle>(program))
                   ? 1
                   : 0;
    }

    KeraBufferHandle createBuffer(KeraRenderer* renderer, const KeraBufferDesc* desc)
    {
        return renderer && renderer->renderer && desc ? toKera(renderer->renderer->createBuffer(fromKera(*desc)))
                                                      : KeraBufferHandle{};
    }

    KeraBufferHandle createUniformRingBuffer(KeraRenderer* renderer, size_t elementSize, uint32_t slotCount)
    {
        return renderer && renderer->renderer
                   ? toKera(renderer->renderer->createUniformRingBuffer(elementSize, slotCount))
                   : KeraBufferHandle{};
    }

    int destroyBuffer(KeraRenderer* renderer, KeraBufferHandle buffer)
    {
        return renderer && renderer->renderer && renderer->renderer->destroyBuffer(fromKera<kera::BufferHandle>(buffer))
                   ? 1
                   : 0;
    }

    int mapBuffer(KeraRenderer* renderer, KeraBufferHandle buffer, void** data)
    {
        return renderer && renderer->renderer &&
                       renderer->renderer->mapBuffer(fromKera<kera::BufferHandle>(buffer), data)
                   ? 1
                   : 0;
    }

    void unmapBuffer(KeraRenderer* renderer, KeraBufferHandle buffer)
    {
        if (renderer && renderer->renderer) renderer->renderer->unmapBuffer(fromKera<kera::BufferHandle>(buffer));
    }

    int uploadBuffer(KeraRenderer* renderer, KeraBufferHandle buffer, const void* data, size_t size, size_t offset)
    {
        return renderer && renderer->renderer &&
                       renderer->renderer->uploadBuffer(fromKera<kera::BufferHandle>(buffer), data, size, offset)
                   ? 1
                   : 0;
    }

    int uploadUniformRingBuffer(KeraRenderer* renderer, KeraBufferHandle buffer, KeraFrameHandle frame,
                                const void* data, size_t size)
    {
        return renderer && renderer->renderer &&
                       renderer->renderer->uploadUniformRingBuffer(fromKera<kera::BufferHandle>(buffer),
                                                                   fromKera<kera::FrameHandle>(frame), data, size)
                   ? 1
                   : 0;
    }

    KeraUniformRingBufferInfo getUniformRingBufferInfo(KeraRenderer* renderer, KeraBufferHandle buffer)
    {
        return renderer && renderer->renderer
                   ? toKera(renderer->renderer->getUniformRingBufferInfo(fromKera<kera::BufferHandle>(buffer)))
                   : KeraUniformRingBufferInfo{};
    }

    uint32_t getUniformRingBufferSlot(KeraRenderer* renderer, KeraBufferHandle buffer, KeraFrameHandle frame)
    {
        return renderer && renderer->renderer
                   ? renderer->renderer->getUniformRingBufferSlot(fromKera<kera::BufferHandle>(buffer),
                                                                  fromKera<kera::FrameHandle>(frame))
                   : 0;
    }

    size_t getUniformRingBufferSlotOffset(KeraRenderer* renderer, KeraBufferHandle buffer, uint32_t slot)
    {
        return renderer && renderer->renderer
                   ? renderer->renderer->getUniformRingBufferSlotOffset(fromKera<kera::BufferHandle>(buffer), slot)
                   : 0;
    }

    KeraTextureHandle createTexture(KeraRenderer* renderer, const KeraTextureDesc* desc)
    {
        return renderer && renderer->renderer && desc ? toKera(renderer->renderer->createTexture(fromKera(*desc)))
                                                      : KeraTextureHandle{};
    }

    int uploadTexture(KeraRenderer* renderer, KeraTextureHandle texture, const void* data, size_t size)
    {
        return renderer && renderer->renderer &&
                       renderer->renderer->uploadTexture(fromKera<kera::TextureHandle>(texture), data, size)
                   ? 1
                   : 0;
    }

    int destroyTexture(KeraRenderer* renderer, KeraTextureHandle texture)
    {
        return renderer && renderer->renderer &&
                       renderer->renderer->destroyTexture(fromKera<kera::TextureHandle>(texture))
                   ? 1
                   : 0;
    }

    KeraSamplerHandle createSampler(KeraRenderer* renderer, const KeraSamplerDesc* desc)
    {
        return renderer && renderer->renderer && desc ? toKera(renderer->renderer->createSampler(fromKera(*desc)))
                                                      : KeraSamplerHandle{};
    }

    int destroySampler(KeraRenderer* renderer, KeraSamplerHandle sampler)
    {
        return renderer && renderer->renderer &&
                       renderer->renderer->destroySampler(fromKera<kera::SamplerHandle>(sampler))
                   ? 1
                   : 0;
    }

    KeraRenderTargetHandle createRenderTarget(KeraRenderer* renderer, const KeraRenderTargetDesc* desc)
    {
        return renderer && renderer->renderer && desc ? toKera(renderer->renderer->createRenderTarget(fromKera(*desc)))
                                                      : KeraRenderTargetHandle{};
    }

    int destroyRenderTarget(KeraRenderer* renderer, KeraRenderTargetHandle target)
    {
        return renderer && renderer->renderer &&
                       renderer->renderer->destroyRenderTarget(fromKera<kera::RenderTargetHandle>(target))
                   ? 1
                   : 0;
    }

    KeraGraphicsPipelineHandle createGraphicsPipeline(KeraRenderer* renderer,
                                                      const KeraGraphicsPipelineCreateDesc* desc)
    {
        if (!renderer || !renderer->renderer || !desc)
        {
            return {};
        }

        return toKera(renderer->renderer->createGraphicsPipeline(fromKera(*desc)));
    }

    KeraRendererValidationReport validateVertexInputLayout(const KeraRenderer* renderer,
                                                           KeraShaderProgramHandle shaderProgram,
                                                           KeraVertexInputLayout vertexInput)
    {
        if (!renderer || !renderer->renderer)
        {
            return {};
        }

        kera::RendererValidationReport report;
        const kera::SlangReflectionMetadata* reflection =
            renderer->renderer->getShaderProgramReflection(fromKera<kera::ShaderProgramHandle>(shaderProgram));
        if (!reflection)
        {
            report.addIssue("Shader program reflection is missing while validating vertex input layout.");
            return storeValidationReport(renderer, report);
        }

        const std::vector<kera::ReflectedVertexBindingDesc> bindings = fromKeraVertexBindings(vertexInput);
        const std::vector<kera::ReflectedVertexFieldDesc> fields = fromKeraVertexFields(vertexInput);
        const kera::VertexInputLayoutBuildResult result =
            kera::buildValidatedVertexInputLayout(*reflection, {
                                                                   .debugName = {},
                                                                   .bindings = bindings,
                                                                   .fields = fields,
                                                               });
        if (!result)
        {
            return storeValidationReport(renderer, result.report());
        }

        return storeValidationReport(renderer, report);
    }

    int destroyGraphicsPipeline(KeraRenderer* renderer, KeraGraphicsPipelineHandle pipeline)
    {
        return renderer && renderer->renderer &&
                       renderer->renderer->destroyGraphicsPipeline(fromKera<kera::GraphicsPipelineHandle>(pipeline))
                   ? 1
                   : 0;
    }

    KeraDescriptorSetHandle createDescriptorSet(KeraRenderer* renderer, KeraGraphicsPipelineHandle pipeline,
                                                uint32_t set)
    {
        return renderer && renderer->renderer ? toKera(renderer->renderer->createDescriptorSet(
                                                    fromKera<kera::GraphicsPipelineHandle>(pipeline), set))
                                              : KeraDescriptorSetHandle{};
    }

    int destroyDescriptorSet(KeraRenderer* renderer, KeraDescriptorSetHandle set)
    {
        return renderer && renderer->renderer &&
                       renderer->renderer->destroyDescriptorSet(fromKera<kera::DescriptorSetHandle>(set))
                   ? 1
                   : 0;
    }

    int updateDescriptorBuffer(KeraRenderer* renderer, KeraDescriptorSetHandle set, KeraStringView name,
                               KeraBufferHandle buffer, size_t offset, size_t range)
    {
        return renderer && renderer->renderer &&
                       renderer->renderer->updateDescriptorSet(fromKera<kera::DescriptorSetHandle>(set), toString(name),
                                                               fromKera<kera::BufferHandle>(buffer), offset, range)
                   ? 1
                   : 0;
    }

    int updateDescriptorTexture(KeraRenderer* renderer, KeraDescriptorSetHandle set, KeraStringView name,
                                KeraTextureHandle texture)
    {
        return renderer && renderer->renderer &&
                       renderer->renderer->updateDescriptorSet(fromKera<kera::DescriptorSetHandle>(set), toString(name),
                                                               fromKera<kera::TextureHandle>(texture))
                   ? 1
                   : 0;
    }

    int updateDescriptorSampler(KeraRenderer* renderer, KeraDescriptorSetHandle set, KeraStringView name,
                                KeraSamplerHandle sampler)
    {
        return renderer && renderer->renderer &&
                       renderer->renderer->updateDescriptorSet(fromKera<kera::DescriptorSetHandle>(set), toString(name),
                                                               fromKera<kera::SamplerHandle>(sampler))
                   ? 1
                   : 0;
    }

    KeraRendererValidationReport validateDescriptorSet(const KeraRenderer* renderer, KeraDescriptorSetHandle set)
    {
        if (!renderer || !renderer->renderer)
        {
            return {};
        }

        const kera::RendererValidationReport report =
            renderer->renderer->validateDescriptorSetDetailed(fromKera<kera::DescriptorSetHandle>(set));

        return storeValidationReport(renderer, report);
    }

    int setDebugName(KeraRenderer*, KeraHandle, KeraStringView)
    {
        return 0;
    }

    KeraFrameHandle beginFrame(KeraRenderer* renderer)
    {
        return renderer && renderer->renderer ? toKera(renderer->renderer->beginFrame()) : KeraFrameHandle{};
    }

    void beginRenderPass(KeraRenderer* renderer, KeraFrameHandle frame, const KeraRenderPassDesc* desc)
    {
        if (renderer && renderer->renderer && desc)
        {
            renderer->renderer->beginRenderPass(
                fromKera<kera::FrameHandle>(frame),
                {.clearColor = fromKera(desc->clearColor), .clearDepth = desc->clearDepth});
        }
    }

    void beginRenderPassTarget(KeraRenderer* renderer, KeraFrameHandle frame, KeraRenderTargetHandle target,
                               const KeraRenderPassDesc* desc)
    {
        if (renderer && renderer->renderer && desc)
        {
            renderer->renderer->beginRenderPass(
                fromKera<kera::FrameHandle>(frame), fromKera<kera::RenderTargetHandle>(target),
                {.clearColor = fromKera(desc->clearColor), .clearDepth = desc->clearDepth});
        }
    }

    void endRenderPass(KeraRenderer* renderer, KeraFrameHandle frame)
    {
        if (renderer && renderer->renderer) renderer->renderer->endRenderPass(fromKera<kera::FrameHandle>(frame));
    }

    void bindPipeline(KeraRenderer* renderer, KeraFrameHandle frame, KeraGraphicsPipelineHandle pipeline)
    {
        if (renderer && renderer->renderer)
        {
            renderer->renderer->bindPipeline(fromKera<kera::FrameHandle>(frame),
                                             fromKera<kera::GraphicsPipelineHandle>(pipeline));
        }
    }

    void bindVertexBuffer(KeraRenderer* renderer, KeraFrameHandle frame, uint32_t slot, KeraBufferHandle buffer,
                          size_t offset)
    {
        if (renderer && renderer->renderer)
        {
            renderer->renderer->bindVertexBuffer(fromKera<kera::FrameHandle>(frame), slot,
                                                 fromKera<kera::BufferHandle>(buffer), offset);
        }
    }

    void bindIndexBuffer(KeraRenderer* renderer, KeraFrameHandle frame, KeraBufferHandle buffer, KeraIndexFormat format,
                         size_t offset)
    {
        if (renderer && renderer->renderer)
        {
            renderer->renderer->bindIndexBuffer(fromKera<kera::FrameHandle>(frame),
                                                fromKera<kera::BufferHandle>(buffer), fromKera(format), offset);
        }
    }

    void bindDescriptorSet(KeraRenderer* renderer, KeraFrameHandle frame, KeraGraphicsPipelineHandle pipeline,
                           uint32_t setIndex, KeraDescriptorSetHandle descriptorSet)
    {
        if (renderer && renderer->renderer)
        {
            renderer->renderer->bindDescriptorSet(fromKera<kera::FrameHandle>(frame),
                                                  fromKera<kera::GraphicsPipelineHandle>(pipeline), setIndex,
                                                  fromKera<kera::DescriptorSetHandle>(descriptorSet));
        }
    }

    void drawIndexed(KeraRenderer* renderer, KeraFrameHandle frame, uint32_t indexCount, uint32_t instanceCount)
    {
        if (renderer && renderer->renderer)
        {
            renderer->renderer->drawIndexed(fromKera<kera::FrameHandle>(frame), indexCount, instanceCount);
        }
    }

    int endFrame(KeraRenderer* renderer, KeraFrameHandle frame)
    {
        return renderer && renderer->renderer && renderer->renderer->endFrame(fromKera<kera::FrameHandle>(frame)) ? 1
                                                                                                                  : 0;
    }

    int loadGltfModel(KeraRenderer* renderer, const KeraGltfLoadDesc* desc, KeraGltfLoadedModel* outModel)
    {
        if (!renderer || !renderer->renderer || !desc || !outModel)
        {
            return 0;
        }

        kera::GltfLoadDesc loadDesc{
            .path = toString(desc->path),
            .debugName = toString(desc->debugName),
            .requireMaterialTextures = desc->requireMaterialTextures != 0,
        };
        kera::RendererResult<kera::GltfLoadedModel> result = kera::loadGltfModel(*renderer->renderer, loadDesc);
        if (!result)
        {
            kera::Logger::getInstance().error("glTF load failed: " + result.errorMessage());
            return 0;
        }

        *outModel = toKera(result.value());
        return 1;
    }

    void destroyGltfModel(KeraRenderer* renderer, KeraGltfLoadedModel* model)
    {
        if (!renderer || !renderer->renderer || !model)
        {
            return;
        }
        kera::GltfLoadedModel internalModel = fromKeraModel(*model);
        kera::destroyGltfModel(*renderer->renderer, internalModel);
        *model = {};
    }

    int loadIblEnvironment(KeraRenderer* renderer, const KeraIblEnvironmentLoadDesc* desc,
                           KeraIblEnvironment* outEnvironment)
    {
        if (!renderer || !renderer->renderer || !desc || !outEnvironment)
        {
            return 0;
        }

        kera::IblEnvironmentLoadDesc loadDesc{.iblKtxPath = toString(desc->iblKtxPath),
                                              .skyboxKtxPath = toString(desc->skyboxKtxPath),
                                              .sphericalHarmonicsPath = toString(desc->sphericalHarmonicsPath),
                                              .debugName = toString(desc->debugName)};

        kera::RendererResult<kera::IblEnvironment> result = kera::loadIblEnvironment(*renderer->renderer, loadDesc);
        if (!result)
        {
            kera::Logger::getInstance().error("IBL environment load failed: " + result.errorMessage());
            return 0;
        }

        *outEnvironment = toKera(result.value());
        return 1;
    }

    void destroyIblEnvironment(KeraRenderer* renderer, KeraIblEnvironment* environment)
    {
        if (!renderer || !renderer->renderer || !environment)
        {
            return;
        }

        kera::IblEnvironment internalEnv = fromKera(*environment);
        kera::destroyIblEnvironment(*renderer->renderer, internalEnv);
        *environment = {};
    }

    const KeraRendererApiV1 g_api{
        .abiVersion = KERA_RENDERER_ABI_VERSION,
        .createRenderer = createRenderer,
        .destroy = destroy,
        .shutdown = shutdown,
        .getBackend = getBackend,
        .getDrawableExtent = getDrawableExtent,
        .getStats = getStats,
        .resize = resize,
        .initializeUi = initializeUi,
        .shutdownUi = shutdownUi,
        .handleUiEvent = handleUiEvent,
        .beginUi = beginUi,
        .endUi = endUi,
        .renderUi = renderUi,
        .isUiAvailable = isUiAvailable,
        .createGraphicsShaderProgram = createGraphicsShaderProgram,
        .destroyShaderProgram = destroyShaderProgram,
        .createBuffer = createBuffer,
        .createUniformRingBuffer = createUniformRingBuffer,
        .destroyBuffer = destroyBuffer,
        .mapBuffer = mapBuffer,
        .unmapBuffer = unmapBuffer,
        .uploadBuffer = uploadBuffer,
        .uploadUniformRingBuffer = uploadUniformRingBuffer,
        .getUniformRingBufferInfo = getUniformRingBufferInfo,
        .getUniformRingBufferSlot = getUniformRingBufferSlot,
        .getUniformRingBufferSlotOffset = getUniformRingBufferSlotOffset,
        .createTexture = createTexture,
        .uploadTexture = uploadTexture,
        .destroyTexture = destroyTexture,
        .createSampler = createSampler,
        .destroySampler = destroySampler,
        .createRenderTarget = createRenderTarget,
        .destroyRenderTarget = destroyRenderTarget,
        .validateVertexInputLayout = validateVertexInputLayout,
        .createGraphicsPipeline = createGraphicsPipeline,
        .destroyGraphicsPipeline = destroyGraphicsPipeline,
        .createDescriptorSet = createDescriptorSet,
        .destroyDescriptorSet = destroyDescriptorSet,
        .updateDescriptorBuffer = updateDescriptorBuffer,
        .updateDescriptorTexture = updateDescriptorTexture,
        .updateDescriptorSampler = updateDescriptorSampler,
        .validateDescriptorSet = validateDescriptorSet,
        .setDebugName = setDebugName,
        .beginFrame = beginFrame,
        .beginRenderPass = beginRenderPass,
        .beginRenderPassTarget = beginRenderPassTarget,
        .endRenderPass = endRenderPass,
        .bindPipeline = bindPipeline,
        .bindVertexBuffer = bindVertexBuffer,
        .bindIndexBuffer = bindIndexBuffer,
        .bindDescriptorSet = bindDescriptorSet,
        .drawIndexed = drawIndexed,
        .endFrame = endFrame,
        .loadGltfModel = loadGltfModel,
        .destroyGltfModel = destroyGltfModel,
        .loadIblEnvironment = loadIblEnvironment,
        .destroyIblEnvironment = destroyIblEnvironment,
    };
}  // namespace

const KeraRendererApiV1* keraGetRendererApiV1(void)
{
    return &g_api;
}

void keraLog(KeraLogLevel level, KeraStringView message)
{
    const std::string text = toString(message);
    switch (level)
    {
        case KERA_LOG_LEVEL_DEBUG:
            kera::Logger::getInstance().debug(text);
            break;
        case KERA_LOG_LEVEL_INFO:
            kera::Logger::getInstance().info(text);
            break;
        case KERA_LOG_LEVEL_WARNING:
            kera::Logger::getInstance().warning(text);
            break;
        case KERA_LOG_LEVEL_ERROR:
            kera::Logger::getInstance().error(text);
            break;
        case KERA_LOG_LEVEL_FATAL:
            kera::Logger::getInstance().fatal(text);
            break;
    }
}
