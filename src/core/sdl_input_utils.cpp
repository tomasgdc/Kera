// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "sdl_input_utils.h"

namespace kera
{

    EKey sdlKeyToKey(SDL_Keycode sdl_key)
    {
        switch (sdl_key)
        {
            case SDLK_A:
                return EKey::A;
            case SDLK_B:
                return EKey::B;
            case SDLK_C:
                return EKey::C;
            case SDLK_D:
                return EKey::D;
            case SDLK_E:
                return EKey::E;
            case SDLK_F:
                return EKey::F;
            case SDLK_G:
                return EKey::G;
            case SDLK_H:
                return EKey::H;
            case SDLK_I:
                return EKey::I;
            case SDLK_J:
                return EKey::J;
            case SDLK_K:
                return EKey::K;
            case SDLK_L:
                return EKey::L;
            case SDLK_M:
                return EKey::M;
            case SDLK_N:
                return EKey::N;
            case SDLK_O:
                return EKey::O;
            case SDLK_P:
                return EKey::P;
            case SDLK_Q:
                return EKey::Q;
            case SDLK_R:
                return EKey::R;
            case SDLK_S:
                return EKey::S;
            case SDLK_T:
                return EKey::T;
            case SDLK_U:
                return EKey::U;
            case SDLK_V:
                return EKey::V;
            case SDLK_W:
                return EKey::W;
            case SDLK_X:
                return EKey::X;
            case SDLK_Y:
                return EKey::Y;
            case SDLK_Z:
                return EKey::Z;
            case SDLK_0:
                return EKey::NUM0;
            case SDLK_1:
                return EKey::NUM1;
            case SDLK_2:
                return EKey::NUM2;
            case SDLK_3:
                return EKey::NUM3;
            case SDLK_4:
                return EKey::NUM4;
            case SDLK_5:
                return EKey::NUM5;
            case SDLK_6:
                return EKey::NUM6;
            case SDLK_7:
                return EKey::NUM7;
            case SDLK_8:
                return EKey::NUM8;
            case SDLK_9:
                return EKey::NUM9;
            case SDLK_ESCAPE:
                return EKey::ESCAPE;
            case SDLK_SPACE:
                return EKey::SPACE;
            case SDLK_RETURN:
                return EKey::ENTER;
            case SDLK_TAB:
                return EKey::TAB;
            case SDLK_BACKSPACE:
                return EKey::BACKSPACE;
            case SDLK_LEFT:
                return EKey::LEFT;
            case SDLK_RIGHT:
                return EKey::RIGHT;
            case SDLK_UP:
                return EKey::UP;
            case SDLK_DOWN:
                return EKey::DOWN;
            case SDLK_F1:
                return EKey::F1;
            case SDLK_F2:
                return EKey::F2;
            case SDLK_F3:
                return EKey::F3;
            case SDLK_F4:
                return EKey::F4;
            case SDLK_F5:
                return EKey::F5;
            case SDLK_F6:
                return EKey::F6;
            case SDLK_F7:
                return EKey::F7;
            case SDLK_F8:
                return EKey::F8;
            case SDLK_F9:
                return EKey::F9;
            case SDLK_F10:
                return EKey::F10;
            case SDLK_F11:
                return EKey::F11;
            case SDLK_F12:
                return EKey::F12;
            case SDLK_LSHIFT:
                return EKey::LEFT_SHIFT;
            case SDLK_RSHIFT:
                return EKey::RIGHT_SHIFT;
            case SDLK_LCTRL:
                return EKey::LEFT_CTRL;
            case SDLK_RCTRL:
                return EKey::RIGHT_CTRL;
            case SDLK_LALT:
                return EKey::LEFT_ALT;
            case SDLK_RALT:
                return EKey::RIGHT_ALT;
            default:
                return EKey::UNKNOWN;
        }
    }

}  // namespace kera
