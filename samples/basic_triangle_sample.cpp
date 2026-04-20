#include "basic_triangle_sample.h"
#include <iostream>

namespace kera {

// BasicTriangleSample implementation
BasicTriangleSample::BasicTriangleSample() : Sample("Basic Triangle") {}

void BasicTriangleSample::initialize() {
    std::cout << "Initializing " << getName() << std::endl;
    // TODO: Initialize Vulkan resources, create triangle geometry, shaders, etc.
    // - Create vertex/index buffers
    // - Load/compile shaders with Slang
    // - Create pipeline
    // - Set up render pass
}

void BasicTriangleSample::update(float deltaTime) {
    // TODO: Update triangle animation, camera, etc.
    // - Rotate triangle
    // - Update MVP matrices
}

void BasicTriangleSample::render() {
    std::cout << "Rendering " << getName() << std::endl;
    // TODO: Record command buffers, submit to queue
    // - Begin render pass
    // - Bind pipeline
    // - Draw triangle
    // - End render pass
}

void BasicTriangleSample::cleanup() {
    std::cout << "Cleaning up " << getName() << std::endl;
    // TODO: Clean up Vulkan resources
    // - Destroy buffers
    // - Destroy pipeline
    // - Free shader modules
}

} // namespace kera