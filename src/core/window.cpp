#include "kera/core/window.h"

#include "kera/utilities/logger.h"

#include "sdl_input_utils.h"

#include <SDL3/SDL.h>

namespace kera
{

    Window::Window() : window_(nullptr), width_(0), height_(0), should_close_(false), was_resized_(false) {}

    Window::~Window()
    {
        shutdown();
    }

    Window::Window(Window&& other) noexcept
        : window_(other.window_)
        , width_(other.width_)
        , height_(other.height_)
        , should_close_(other.should_close_)
        , was_resized_(other.was_resized_)
    {
        other.window_ = nullptr;
        other.width_ = 0;
        other.height_ = 0;
        other.should_close_ = false;
        other.was_resized_ = false;
    }

    Window& Window::operator=(Window&& other) noexcept
    {
        if (this != &other)
        {
            shutdown();
            window_ = other.window_;
            width_ = other.width_;
            height_ = other.height_;
            should_close_ = other.should_close_;
            was_resized_ = other.was_resized_;

            other.window_ = nullptr;
            other.width_ = 0;
            other.height_ = 0;
            other.should_close_ = false;
            other.was_resized_ = false;
        }
        return *this;
    }

    bool Window::initialize(const std::string& title, int width, int height)
    {
        if (window_)
        {
            shutdown();
        }

        // Create SDL window with Vulkan support
        window_ = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

        if (!window_)
        {
            Logger::getInstance().error("Failed to create SDL window: " + std::string(SDL_GetError()));
            return false;
        }

        width_ = width;
        height_ = height;
        should_close_ = false;
        was_resized_ = false;

        Logger::getInstance().info("Window initialized: " + title + " (" + std::to_string(width) + "x" +
                                   std::to_string(height) + ")");
        return true;
    }

    void Window::shutdown()
    {
        if (window_)
        {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
            width_ = 0;
            height_ = 0;
            should_close_ = false;
            was_resized_ = false;
            Logger::getInstance().info("Window shutdown");
        }
    }

    bool Window::shouldClose() const
    {
        return should_close_;
    }

    void Window::processEvents()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event_callback_)
            {
                event_callback_(event);
            }

            switch (event.type)
            {
                case SDL_EVENT_QUIT:
                    should_close_ = true;
                    break;
                case SDL_EVENT_WINDOW_RESIZED:
                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    width_ = event.window.data1;
                    height_ = event.window.data2;
                    was_resized_ = true;
                    break;
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP:
                    if (key_callback_ && !event.key.repeat)
                    {
                        key_callback_(sdlKeyToKey(event.key.key), event.key.down);
                    }
                    break;
                default:
                    break;
            }
        }
    }

}  // namespace kera
