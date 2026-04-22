#include "basic_triangle_sample.h"
#include "kera/utilities/logger.h"

#include <cstddef>
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

    if (!compileShaders()) {
        Logger::getInstance().error("Failed to compile shaders");
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

bool BasicTriangleSample::compileShaders() {
    const auto vertexShaderResult = renderer_->createShaderModule({
        .path = "shaders/triangle.vert.slang",
        .entryPoint = "main",
        .stage = ShaderStage::Vertex,
    });
    if (vertexShaderResult.hasError()) {
        Logger::getInstance().error(vertexShaderResult.error());
        return false;
    }
    vertexShader_ = vertexShaderResult.value();

    const auto fragmentShaderResult = renderer_->createShaderModule({
        .path = "shaders/triangle.frag.slang",
        .entryPoint = "main",
        .stage = ShaderStage::Fragment,
    });
    if (fragmentShaderResult.hasError()) {
        Logger::getInstance().error(fragmentShaderResult.error());
        return false;
    }
    fragmentShader_ = fragmentShaderResult.value();

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

    const auto vertexBufferResult = renderer_->createBuffer({
        .size = vertices.size() * sizeof(Vertex),
        .usage = BufferUsageKind::Vertex,
        .memoryAccess = MemoryAccess::CpuWrite,
    });
    if (vertexBufferResult.hasError()) {
        Logger::getInstance().error(vertexBufferResult.error());
        return false;
    }
    vertexBuffer_ = vertexBufferResult.value();

    const auto vertexUploadResult = vertexBuffer_->upload(vertices.data(), vertices.size() * sizeof(Vertex));
    if (vertexUploadResult.hasError()) {
        Logger::getInstance().error(vertexUploadResult.error());
        return false;
    }

    const auto indexBufferResult = renderer_->createBuffer({
        .size = indices.size() * sizeof(uint16_t),
        .usage = BufferUsageKind::Index,
        .memoryAccess = MemoryAccess::CpuWrite,
    });
    if (indexBufferResult.hasError()) {
        Logger::getInstance().error(indexBufferResult.error());
        return false;
    }
    indexBuffer_ = indexBufferResult.value();

    const auto indexUploadResult = indexBuffer_->upload(indices.data(), indices.size() * sizeof(uint16_t));
    if (indexUploadResult.hasError()) {
        Logger::getInstance().error(indexUploadResult.error());
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

    const auto pipelineResult = renderer_->createGraphicsPipeline(
        pipelineDesc,
        *vertexShader_,
        *fragmentShader_);
    if (pipelineResult.hasError()) {
        Logger::getInstance().error(pipelineResult.error());
        return false;
    }

    pipeline_ = pipelineResult.value();
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

    auto frameResult = renderer_->beginFrame();
    if (frameResult.hasError()) {
        Logger::getInstance().error(frameResult.error());
        return;
    }

    std::unique_ptr<IFrame> frame = std::move(frameResult.value());
    frame->beginRenderPass({
        .clearColor = {0.0f, 0.0f, 0.1f, 1.0f},
    });
    frame->bindPipeline(*pipeline_);
    frame->bindVertexBuffer(0, *vertexBuffer_);
    frame->bindIndexBuffer(*indexBuffer_, IndexFormat::UInt16);
    frame->drawIndexed(indexCount_);
    frame->endRenderPass();

    auto endFrameResult = renderer_->endFrame(std::move(frame));
    if (endFrameResult.hasError()) {
        Logger::getInstance().error(endFrameResult.error());
    }
}

void BasicTriangleSample::cleanup() {
    Logger::getInstance().info("Cleaning up " + std::string(getName()));
    pipeline_.reset();
    indexBuffer_.reset();
    vertexBuffer_.reset();
    fragmentShader_.reset();
    vertexShader_.reset();
}

} // namespace kera
