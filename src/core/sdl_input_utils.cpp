#include "sdl_input_utils.h"

namespace kera {

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

}  // namespace kera
