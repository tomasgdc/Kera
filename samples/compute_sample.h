#pragma once

#include "samples.h"

namespace kera {

class ComputeSample : public Sample {
public:
    ComputeSample();

    void initialize() override;
    void update(float deltaTime) override;
    void render(RenderContext& context) override;
    void cleanup() override;
};

} // namespace kera
