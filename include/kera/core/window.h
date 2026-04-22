#pragma once

#include <memory>
#include <string>

struct SDL_Window;

namespace kera {

class Window {
public:
    Window();
    ~Window();

    // Delete copy operations
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Move operations
    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    bool initialize(const std::string& title, int width, int height);
    void shutdown();

    bool shouldClose() const;
    void processEvents();
    bool wasResized() const { return was_resized_; }
    void clearResizeFlag() { was_resized_ = false; }

    SDL_Window* getSDLWindow() const { return window_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

private:
    SDL_Window* window_;
    int width_;
    int height_;
    bool should_close_;
    bool was_resized_;
};

} // namespace kera
