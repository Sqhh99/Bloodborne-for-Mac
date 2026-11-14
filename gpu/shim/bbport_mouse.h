// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <SDL3/SDL.h>

/// Mouse look in the GPU host's window (split mode): when it is on (mouse_look, off by default),
/// a click captures the cursor. Displacement goes to the native free camera; speed is retained
/// for the stick fallback. Buttons and bounded wheel gestures use the pad bindings.
namespace BbMouse {

/// Before SDL video initialization: no scroll momentum or system relative-motion acceleration.
void ConfigureInput();

/// Window thread, for an event the settings overlay passed on. True if it was consumed.
bool HandleEvent(SDL_Window* window, const SDL_Event& event);

/// Window thread, after each event pump: releases the cursor when it must, publishes the speed.
void Update(SDL_Window* window);

} // namespace BbMouse
