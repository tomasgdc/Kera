#pragma once

#include "kera/core/input.h"

#include <SDL3/SDL_keycode.h>

namespace kera {

Key sdlKeyToKey(SDL_Keycode sdlKey);

}  // namespace kera
