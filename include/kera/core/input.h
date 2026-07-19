// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <functional>

namespace kera
{

    enum class EKey
    {
        UNKNOWN = 0,
        A,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        I,
        J,
        K,
        L,
        M,
        N,
        O,
        P,
        Q,
        R,
        S,
        T,
        U,
        V,
        W,
        X,
        Y,
        Z,
        NUM0,
        NUM1,
        NUM2,
        NUM3,
        NUM4,
        NUM5,
        NUM6,
        NUM7,
        NUM8,
        NUM9,
        ESCAPE,
        SPACE,
        ENTER,
        TAB,
        BACKSPACE,
        LEFT,
        RIGHT,
        UP,
        DOWN,
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,
        LEFT_SHIFT,
        RIGHT_SHIFT,
        LEFT_CTRL,
        RIGHT_CTRL,
        LEFT_ALT,
        RIGHT_ALT,
        COUNT
    };

    enum class EMouseButton
    {
        LEFT = 0,
        MIDDLE,
        RIGHT,
        COUNT
    };

    struct MousePosition
    {
        float x;
        float y;
    };

    class Input
    {
    public:
        Input();
        ~Input() = default;

        void update();

        // Keyboard
        bool isKeyPressed(EKey key) const;
        bool isKeyDown(EKey key) const;
        bool isKeyReleased(EKey key) const;

        // Mouse
        bool isMouseButtonPressed(EMouseButton button) const;
        bool isMouseButtonDown(EMouseButton button) const;
        bool isMouseButtonReleased(EMouseButton button) const;

        MousePosition getMousePosition() const;
        MousePosition getMouseDelta() const;

        // Input callbacks
        using KeyCallback = std::function<void(EKey, bool pressed)>;
        using MouseButtonCallback = std::function<void(EMouseButton, bool pressed)>;
        using MouseMoveCallback = std::function<void(int x, int y, int delta_x, int delta_y)>;

        void setKeyCallback(KeyCallback callback)
        {
            m_key_callback = callback;
        }
        void setMouseButtonCallback(MouseButtonCallback callback)
        {
            m_mouse_button_callback = callback;
        }
        void setMouseMoveCallback(MouseMoveCallback callback)
        {
            m_mouse_move_callback = callback;
        }

    private:
        std::array<bool, static_cast<size_t>(EKey::COUNT)> m_current_keys;
        std::array<bool, static_cast<size_t>(EKey::COUNT)> m_previous_keys;

        std::array<bool, static_cast<size_t>(EMouseButton::COUNT)> m_current_mouse_buttons;
        std::array<bool, static_cast<size_t>(EMouseButton::COUNT)> m_previous_mouse_buttons;

        MousePosition m_mouse_position;
        MousePosition m_previous_mouse_position;

        KeyCallback m_key_callback;
        MouseButtonCallback m_mouse_button_callback;
        MouseMoveCallback m_mouse_move_callback;
    };

}  // namespace kera
