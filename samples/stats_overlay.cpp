#include "stats_overlay.h"

#include "kera/renderer/interfaces.h"

#ifdef KERA_HAS_IMGUI
#include <imgui.h>

namespace kera
{

    namespace
    {

        const char* backendName(GraphicsBackend backend)
        {
            switch (backend)
            {
                case GraphicsBackend::Vulkan:
                    return "Vulkan";
                default:
                    return "Unknown";
            }
        }

    }  // namespace

#else

namespace kera
{

#endif

    void StatsOverlay::draw(const IRenderer& renderer, int activeSampleIndex, const std::string& activeSampleName,
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
        ImGui::Text("Frame %.2f ms (%.1f FPS)", frameTimeMs, fps);
        ImGui::Text("Sample %d: %s", activeSampleIndex, activeSampleName.c_str());
        ImGui::Text("Backend: %s", backendName(renderer.getBackend()));
        ImGui::Text("Drawable: %ux%u", extent.width, extent.height);
        ImGui::Separator();
        ImGui::Text("Draw calls: %u", stats.drawCallsThisFrame);
        ImGui::Text("Frame index: %llu", static_cast<unsigned long long>(stats.frameIndex));
        ImGui::Separator();
        ImGui::Text("Buffers: %u", stats.resources.buffers);
        ImGui::Text("Textures: %u", stats.resources.textures);
        ImGui::Text("Samplers: %u", stats.resources.samplers);
        ImGui::Text("Render targets: %u", stats.resources.renderTargets);
        ImGui::Text("Shader modules: %u", stats.resources.shaderModules);
        ImGui::Text("Shader programs: %u", stats.resources.shaderPrograms);
        ImGui::Text("Graphics pipelines: %u", stats.resources.graphicsPipelines);
        ImGui::Text("Descriptor sets: %u", stats.resources.descriptorSets);
        ImGui::Text("Frame resources: %u", stats.resources.frames);
        ImGui::End();
#else
        (void)renderer;
        (void)activeSampleIndex;
        (void)activeSampleName;
        (void)frameTimeMs;
#endif
    }

}  // namespace kera
