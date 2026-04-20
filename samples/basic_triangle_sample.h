#pragma once

#include "samples.h"

namespace kera {

class BasicTriangleSample : public Sample {
public:
    BasicTriangleSample();

    void initialize() override;
    void update(float deltaTime) override;
    void render() override;
    void cleanup() override;
};

} // namespace kera