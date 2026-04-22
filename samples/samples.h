#pragma once

#include <memory>
#include <string>
#include <vector>

namespace kera {

// Forward declarations
class Sample;
class IRenderer;
class Window;

class SampleApplication {
public:
    SampleApplication();
    ~SampleApplication();

    void run();
    void addSample(std::unique_ptr<Sample> sample);
    void setActiveSample(int index);

private:
    bool initializeRenderer();
    bool recreateSwapchainResources();
    void cleanupRenderer();

    std::vector<std::unique_ptr<Sample>> samples_;
    int activeSampleIndex_;

    // Window and renderer resources
    std::unique_ptr<Window> window_;
    std::shared_ptr<IRenderer> renderer_;
};

class Sample {
public:
    Sample(const std::string& name) : name_(name) {}
    virtual ~Sample() = default;

    const std::string& getName() const { return name_; }

    virtual void initialize() = 0;
    virtual void update(float deltaTime) = 0;
    virtual void render() = 0;
    virtual void cleanup() = 0;

private:
    std::string name_;
};

} // namespace kera
