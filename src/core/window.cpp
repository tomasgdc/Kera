// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/core/window.h"

#include "kera/utilities/logger.h"
#include "sdl_input_utils.h"

#include <SDL3/SDL.h>

namespace kera
{

    Window::Window() : m_window(nullptr), m_width(0), m_height(0), m_should_close(false), m_was_resized(false) {}

    Window::~Window()
    {
        shutdown();
    }

    Window::Window(Window&& other) noexcept
        : m_window(other.m_window)
        , m_width(other.m_width)
        , m_height(other.m_height)
        , m_should_close(other.m_should_close)
        , m_was_resized(other.m_was_resized)
    {
        other.m_window = nullptr;
        other.m_width = 0;
        other.m_height = 0;
        other.m_should_close = false;
        other.m_was_resized = false;
    }

    Window& Window::operator=(Window&& other) noexcept
    {
        if (this != &other)
        {
            shutdown();
            m_window = other.m_window;
            m_width = other.m_width;
            m_height = other.m_height;
            m_should_close = other.m_should_close;
            m_was_resized = other.m_was_resized;

            other.m_window = nullptr;
            other.m_width = 0;
            other.m_height = 0;
            other.m_should_close = false;
            other.m_was_resized = false;
        }
        return *this;
    }

    bool Window::initialize(const std::string& title, int width, int height)
    {
        if (m_window)
        {
            shutdown();
        }

        // Create SDL window with Vulkan support
        m_window = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

        if (!m_window)
        {
            Logger::getInstance().error("Failed to create SDL window: " + std::string(SDL_GetError()));
            return false;
        }

        m_width = width;
        m_height = height;
        m_should_close = false;
        m_was_resized = false;

        Logger::getInstance().info("Window initialized: " + title + " (" + std::to_string(width) + "x" +
                                   std::to_string(height) + ")");
        return true;
    }

    void Window::shutdown()
    {
        if (m_window)
        {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
            m_width = 0;
            m_height = 0;
            m_should_close = false;
            m_was_resized = false;
            Logger::getInstance().info("Window shutdown");
        }
    }

    bool Window::shouldClose() const
    {
        return m_should_close;
    }

    void Window::processEvents()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (m_event_callback)
            {
                m_event_callback(event);
            }

            switch (event.type)
            {
                case SDL_EVENT_QUIT:
                    m_should_close = true;
                    break;
                case SDL_EVENT_WINDOW_RESIZED:
                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    m_width = event.window.data1;
                    m_height = event.window.data2;
                    m_was_resized = true;
                    break;
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP:
                    if (m_key_callback && !event.key.repeat)
                    {
                        m_key_callback(sdlKeyToKey(event.key.key), event.key.down);
                    }
                    break;
                default:
                    break;
            }
        }
    }

}  // namespace kera
