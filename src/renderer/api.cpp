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
    mutable std::vector<KeraRendererValidationIssue> validation_issues;
    mutable std::vector<std::string> validation_messages;
    mutable std::vector<std::string> validation_names;
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

    kera::EBufferUsageKind fromKera(KeraBufferUsageKind value)
    {
        return static_cast<kera::EBufferUsageKind>(value);
    }

    kera::EMemoryAccess fromKera(KeraMemoryAccess value)
    {
        return static_cast<kera::EMemoryAccess>(value);
    }

    kera::EIndexFormat fromKera(KeraIndexFormat value)
    {
        return static_cast<kera::EIndexFormat>(value);
    }

    kera::EVertexFormat fromKera(KeraVertexFormat value)
    {
        return static_cast<kera::EVertexFormat>(value);
    }

    kera::EVertexInputRate fromKera(KeraVertexInputRate value)
    {
        return static_cast<kera::EVertexInputRate>(value);
    }

    [[maybe_unused]] kera::EDescriptorType fromKera(KeraDescriptorType value)
    {
        return static_cast<kera::EDescriptorType>(value);
    }

    kera::ETextureFormat fromKera(KeraTextureFormat value)
    {
        return static_cast<kera::ETextureFormat>(value);
    }

    kera::ESamplerFilter fromKera(KeraSamplerFilter value)
    {
        return static_cast<kera::ESamplerFilter>(value);
    }

    kera::ESamplerMipFilter fromKera(KeraSamplerMipFilter value)
    {
        return static_cast<kera::ESamplerMipFilter>(value);
    }

    kera::ESamplerAddressMode fromKera(KeraSamplerAddressMode value)
    {
        return static_cast<kera::ESamplerAddressMode>(value);
    }

    kera::EPrimitiveTopologyKind fromKera(KeraPrimitiveTopologyKind value)
    {
        return static_cast<kera::EPrimitiveTopologyKind>(value);
    }

    kera::ECullModeKind fromKera(KeraCullModeKind value)
    {
        return static_cast<kera::ECullModeKind>(value);
    }

    kera::EFrontFaceKind fromKera(KeraFrontFaceKind value)
    {
        return static_cast<kera::EFrontFaceKind>(value);
    }

    kera::EBlendModeKind fromKera(KeraBlendModeKind value)
    {
        return static_cast<kera::EBlendModeKind>(value);
    }

    KeraIndexFormat toKera(kera::EIndexFormat value)
    {
        return static_cast<KeraIndexFormat>(value);
    }

    KeraGltfAlphaMode toKera(kera::EGltfAlphaMode value)
    {
        return static_cast<KeraGltfAlphaMode>(value);
    }

    [[maybe_unused]] kera::EGltfAlphaMode fromKera(KeraGltfAlphaMode value)
    {
        return static_cast<kera::EGltfAlphaMode>(value);
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
            .memory_access = fromKera(desc.memory_access),
            .debug_name = toString(desc.debug_name),
        };
    }

    kera::TextureDesc fromKera(const KeraTextureDesc& desc)
    {
        return {
            .width = desc.width,
            .height = desc.height,
            .format = fromKera(desc.format),
            .mip_levels = desc.mip_levels == 0 ? 1u : desc.mip_levels,
            .generate_mipmaps = desc.generate_mipmaps != 0,
            .render_target = desc.render_target != 0,
            .sampled = desc.sampled != 0,
            .depth_stencil = desc.depth_stencil != 0,
            .debug_name = toString(desc.debug_name),
        };
    }

    kera::SamplerDesc fromKera(const KeraSamplerDesc& desc)
    {
        return {
            .min_filter = fromKera(desc.min_filter),
            .mag_filter = fromKera(desc.mag_filter),
            .mip_filter = fromKera(desc.mip_filter),
            .address_mode_u = fromKera(desc.address_mode_u),
            .address_mode_v = fromKera(desc.address_mode_v),
            .min_lod = desc.min_lod,
            .max_lod = desc.max_lod,
            .max_anisotropy = desc.max_anisotropy == 0.0f ? 1.0f : desc.max_anisotropy,
            .debug_name = toString(desc.debug_name),
        };
    }

    kera::RenderTargetDesc fromKera(const KeraRenderTargetDesc& desc)
    {
        return {
            .color_texture = fromKera<kera::TextureHandle>(desc.color_texture),
            .depth_texture = fromKera<kera::TextureHandle>(desc.depth_texture),
            .debug_name = toString(desc.debug_name),
        };
    }

    std::vector<kera::ReflectedVertexBindingDesc> fromKeraVertexBindings(const KeraVertexInputLayout& layout)
    {
        std::vector<kera::ReflectedVertexBindingDesc> bindings;
        bindings.reserve(layout.binding_count);
        for (size_t i = 0; i < layout.binding_count; ++i)
        {
            const KeraVertexInputBindingDesc& binding = layout.bindings[i];
            bindings.push_back({
                .binding = binding.binding,
                .stride = binding.stride,
                .input_rate = fromKera(binding.input_rate),
            });
        }
        return bindings;
    }

    std::vector<kera::ReflectedVertexFieldDesc> fromKeraVertexFields(const KeraVertexInputLayout& layout)
    {
        std::vector<kera::ReflectedVertexFieldDesc> fields;
        fields.reserve(layout.field_count);
        for (size_t i = 0; i < layout.field_count; ++i)
        {
            const KeraVertexInputFieldDesc& field = layout.fields[i];
            fields.push_back({
                .parameter_name = toString(field.parameter_name),
                .field_name = toString(field.field_name),
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
            .shader_program = fromKera<kera::ShaderProgramHandle>(desc.shader_program),
            .render_target = fromKera<kera::RenderTargetHandle>(desc.render_target),
            .topology = fromKera(desc.topology),
            .cull_mode = fromKera(desc.cull_mode),
            .front_face = fromKera(desc.front_face),
            .blend_mode = fromKera(desc.blend_mode),
            .depth_test = desc.depth_test != 0,
            .depth_write = desc.depth_write != 0,
            .vertex_bindings = fromKeraVertexBindings(desc.vertex_input),
            .vertex_fields = fromKeraVertexFields(desc.vertex_input),
            .vertex_layout = {},
            .descriptor_sets = {},
            .debug_name = toString(desc.debug_name),
        };
    }

    KeraRendererValidationReport storeValidationReport(const KeraRenderer* renderer,
                                                       const kera::RendererValidationReport& report)
    {
        renderer->validation_issues.clear();
        renderer->validation_messages.clear();
        renderer->validation_names.clear();

        renderer->validation_issues.reserve(report.issues.size());
        renderer->validation_messages.reserve(report.issues.size());
        renderer->validation_names.reserve(report.issues.size());

        for (const kera::RendererValidationIssue& issue : report.issues)
        {
            renderer->validation_messages.push_back(issue.message);
            renderer->validation_names.push_back(issue.name);
            renderer->validation_issues.push_back({
                .message = toView(renderer->validation_messages.back()),
                .name = toView(renderer->validation_names.back()),
                .set = issue.set,
                .binding = issue.binding,
            });
        }

        return {
            .issues = renderer->validation_issues.data(),
            .issue_count = renderer->validation_issues.size(),
        };
    }

    KeraRendererStats toKera(const kera::RendererStats& stats)
    {
        return {
            .resources =
                {
                    .shader_modules = stats.resources.shader_modules,
                    .shader_programs = stats.resources.shader_programs,
                    .buffers = stats.resources.buffers,
                    .textures = stats.resources.textures,
                    .samplers = stats.resources.samplers,
                    .render_targets = stats.resources.render_targets,
                    .graphics_pipelines = stats.resources.graphics_pipelines,
                    .descriptor_sets = stats.resources.descriptor_sets,
                    .frames = stats.resources.frames,
                },
            .draw_calls_this_frame = stats.draw_calls_this_frame,
            .pipelines_bound_this_frame = stats.pipelines_bound_this_frame,
            .descriptor_sets_bound_this_frame = stats.descriptor_sets_bound_this_frame,
            .vertex_buffers_bound_this_frame = stats.vertex_buffers_bound_this_frame,
            .index_buffers_bound_this_frame = stats.index_buffers_bound_this_frame,
            .buffer_uploads_this_frame = stats.buffer_uploads_this_frame,
            .texture_uploads_this_frame = stats.texture_uploads_this_frame,
            .validation_issues_this_frame = stats.validation_issues_this_frame,
            .frame_index = stats.frame_index,
        };
    }

    KeraGltfLoadedModel toKera(const kera::GltfLoadedModel& model)
    {
        KeraGltfLoadedModel out{};
        out.vertex_buffer = toKera(model.vertex_buffer);
        out.index_buffer = toKera(model.index_buffer);
        out.material_textures.base_color = toKera(model.material_textures.base_color);
        out.material_textures.metal_roughness = toKera(model.material_textures.metal_roughness);
        out.material_textures.emissive = toKera(model.material_textures.emissive);
        out.material_textures.occlusion = toKera(model.material_textures.occlusion);
        out.material_textures.normal = toKera(model.material_textures.normal);
        out.material_sampler = toKera(model.material_sampler);
        out.index_format = toKera(model.index_format);
        out.index_count = model.index_count;
        const float* transform = &model.transform[0][0];
        for (size_t i = 0; i < 16; ++i)
        {
            out.transform[i] = transform[i];
        }
        out.material_factors.base_color[0] = model.material_factors.base_color.r;
        out.material_factors.base_color[1] = model.material_factors.base_color.g;
        out.material_factors.base_color[2] = model.material_factors.base_color.b;
        out.material_factors.base_color[3] = model.material_factors.base_color.a;
        out.material_factors.emissive[0] = model.material_factors.emissive.x;
        out.material_factors.emissive[1] = model.material_factors.emissive.y;
        out.material_factors.emissive[2] = model.material_factors.emissive.z;
        out.material_factors.metallic = model.material_factors.metallic;
        out.material_factors.roughness = model.material_factors.roughness;
        out.material_factors.normal_scale = model.material_factors.normal_scale;
        out.material_factors.occlusion_strength = model.material_factors.occlusion_strength;
        out.material_factors.alpha_cutoff = model.material_factors.alpha_cutoff;
        out.material_factors.alpha_mode = toKera(model.material_factors.alpha_mode);
        out.material_factors.double_sided = model.material_factors.double_sided ? 1u : 0u;
        return out;
    }

    kera::GltfLoadedModel fromKeraModel(const KeraGltfLoadedModel& model)
    {
        kera::GltfLoadedModel out{};
        out.vertex_buffer = fromKera<kera::BufferHandle>(model.vertex_buffer);
        out.index_buffer = fromKera<kera::BufferHandle>(model.index_buffer);
        out.material_textures.base_color = fromKera<kera::TextureHandle>(model.material_textures.base_color);
        out.material_textures.metal_roughness = fromKera<kera::TextureHandle>(model.material_textures.metal_roughness);
        out.material_textures.emissive = fromKera<kera::TextureHandle>(model.material_textures.emissive);
        out.material_textures.occlusion = fromKera<kera::TextureHandle>(model.material_textures.occlusion);
        out.material_textures.normal = fromKera<kera::TextureHandle>(model.material_textures.normal);
        out.material_sampler = fromKera<kera::SamplerHandle>(model.material_sampler);
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
            .ibl_texture = toKera(value.ibl_texture),
            .skybox_texture = toKera(value.skybox_texture),
            .sampler = toKera(value.sampler),
            .spherical_harmonics = toKera(value.spherical_harmonics),
            .ibl_mip_levels = value.ibl_miplevels,
            .skybox_mip_levels = value.skybox_miplevels,
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
            .ibl_texture = fromKera<kera::TextureHandle>(value.ibl_texture),
            .skybox_texture = fromKera<kera::TextureHandle>(value.skybox_texture),
            .sampler = fromKera<kera::SamplerHandle>(value.sampler),
            .spherical_harmonics = fromKera(value.spherical_harmonics),
            .ibl_miplevels = value.ibl_mip_levels,
            .skybox_miplevels = value.skybox_mip_levels,
        };
    }

    KeraUniformRingBufferInfo toKera(const kera::UniformRingBufferInfo& value)
    {
        return {
            .element_size = value.element_size,
            .slot_stride = value.slot_stride,
            .slot_count = value.slot_count,
        };
    }

    [[maybe_unused]] kera::UniformRingBufferInfo fromKera(const KeraUniformRingBufferInfo& value)
    {
        return {
            .element_size = value.element_size,
            .slot_stride = value.slot_stride,
            .slot_count = value.slot_count,
        };
    }

    KeraRenderer* createRenderer(const KeraRendererCreateDesc* desc)
    {
        if (!desc || !desc->sdl_window || desc->backend != KERA_GRAPHICS_BACKEND_VULKAN)
        {
            return nullptr;
        }

        auto renderer = std::make_unique<kera::VulkanRenderer>();
        if (!renderer->initialize(static_cast<SDL_Window*>(desc->sdl_window), desc->width, desc->height))
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

    void handleUiEvent(KeraRenderer* renderer, const void* sdl_event)
    {
        if (renderer && renderer->renderer && sdl_event)
        {
            renderer->renderer->handleUiEvent(*static_cast<const SDL_Event*>(sdl_event));
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
            .vertex_entry_point = toString(desc->vertex_entry_point),
            .fragment_entry_point = toString(desc->fragment_entry_point),
            .source = static_cast<kera::EShaderSourceKind>(desc->source),
            .debug_name = toString(desc->debug_name),
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

    KeraBufferHandle createUniformRingBuffer(KeraRenderer* renderer, size_t element_size, uint32_t slot_count)
    {
        return renderer && renderer->renderer
                   ? toKera(renderer->renderer->createUniformRingBuffer(element_size, slot_count))
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
                                                           KeraShaderProgramHandle shader_program,
                                                           KeraVertexInputLayout vertex_input)
    {
        if (!renderer || !renderer->renderer)
        {
            return {};
        }

        kera::RendererValidationReport report;
        const kera::SlangReflectionMetadata* reflection =
            renderer->renderer->getShaderProgramReflection(fromKera<kera::ShaderProgramHandle>(shader_program));
        if (!reflection)
        {
            report.addIssue("Shader program reflection is missing while validating vertex input layout.");
            return storeValidationReport(renderer, report);
        }

        const std::vector<kera::ReflectedVertexBindingDesc> bindings = fromKeraVertexBindings(vertex_input);
        const std::vector<kera::ReflectedVertexFieldDesc> fields = fromKeraVertexFields(vertex_input);
        const kera::VertexInputLayoutBuildResult result =
            kera::buildValidatedVertexInputLayout(*reflection, {
                                                                   .debug_name = {},
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
                {.clear_color = fromKera(desc->clear_color), .clear_depth = desc->clear_depth});
        }
    }

    void beginRenderPassTarget(KeraRenderer* renderer, KeraFrameHandle frame, KeraRenderTargetHandle target,
                               const KeraRenderPassDesc* desc)
    {
        if (renderer && renderer->renderer && desc)
        {
            renderer->renderer->beginRenderPass(
                fromKera<kera::FrameHandle>(frame), fromKera<kera::RenderTargetHandle>(target),
                {.clear_color = fromKera(desc->clear_color), .clear_depth = desc->clear_depth});
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
                           uint32_t set_index, KeraDescriptorSetHandle descriptor_set)
    {
        if (renderer && renderer->renderer)
        {
            renderer->renderer->bindDescriptorSet(fromKera<kera::FrameHandle>(frame),
                                                  fromKera<kera::GraphicsPipelineHandle>(pipeline), set_index,
                                                  fromKera<kera::DescriptorSetHandle>(descriptor_set));
        }
    }

    void drawIndexed(KeraRenderer* renderer, KeraFrameHandle frame, uint32_t index_count, uint32_t instance_count)
    {
        if (renderer && renderer->renderer)
        {
            renderer->renderer->drawIndexed(fromKera<kera::FrameHandle>(frame), index_count, instance_count);
        }
    }

    int endFrame(KeraRenderer* renderer, KeraFrameHandle frame)
    {
        return renderer && renderer->renderer && renderer->renderer->endFrame(fromKera<kera::FrameHandle>(frame)) ? 1
                                                                                                                  : 0;
    }

    int loadGltfModel(KeraRenderer* renderer, const KeraGltfLoadDesc* desc, KeraGltfLoadedModel* out_model)
    {
        if (!renderer || !renderer->renderer || !desc || !out_model)
        {
            return 0;
        }

        kera::GltfLoadDesc load_desc{
            .path = toString(desc->path),
            .debug_name = toString(desc->debug_name),
            .require_material_textures = desc->require_material_textures != 0,
        };
        kera::RendererResult<kera::GltfLoadedModel> result = kera::loadGltfModel(*renderer->renderer, load_desc);
        if (!result)
        {
            kera::Logger::getInstance().error("glTF load failed: " + result.errorMessage());
            return 0;
        }

        *out_model = toKera(result.value());
        return 1;
    }

    void destroyGltfModel(KeraRenderer* renderer, KeraGltfLoadedModel* model)
    {
        if (!renderer || !renderer->renderer || !model)
        {
            return;
        }
        kera::GltfLoadedModel internal_model = fromKeraModel(*model);
        kera::destroyGltfModel(*renderer->renderer, internal_model);
        *model = {};
    }

    int loadIblEnvironment(KeraRenderer* renderer, const KeraIblEnvironmentLoadDesc* desc,
                           KeraIblEnvironment* out_environment)
    {
        if (!renderer || !renderer->renderer || !desc || !out_environment)
        {
            return 0;
        }

        kera::IblEnvironmentLoadDesc load_desc{.ibl_ktx_path = toString(desc->ibl_ktx_path),
                                               .skybox_ktx_path = toString(desc->skybox_ktx_path),
                                               .spherical_harmonics_path = toString(desc->spherical_harmonics_path),
                                               .debug_name = toString(desc->debug_name)};

        kera::RendererResult<kera::IblEnvironment> result = kera::loadIblEnvironment(*renderer->renderer, load_desc);
        if (!result)
        {
            kera::Logger::getInstance().error("IBL environment load failed: " + result.errorMessage());
            return 0;
        }

        *out_environment = toKera(result.value());
        return 1;
    }

    void destroyIblEnvironment(KeraRenderer* renderer, KeraIblEnvironment* environment)
    {
        if (!renderer || !renderer->renderer || !environment)
        {
            return;
        }

        kera::IblEnvironment internal_env = fromKera(*environment);
        kera::destroyIblEnvironment(*renderer->renderer, internal_env);
        *environment = {};
    }

    const KeraRendererApiV1 kGApi{
        .abi_version = KERA_RENDERER_ABI_VERSION,
        .create_renderer = createRenderer,
        .destroy = destroy,
        .shutdown = shutdown,
        .get_backend = getBackend,
        .get_drawable_extent = getDrawableExtent,
        .get_stats = getStats,
        .resize = resize,
        .initialize_ui = initializeUi,
        .shutdown_ui = shutdownUi,
        .handle_ui_event = handleUiEvent,
        .begin_ui = beginUi,
        .end_ui = endUi,
        .render_ui = renderUi,
        .is_ui_available = isUiAvailable,
        .create_graphics_shader_program = createGraphicsShaderProgram,
        .destroy_shader_program = destroyShaderProgram,
        .create_buffer = createBuffer,
        .create_uniform_ring_buffer = createUniformRingBuffer,
        .destroy_buffer = destroyBuffer,
        .map_buffer = mapBuffer,
        .unmap_buffer = unmapBuffer,
        .upload_buffer = uploadBuffer,
        .upload_uniform_ring_buffer = uploadUniformRingBuffer,
        .get_uniform_ring_buffer_info = getUniformRingBufferInfo,
        .get_uniform_ring_buffer_slot = getUniformRingBufferSlot,
        .get_uniform_ring_buffer_slot_offset = getUniformRingBufferSlotOffset,
        .create_texture = createTexture,
        .upload_texture = uploadTexture,
        .destroy_texture = destroyTexture,
        .create_sampler = createSampler,
        .destroy_sampler = destroySampler,
        .create_render_target = createRenderTarget,
        .destroy_render_target = destroyRenderTarget,
        .validate_vertex_input_layout = validateVertexInputLayout,
        .create_graphics_pipeline = createGraphicsPipeline,
        .destroy_graphics_pipeline = destroyGraphicsPipeline,
        .create_descriptor_set = createDescriptorSet,
        .destroy_descriptor_set = destroyDescriptorSet,
        .update_descriptor_buffer = updateDescriptorBuffer,
        .update_descriptor_texture = updateDescriptorTexture,
        .update_descriptor_sampler = updateDescriptorSampler,
        .validate_descriptor_set = validateDescriptorSet,
        .set_debug_name = setDebugName,
        .begin_frame = beginFrame,
        .begin_render_pass = beginRenderPass,
        .begin_render_pass_target = beginRenderPassTarget,
        .end_render_pass = endRenderPass,
        .bind_pipeline = bindPipeline,
        .bind_vertex_buffer = bindVertexBuffer,
        .bind_index_buffer = bindIndexBuffer,
        .bind_descriptor_set = bindDescriptorSet,
        .draw_indexed = drawIndexed,
        .end_frame = endFrame,
        .load_gltf_model = loadGltfModel,
        .destroy_gltf_model = destroyGltfModel,
        .load_ibl_environment = loadIblEnvironment,
        .destroy_ibl_environment = destroyIblEnvironment,
    };
}  // namespace

const KeraRendererApiV1* keraGetRendererApiV1(void)
{
    return &kGApi;
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
