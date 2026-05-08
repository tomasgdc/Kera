#pragma once

#include "kera/core/input.h"

#include <SDL3/SDL_events.h>
#include <functional>
#include <memory>
#include <string>
#include <utility>

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

    ::SDL_Window* getSDLWindow() const { return window_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

    using KeyCallback = std::function<void(Key key, bool pressed)>;
    void setKeyCallback(KeyCallback callback) { key_callback_ = std::move(callback); }

    using EventCallback = std::function<void(const SDL_Event& event)>;
    void setEventCallback(EventCallback callback) { event_callback_ = std::move(callback); }

private:
    ::SDL_Window* window_;
    int width_;
    int height_;
    bool should_close_;
    bool was_resized_;
    KeyCallback key_callback_;
    EventCallback event_callback_;
};

} // namespace kera
