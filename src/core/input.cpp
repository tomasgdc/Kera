#include "kera/core/input.h"
#include "sdl_input_utils.h"

#include <SDL3/SDL.h>
#include <algorithm>

namespace kera {

namespace {

MouseButton sdlButtonToMouseButton(int sdlButton) {
    switch (sdlButton) {
        case SDL_BUTTON_LEFT: return MouseButton::Left;
        case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
        case SDL_BUTTON_RIGHT: return MouseButton::Right;
        default: return MouseButton::Left;
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
    previous_keys_ = current_keys_;
    previous_mouse_buttons_ = current_mouse_buttons_;
    previous_mouse_position_ = mouse_position_;

    const bool* sdlKeys = SDL_GetKeyboardState(nullptr);
    for (size_t i = 0; i < current_keys_.size(); ++i) {
        current_keys_[i] = sdlKeys[i];
    }

    Uint32 mouseState = SDL_GetMouseState(&mouse_position_.x, &mouse_position_.y);
    current_mouse_buttons_[static_cast<size_t>(MouseButton::Left)] = mouseState & SDL_BUTTON_LMASK;
    current_mouse_buttons_[static_cast<size_t>(MouseButton::Middle)] = mouseState & SDL_BUTTON_MMASK;
    current_mouse_buttons_[static_cast<size_t>(MouseButton::Right)] = mouseState & SDL_BUTTON_RMASK;

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
