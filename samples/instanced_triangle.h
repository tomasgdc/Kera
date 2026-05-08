#pragma once

#include "kera/renderer/interfaces.h"
#include "samples.h"

namespace kera {

class InstancedTriangleSample : public Sample {
 public:
  explicit InstancedTriangleSample(IRenderer& renderer);

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
  BufferHandle m_instanceBuffer;
  BufferHandle m_uniformBuffer;
  uint32_t m_indexCount;
  uint32_t m_instanceCount;
  GraphicsPipelineHandle m_pipeline;
  float m_rotationAngle;
};

}  // namespace kera
