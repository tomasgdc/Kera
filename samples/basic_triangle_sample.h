#pragma once

#include "samples.h"
#include "kera/renderer/interfaces.h"
#include <memory>

namespace kera {

class BasicTriangleSample : public Sample {
public:
    explicit BasicTriangleSample(std::shared_ptr<IRenderer> renderer);

    void initialize() override;
    void update(float deltaTime) override;
    void render() override;
    void cleanup() override;

private:
    bool compileShaders();
    bool createGeometry();
    bool createPipeline();
    std::shared_ptr<IRenderer> renderer_;

    // Shaders
    std::shared_ptr<IShaderModule> vertexShader_;
    std::shared_ptr<IShaderModule> fragmentShader_;

    // Geometry
    std::shared_ptr<IBuffer> vertexBuffer_;
    std::shared_ptr<IBuffer> indexBuffer_;
    uint32_t indexCount_;

    // Pipeline
    std::shared_ptr<IGraphicsPipeline> pipeline_;

    // Animation
    float rotationAngle_;
};

} // namespace kera
