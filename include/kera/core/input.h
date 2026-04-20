#pragma once

#include <array>
#include <functional>

namespace kera {

enum class Key {
    Unknown = 0,
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    Escape, Space, Enter, Tab, Backspace,
    Left, Right, Up, Down,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    LeftShift, RightShift, LeftCtrl, RightCtrl, LeftAlt, RightAlt,
    Count
};

enum class MouseButton {
    Left = 0,
    Middle,
    Right,
    Count
};

struct MousePosition {
    float x;
    float y;
};

class Input {
public:
    Input();
    ~Input() = default;

    void update();

    // Keyboard
    bool isKeyPressed(Key key) const;
    bool isKeyDown(Key key) const;
    bool isKeyReleased(Key key) const;

    // Mouse
    bool isMouseButtonPressed(MouseButton button) const;
    bool isMouseButtonDown(MouseButton button) const;
    bool isMouseButtonReleased(MouseButton button) const;

    MousePosition getMousePosition() const;
    MousePosition getMouseDelta() const;

    // Input callbacks
    using KeyCallback = std::function<void(Key, bool pressed)>;
    using MouseButtonCallback = std::function<void(MouseButton, bool pressed)>;
    using MouseMoveCallback = std::function<void(int x, int y, int deltaX, int deltaY)>;

    void setKeyCallback(KeyCallback callback) { key_callback_ = callback; }
    void setMouseButtonCallback(MouseButtonCallback callback) { mouse_button_callback_ = callback; }
    void setMouseMoveCallback(MouseMoveCallback callback) { mouse_move_callback_ = callback; }

private:
    std::array<bool, static_cast<size_t>(Key::Count)> current_keys_;
    std::array<bool, static_cast<size_t>(Key::Count)> previous_keys_;

    std::array<bool, static_cast<size_t>(MouseButton::Count)> current_mouse_buttons_;
    std::array<bool, static_cast<size_t>(MouseButton::Count)> previous_mouse_buttons_;

    MousePosition mouse_position_;
    MousePosition previous_mouse_position_;

    KeyCallback key_callback_;
    MouseButtonCallback mouse_button_callback_;
    MouseMoveCallback mouse_move_callback_;
};

} // namespace kera