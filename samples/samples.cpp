#include "samples.h"
#include "basic_triangle_sample.h"
#include "compute_sample.h"
#include <iostream>

namespace kera {

// SampleApplication implementation
SampleApplication::SampleApplication() : activeSampleIndex_(-1) {
    // Register available samples
    addSample(std::make_unique<BasicTriangleSample>());
    addSample(std::make_unique<ComputeSample>());
}

SampleApplication::~SampleApplication() {
    if (activeSampleIndex_ >= 0 && activeSampleIndex_ < samples_.size()) {
        samples_[activeSampleIndex_]->cleanup();
    }
}

void SampleApplication::addSample(std::unique_ptr<Sample> sample) {
    samples_.push_back(std::move(sample));
}

void SampleApplication::setActiveSample(int index) {
    if (index < 0 || index >= static_cast<int>(samples_.size())) {
        std::cerr << "Invalid sample index: " << index << std::endl;
        return;
    }

    // Cleanup current sample
    if (activeSampleIndex_ >= 0 && activeSampleIndex_ < static_cast<int>(samples_.size())) {
        samples_[activeSampleIndex_]->cleanup();
    }

    activeSampleIndex_ = index;
    samples_[activeSampleIndex_]->initialize();
    std::cout << "Switched to sample: " << samples_[activeSampleIndex_]->getName() << std::endl;
}

void SampleApplication::run() {
    std::cout << "Kera Sample Application" << std::endl;
    std::cout << "Available samples:" << std::endl;

    for (size_t i = 0; i < samples_.size(); ++i) {
        std::cout << "  " << i << ": " << samples_[i]->getName() << std::endl;
    }

    std::cout << "Enter sample number to run (or 'q' to quit): ";
    std::string input;
    while (std::getline(std::cin, input)) {
        if (input == "q" || input == "quit") {
            break;
        }

        try {
            int sampleIndex = std::stoi(input);
            setActiveSample(sampleIndex);

            // Simple render loop simulation
            std::cout << "Running sample for 10 frames..." << std::endl;
            for (int frame = 0; frame < 10; ++frame) {
                float deltaTime = 1.0f / 60.0f; // 60 FPS
                samples_[activeSampleIndex_]->update(deltaTime);
                samples_[activeSampleIndex_]->render();
            }
            std::cout << "Sample completed." << std::endl;

        } catch (const std::exception&) {
            std::cout << "Invalid input. Please enter a sample number or 'q' to quit." << std::endl;
        }

        std::cout << "Enter sample number to run (or 'q' to quit): ";
    }
}

} // namespace kera