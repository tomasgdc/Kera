#pragma once

#include <string>

namespace kera {

class IRenderer;

class StatsOverlay {
 public:
  void toggleVisible() { m_visible = !m_visible; }
  bool isVisible() const { return m_visible; }

  void draw(const IRenderer& renderer, int activeSampleIndex,
            const std::string& activeSampleName, float frameTimeMs);

 private:
  bool m_visible = true;
};

}  // namespace kera
