#include "basic_triangle_sample.h"
#include "kera/utilities/logger.h"

#include <glm/glm.hpp>
#include <vector>

namespace kera {

namespace {

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
};

} // namespace

BasicTriangleSample::BasicTriangleSample(std::shared_ptr<IRenderer> renderer)
    : Sample("Basic Triangle with Slang")
    , renderer_(std::move(renderer))
    , indexCount_(0)
    , rotationAngle_(0.0f) {
}

void BasicTriangleSample::initialize() {
    Logger::getInstance().info("Initializing " + std::string(getName()));

    if (!createShaderProgram()) {
        Logger::getInstance().error("Failed to create shader program");
        return;
    }

    if (!createGeometry()) {
        Logger::getInstance().error("Failed to create geometry");
        return;
    }

    if (!createPipeline()) {
        Logger::getInstance().error("Failed to create pipeline");
        return;
    }

    Logger::getInstance().info("Triangle sample initialized successfully");
}

bool BasicTriangleSample::createShaderProgram() {
    ShaderProgramDesc programDesc{
        .stages = {
            {
                .path = "shaders/triangle.vert.slang",
                .entryPoint = "main",
                .stage = ShaderStage::Vertex,
                .source = ShaderSourceKind::SlangFile,
            },
            {
                .path = "shaders/triangle.frag.slang",
                .entryPoint = "main",
                .stage = ShaderStage::Fragment,
                .source = ShaderSourceKind::SlangFile,
            },
        },
    };

    shaderProgram_ = renderer_->createShaderProgram(programDesc);
    if (!shaderProgram_) {
        return false;
    }

    return true;
}

bool BasicTriangleSample::createGeometry() {
    std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{0.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}
    };

    std::vector<uint16_t> indices = { 0, 1, 2 };
    indexCount_ = static_cast<uint32_t>(indices.size());

    vertexBuffer_ = renderer_->createBuffer({
        .size = vertices.size() * sizeof(Vertex),
        .usage = BufferUsageKind::Vertex,
        .memoryAccess = MemoryAccess::CpuWrite,
    });
    if (!vertexBuffer_) {
        return false;
    }

    if (!vertexBuffer_->upload(vertices.data(), vertices.size() * sizeof(Vertex))) {
        return false;
    }

    indexBuffer_ = renderer_->createBuffer({
        .size = indices.size() * sizeof(uint16_t),
        .usage = BufferUsageKind::Index,
        .memoryAccess = MemoryAccess::CpuWrite,
    });
    if (!indexBuffer_) {
        return false;
    }

    if (!indexBuffer_->upload(indices.data(), indices.size() * sizeof(uint16_t))) {
        return false;
    }

    return true;
}

bool BasicTriangleSample::createPipeline() {
    GraphicsPipelineDesc pipelineDesc{};
    pipelineDesc.topology = PrimitiveTopologyKind::TriangleList;
    pipelineDesc.vertexLayout.bindings.push_back({
        .binding = 0,
        .stride = static_cast<uint32_t>(sizeof(Vertex)),
    });
    pipelineDesc.vertexLayout.attributes.push_back({
        .location = 0,
        .binding = 0,
        .offset = 0,
        .format = VertexFormat::Float3,
    });
    pipelineDesc.vertexLayout.attributes.push_back({
        .location = 1,
        .binding = 0,
        .offset = static_cast<uint32_t>(offsetof(Vertex, color)),
        .format = VertexFormat::Float3,
    });

    pipeline_ = renderer_->createGraphicsPipeline(
        pipelineDesc,
        *shaderProgram_);
    if (!pipeline_) {
        return false;
    }

    return true;
}

void BasicTriangleSample::update(float deltaTime) {
    rotationAngle_ += deltaTime * 45.0f;
    if (rotationAngle_ >= 360.0f) {
        rotationAngle_ -= 360.0f;
    }
}

void BasicTriangleSample::render() {
    if (!renderer_ || !pipeline_ || !vertexBuffer_ || !indexBuffer_) {
        Logger::getInstance().warning("Render called before sample resources were initialized");
        return;
    }

    std::unique_ptr<IFrame> frame = renderer_->beginFrame();
    if (!frame) {
        return;
    }

    frame->beginRenderPass({
        .clearColor = {0.0f, 0.0f, 0.1f, 1.0f},
    });
    frame->bindPipeline(*pipeline_);
    frame->bindVertexBuffer(0, *vertexBuffer_);
    frame->bindIndexBuffer(*indexBuffer_, IndexFormat::UInt16);
    frame->drawIndexed(indexCount_);
    frame->endRenderPass();

    if (!renderer_->endFrame(std::move(frame))) {
        Logger::getInstance().error("Failed to end frame");
    }
}

void BasicTriangleSample::cleanup() {
    Logger::getInstance().info("Cleaning up " + std::string(getName()));
    pipeline_.reset();
    indexBuffer_.reset();
    vertexBuffer_.reset();
    shaderProgram_.reset();
}

} // namespace kera
