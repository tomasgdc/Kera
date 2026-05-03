#include "samples.h"

#include "basic_triangle_sample.h"
#include "compute_sample.h"
#include "kera/core/window.h"
#include "kera/renderer/factory.h"
#include "kera/utilities/logger.h"

namespace kera {

namespace {

void cleanupActiveSample(std::vector<std::unique_ptr<Sample>>& samples,
                         int& activeSampleIndex) {
  if (activeSampleIndex < 0 ||
      activeSampleIndex >= static_cast<int>(samples.size())) {
    return;
  }

  samples[activeSampleIndex]->cleanup();
  activeSampleIndex = -1;
}

}  // namespace

SampleApplication::SampleApplication() : m_activeSampleIndex(-1) {}

SampleApplication::~SampleApplication() {
  cleanupActiveSample(m_samples, m_activeSampleIndex);
  cleanupRenderer();
}

void SampleApplication::addSample(std::unique_ptr<Sample> sample) {
  m_samples.push_back(std::move(sample));
}

void SampleApplication::setActiveSample(int index) {
  if (index < 0 || index >= static_cast<int>(m_samples.size())) {
    Logger::getInstance().error("Invalid sample index: " +
                                std::to_string(index));
    return;
  }

  cleanupActiveSample(m_samples, m_activeSampleIndex);

  m_activeSampleIndex = index;
  m_samples[m_activeSampleIndex]->initialize();
  Logger::getInstance().info("Switched to sample: " +
                             m_samples[m_activeSampleIndex]->getName());
}

bool SampleApplication::initializeRenderer() {
  Logger::getInstance().info("Initializing renderer");

  m_window = std::make_unique<Window>();
  if (!m_window->initialize("Kera Triangle Sample", 1280, 720)) {
    Logger::getInstance().error("Failed to create window");
    return false;
  }
  Logger::getInstance().info("Window created successfully (1280x720)");

  m_renderer = CreateRenderer(GraphicsBackend::Vulkan, *m_window);
  if (!m_renderer) {
    Logger::getInstance().error("Failed to create renderer");
    return false;
  }

  Logger::getInstance().info("Renderer initialized successfully");
  return true;
}

bool SampleApplication::recreateSwapchainResources() {
  if (!m_window || !m_renderer) {
    return false;
  }

  const int width = m_window->getWidth();
  const int height = m_window->getHeight();
  if (width <= 0 || height <= 0) {
    return false;
  }

  Logger::getInstance().info("Recreating swapchain resources for window size " +
                             std::to_string(width) + "x" +
                             std::to_string(height));

  if (!m_renderer->resize({
          static_cast<uint32_t>(width),
          static_cast<uint32_t>(height),
      })) {
    Logger::getInstance().error("Failed to resize renderer");
    return false;
  }

  m_window->clearResizeFlag();
  Logger::getInstance().info("Swapchain resources recreated successfully");
  return true;
}

void SampleApplication::cleanupRenderer() {
  cleanupActiveSample(m_samples, m_activeSampleIndex);

  if (m_renderer) {
    m_renderer->shutdown();
    m_renderer.reset();
  }
  if (m_window) {
    m_window->shutdown();
  }
}

void SampleApplication::run() {
  Logger::getInstance().info("Starting Kera Sample Application");

  if (!initializeRenderer()) {
    Logger::getInstance().error("Failed to initialize renderer");
    return;
  }

  addSample(std::make_unique<BasicTriangleSample>(*m_renderer));
  // addSample(std::make_unique<ComputeSample>());

  Logger::getInstance().info("Available samples:");
  for (size_t i = 0; i < m_samples.size(); ++i) {
    Logger::getInstance().info("  " + std::to_string(i) + ": " +
                               m_samples[i]->getName());
  }

  if (m_samples.empty()) {
    Logger::getInstance().warning("No samples registered");
    return;
  }

  setActiveSample(0);
  Logger::getInstance().info("Running windowed render loop for sample 0");

  constexpr float deltaTime = 1.0f / 60.0f;
  while (!m_window->shouldClose()) {
    m_window->processEvents();
    if (m_activeSampleIndex < 0 ||
        m_activeSampleIndex >= static_cast<int>(m_samples.size())) {
      break;
    }

    if (m_window->wasResized()) {
      if (m_window->getWidth() == 0 || m_window->getHeight() == 0) {
        continue;
      }

      if (!recreateSwapchainResources()) {
        Logger::getInstance().error("Failed to handle window resize");
        break;
      }
    }

    m_samples[m_activeSampleIndex]->update(deltaTime);
    m_samples[m_activeSampleIndex]->render();
  }

  cleanupRenderer();
}

}  // namespace kera
