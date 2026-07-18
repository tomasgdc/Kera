// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "stats_overlay.h"

#include "kera/renderer/api.h"

#ifdef KERA_HAS_IMGUI
#include <imgui.h>
#endif

namespace kera
{

    void StatsOverlay::draw(const Renderer& renderer, int active_sample_index, const std::string& active_sample_name,
                            float frame_time_ms)
    {
#ifdef KERA_HAS_IMGUI
        if (!m_visible)
        {
            return;
        }

        const RendererStats stats = renderer.getStats();
        const Extent2D extent = renderer.getDrawableExtent();
        const float fps = frame_time_ms > 0.0f ? 1000.0f / frame_time_ms : 0.0f;

        ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Kera Stats", &m_visible, ImGuiWindowFlags_AlwaysAutoResize);

        if (ImGui::CollapsingHeader("Frame", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Frame %.2f ms (%.1f FPS)", frame_time_ms, fps);
            ImGui::Text("Sample %d: %s", active_sample_index, active_sample_name.c_str());
            ImGui::Text("Drawable: %ux%u", extent.width, extent.height);
            ImGui::Text("Frame index: %llu", static_cast<unsigned long long>(stats.frame_index));
        }

        if (ImGui::CollapsingHeader("Frame Counters", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Draw calls: %u", stats.draw_calls_this_frame);
            ImGui::Text("Pipeline binds: %u", stats.pipelines_bound_this_frame);
            ImGui::Text("Descriptor binds: %u", stats.descriptor_sets_bound_this_frame);
            ImGui::Text("Vertex buffer binds: %u", stats.vertex_buffers_bound_this_frame);
            ImGui::Text("Index buffer binds: %u", stats.index_buffers_bound_this_frame);
            ImGui::Text("Buffer uploads: %u", stats.buffer_uploads_this_frame);
            ImGui::Text("Texture uploads: %u", stats.texture_uploads_this_frame);
            ImGui::Text("Validation issues: %u", stats.validation_issues_this_frame);
        }

        if (ImGui::CollapsingHeader("Resources", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Buffers: %u", stats.resources.buffers);
            ImGui::Text("Textures: %u", stats.resources.textures);
            ImGui::Text("Samplers: %u", stats.resources.samplers);
            ImGui::Text("Render targets: %u", stats.resources.render_targets);
            ImGui::Text("Shader modules: %u", stats.resources.shader_modules);
            ImGui::Text("Shader programs: %u", stats.resources.shader_programs);
            ImGui::Text("Graphics pipelines: %u", stats.resources.graphics_pipelines);
            ImGui::Text("Descriptor sets: %u", stats.resources.descriptor_sets);
            ImGui::Text("Frame resources: %u", stats.resources.frames);
        }
        ImGui::End();
#else
        (void)renderer;
        (void)active_sample_index;
        (void)active_sample_name;
        (void)frame_time_ms;
#endif
    }

}  // namespace kera
