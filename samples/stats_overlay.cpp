// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "stats_overlay.h"

#include "kera/renderer/api.h"

#ifdef KERA_HAS_IMGUI
#include <imgui.h>
#endif

namespace kera
{

    void StatsOverlay::draw(const Renderer& renderer, int activeSampleIndex, const std::string& activeSampleName,
                            float frameTimeMs)
    {
#ifdef KERA_HAS_IMGUI
        if (!m_visible)
        {
            return;
        }

        const RendererStats stats = renderer.getStats();
        const Extent2D extent = renderer.getDrawableExtent();
        const float fps = frameTimeMs > 0.0f ? 1000.0f / frameTimeMs : 0.0f;

        ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Kera Stats", &m_visible, ImGuiWindowFlags_AlwaysAutoResize);

        if (ImGui::CollapsingHeader("Frame", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Frame %.2f ms (%.1f FPS)", frameTimeMs, fps);
            ImGui::Text("Sample %d: %s", activeSampleIndex, activeSampleName.c_str());
            ImGui::Text("Drawable: %ux%u", extent.width, extent.height);
            ImGui::Text("Frame index: %llu", static_cast<unsigned long long>(stats.frameIndex));
        }

        if (ImGui::CollapsingHeader("Frame Counters", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Draw calls: %u", stats.drawCallsThisFrame);
            ImGui::Text("Pipeline binds: %u", stats.pipelinesBoundThisFrame);
            ImGui::Text("Descriptor binds: %u", stats.descriptorSetsBoundThisFrame);
            ImGui::Text("Vertex buffer binds: %u", stats.vertexBuffersBoundThisFrame);
            ImGui::Text("Index buffer binds: %u", stats.indexBuffersBoundThisFrame);
            ImGui::Text("Buffer uploads: %u", stats.bufferUploadsThisFrame);
            ImGui::Text("Texture uploads: %u", stats.textureUploadsThisFrame);
            ImGui::Text("Validation issues: %u", stats.validationIssuesThisFrame);
        }

        if (ImGui::CollapsingHeader("Resources", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Buffers: %u", stats.resources.buffers);
            ImGui::Text("Textures: %u", stats.resources.textures);
            ImGui::Text("Samplers: %u", stats.resources.samplers);
            ImGui::Text("Render targets: %u", stats.resources.renderTargets);
            ImGui::Text("Shader modules: %u", stats.resources.shaderModules);
            ImGui::Text("Shader programs: %u", stats.resources.shaderPrograms);
            ImGui::Text("Graphics pipelines: %u", stats.resources.graphicsPipelines);
            ImGui::Text("Descriptor sets: %u", stats.resources.descriptorSets);
            ImGui::Text("Frame resources: %u", stats.resources.frames);
        }
        ImGui::End();
#else
        (void)renderer;
        (void)activeSampleIndex;
        (void)activeSampleName;
        (void)frameTimeMs;
#endif
    }

}  // namespace kera
