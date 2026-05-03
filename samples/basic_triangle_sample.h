#pragma once

#include "kera/renderer/interfaces.h"
#include "samples.h"

namespace kera {

class BasicTriangleSample : public Sample {
 public:
  explicit BasicTriangleSample(IRenderer& renderer);

  void initialize() override;
  void update(float deltaTime) override;
  void render() override;
  void cleanup() override;

 private:
  bool createShaderProgram();
  bool createGeometry();
  bool createPipeline();

  IRenderer& m_renderer;
  ShaderProgramHandle m_shaderProgram;
  BufferHandle m_vertexBuffer;
  BufferHandle m_indexBuffer;
  uint32_t m_indexCount;
  GraphicsPipelineHandle m_pipeline;
  float m_rotationAngle;
};

}  // namespace kera
