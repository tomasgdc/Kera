// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/core/input.h"

#include "sdl_input_utils.h"

#include <SDL3/SDL.h>

#include <algorithm>

namespace kera
{

    namespace
    {

        EMouseButton sdlButtonToMouseButton(int sdl_button)
        {
            switch (sdl_button)
            {
                case SDL_BUTTON_LEFT:
                    return EMouseButton::LEFT;
                case SDL_BUTTON_MIDDLE:
                    return EMouseButton::MIDDLE;
                case SDL_BUTTON_RIGHT:
                    return EMouseButton::RIGHT;
                default:
                    return EMouseButton::LEFT;
            }
        }

    }  // anonymous namespace

    Input::Input()
        : m_current_keys()
        , m_previous_keys()
        , m_current_mouse_buttons()
        , m_previous_mouse_buttons()
        , m_mouse_position{0, 0}
        , m_previous_mouse_position{0, 0}
    {
    }

    void Input::update()
    {
        m_previous_keys = m_current_keys;
        m_previous_mouse_buttons = m_current_mouse_buttons;
        m_previous_mouse_position = m_mouse_position;

        const bool* sdl_keys = SDL_GetKeyboardState(nullptr);
        for (size_t i = 0; i < m_current_keys.size(); ++i)
        {
            m_current_keys[i] = sdl_keys[i];
        }

        Uint32 mouse_state = SDL_GetMouseState(&m_mouse_position.x, &m_mouse_position.y);
        m_current_mouse_buttons[static_cast<size_t>(EMouseButton::LEFT)] = mouse_state & SDL_BUTTON_LMASK;
        m_current_mouse_buttons[static_cast<size_t>(EMouseButton::MIDDLE)] = mouse_state & SDL_BUTTON_MMASK;
        m_current_mouse_buttons[static_cast<size_t>(EMouseButton::RIGHT)] = mouse_state & SDL_BUTTON_RMASK;

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP:
                {
                    EKey key = sdlKeyToKey(event.key.key);
                    bool pressed = event.key.down;
                    if (m_key_callback)
                    {
                        m_key_callback(key, pressed);
                    }
                    break;
                }
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP:
                {
                    EMouseButton button = sdlButtonToMouseButton(event.button.button);
                    bool pressed = event.button.down;
                    if (m_mouse_button_callback)
                    {
                        m_mouse_button_callback(button, pressed);
                    }
                    break;
                }
                case SDL_EVENT_MOUSE_MOTION:
                {
                    if (m_mouse_move_callback)
                    {
                        m_mouse_move_callback(static_cast<int>(event.motion.x), static_cast<int>(event.motion.y),
                                             static_cast<int>(event.motion.xrel), static_cast<int>(event.motion.yrel));
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    bool Input::isKeyPressed(EKey key) const
    {
        size_t index = static_cast<size_t>(key);
        return index < m_current_keys.size() && m_current_keys[index] && !m_previous_keys[index];
    }

    bool Input::isKeyDown(EKey key) const
    {
        size_t index = static_cast<size_t>(key);
        return index < m_current_keys.size() && m_current_keys[index];
    }

    bool Input::isKeyReleased(EKey key) const
    {
        size_t index = static_cast<size_t>(key);
        return index < m_current_keys.size() && !m_current_keys[index] && m_previous_keys[index];
    }

    bool Input::isMouseButtonPressed(EMouseButton button) const
    {
        size_t index = static_cast<size_t>(button);
        return index < m_current_mouse_buttons.size() && m_current_mouse_buttons[index] &&
               !m_previous_mouse_buttons[index];
    }

    bool Input::isMouseButtonDown(EMouseButton button) const
    {
        size_t index = static_cast<size_t>(button);
        return index < m_current_mouse_buttons.size() && m_current_mouse_buttons[index];
    }

    bool Input::isMouseButtonReleased(EMouseButton button) const
    {
        size_t index = static_cast<size_t>(button);
        return index < m_current_mouse_buttons.size() && !m_current_mouse_buttons[index] &&
               m_previous_mouse_buttons[index];
    }

    MousePosition Input::getMousePosition() const
    {
        return m_mouse_position;
    }

    MousePosition Input::getMouseDelta() const
    {
        return {m_mouse_position.x - m_previous_mouse_position.x, m_mouse_position.y - m_previous_mouse_position.y};
    }

}  // namespace kera
