// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/core/input.h"

#include <SDL3/SDL_events.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>

struct SDL_Window;

namespace kera
{

    class Window
    {
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
        bool wasResized() const
        {
            return m_was_resized;
        }
        void clearResizeFlag()
        {
            m_was_resized = false;
        }

        ::SDL_Window* getSDLWindow() const
        {
            return m_window;
        }
        int getWidth() const
        {
            return m_width;
        }
        int getHeight() const
        {
            return m_height;
        }

        using KeyCallback = std::function<void(EKey key, bool pressed)>;
        void setKeyCallback(KeyCallback callback)
        {
            m_key_callback = std::move(callback);
        }

        using EventCallback = std::function<void(const SDL_Event& event)>;
        void setEventCallback(EventCallback callback)
        {
            m_event_callback = std::move(callback);
        }

    private:
        ::SDL_Window* m_window;
        int m_width;
        int m_height;
        bool m_should_close;
        bool m_was_resized;
        KeyCallback m_key_callback;
        EventCallback m_event_callback;
    };

}  // namespace kera
