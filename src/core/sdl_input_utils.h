// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/core/input.h"

#include <SDL3/SDL_keycode.h>

namespace kera
{

    Key sdlKeyToKey(SDL_Keycode sdlKey);

}  // namespace kera
