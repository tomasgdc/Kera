#include "kera/core/input.h"
#include <SDL3/SDL.h>
#include <algorithm>

namespace kera {

namespace {

// Convert SDL keycode to our Key enum
Key sdlKeyToKey(SDL_Keycode sdlKey) {
    switch (sdlKey) {
        case SDLK_A: return Key::A;
        case SDLK_B: return Key::B;
        case SDLK_C: return Key::C;
        case SDLK_D: return Key::D;
        case SDLK_E: return Key::E;
        case SDLK_F: return Key::F;
        case SDLK_G: return Key::G;
        case SDLK_H: return Key::H;
        case SDLK_I: return Key::I;
        case SDLK_J: return Key::J;
        case SDLK_K: return Key::K;
        case SDLK_L: return Key::L;
        case SDLK_M: return Key::M;
        case SDLK_N: return Key::N;
        case SDLK_O: return Key::O;
        case SDLK_P: return Key::P;
        case SDLK_Q: return Key::Q;
        case SDLK_R: return Key::R;
        case SDLK_S: return Key::S;
        case SDLK_T: return Key::T;
        case SDLK_U: return Key::U;
        case SDLK_V: return Key::V;
        case SDLK_W: return Key::W;
        case SDLK_X: return Key::X;
        case SDLK_Y: return Key::Y;
        case SDLK_Z: return Key::Z;
        case SDLK_0: return Key::Num0;
        case SDLK_1: return Key::Num1;
        case SDLK_2: return Key::Num2;
        case SDLK_3: return Key::Num3;
        case SDLK_4: return Key::Num4;
        case SDLK_5: return Key::Num5;
        case SDLK_6: return Key::Num6;
        case SDLK_7: return Key::Num7;
        case SDLK_8: return Key::Num8;
        case SDLK_9: return Key::Num9;
        case SDLK_ESCAPE: return Key::Escape;
        case SDLK_SPACE: return Key::Space;
        case SDLK_RETURN: return Key::Enter;
        case SDLK_TAB: return Key::Tab;
        case SDLK_BACKSPACE: return Key::Backspace;
        case SDLK_LEFT: return Key::Left;
        case SDLK_RIGHT: return Key::Right;
        case SDLK_UP: return Key::Up;
        case SDLK_DOWN: return Key::Down;
        case SDLK_F1: return Key::F1;
        case SDLK_F2: return Key::F2;
        case SDLK_F3: return Key::F3;
        case SDLK_F4: return Key::F4;
        case SDLK_F5: return Key::F5;
        case SDLK_F6: return Key::F6;
        case SDLK_F7: return Key::F7;
        case SDLK_F8: return Key::F8;
        case SDLK_F9: return Key::F9;
        case SDLK_F10: return Key::F10;
        case SDLK_F11: return Key::F11;
        case SDLK_F12: return Key::F12;
        case SDLK_LSHIFT: return Key::LeftShift;
        case SDLK_RSHIFT: return Key::RightShift;
        case SDLK_LCTRL: return Key::LeftCtrl;
        case SDLK_RCTRL: return Key::RightCtrl;
        case SDLK_LALT: return Key::LeftAlt;
        case SDLK_RALT: return Key::RightAlt;
        default: return Key::Unknown;
    }
}

MouseButton sdlButtonToMouseButton(int sdlButton) {
    switch (sdlButton) {
        case SDL_BUTTON_LEFT: return MouseButton::Left;
        case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
        case SDL_BUTTON_RIGHT: return MouseButton::Right;
        default: return MouseButton::Left; // Default to left
    }
}

} // anonymous namespace

Input::Input()
    : current_keys_()
    , previous_keys_()
    , current_mouse_buttons_()
    , previous_mouse_buttons_()
    , mouse_position_{0, 0}
    , previous_mouse_position_{0, 0} {
}

void Input::update() {
    // Copy current state to previous
    previous_keys_ = current_keys_;
    previous_mouse_buttons_ = current_mouse_buttons_;
    previous_mouse_position_ = mouse_position_;

    // Get current keyboard state
    const bool* sdlKeys = SDL_GetKeyboardState(nullptr);
    for (size_t i = 0; i < current_keys_.size(); ++i) {
        current_keys_[i] = sdlKeys[i];
    }

    // Get current mouse state
    Uint32 mouseState = SDL_GetMouseState(&mouse_position_.x, &mouse_position_.y);
    current_mouse_buttons_[static_cast<size_t>(MouseButton::Left)] = mouseState & SDL_BUTTON_LMASK;
    current_mouse_buttons_[static_cast<size_t>(MouseButton::Middle)] = mouseState & SDL_BUTTON_MMASK;
    current_mouse_buttons_[static_cast<size_t>(MouseButton::Right)] = mouseState & SDL_BUTTON_RMASK;

    // Process events for callbacks
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP: {
                Key key = sdlKeyToKey(event.key.key);
                bool pressed = event.key.down;
                if (key_callback_) {
                    key_callback_(key, pressed);
                }
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                MouseButton button = sdlButtonToMouseButton(event.button.button);
				bool pressed = event.button.down;
                if (mouse_button_callback_) {
                    mouse_button_callback_(button, pressed);
                }
                break;
            }
            case SDL_EVENT_MOUSE_MOTION: {
                if (mouse_move_callback_) {
                    mouse_move_callback_(
                        static_cast<int>(event.motion.x),
                        static_cast<int>(event.motion.y),
                        static_cast<int>(event.motion.xrel),
                        static_cast<int>(event.motion.yrel)
                    );
                }
                break;
            }
            default:
                break;
        }
    }
}

bool Input::isKeyPressed(Key key) const {
    size_t index = static_cast<size_t>(key);
    return index < current_keys_.size() &&
           current_keys_[index] &&
           !previous_keys_[index];
}

bool Input::isKeyDown(Key key) const {
    size_t index = static_cast<size_t>(key);
    return index < current_keys_.size() && current_keys_[index];
}

bool Input::isKeyReleased(Key key) const {
    size_t index = static_cast<size_t>(key);
    return index < current_keys_.size() &&
           !current_keys_[index] &&
           previous_keys_[index];
}

bool Input::isMouseButtonPressed(MouseButton button) const {
    size_t index = static_cast<size_t>(button);
    return index < current_mouse_buttons_.size() &&
           current_mouse_buttons_[index] &&
           !previous_mouse_buttons_[index];
}

bool Input::isMouseButtonDown(MouseButton button) const {
    size_t index = static_cast<size_t>(button);
    return index < current_mouse_buttons_.size() && current_mouse_buttons_[index];
}

bool Input::isMouseButtonReleased(MouseButton button) const {
    size_t index = static_cast<size_t>(button);
    return index < current_mouse_buttons_.size() &&
           !current_mouse_buttons_[index] &&
           previous_mouse_buttons_[index];
}

MousePosition Input::getMousePosition() const {
    return mouse_position_;
}

MousePosition Input::getMouseDelta() const {
    return {
        mouse_position_.x - previous_mouse_position_.x,
        mouse_position_.y - previous_mouse_position_.y
    };
}

} // namespace kera