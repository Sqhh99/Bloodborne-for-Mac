// SPDX-License-Identifier: GPL-2.0-or-later
#include "bbport_mouse.h"

#include <array>
#include <cmath>
#include <time.h>
#include "bbport_overlay.h"
#include "bbport_settings.h"
#include "bbport_split.h"

namespace BbMouse {
namespace {

// The speed is the motion of the last SpeedWindow, so it does not depend on how often the game
// samples its pad, and falls to zero once the mouse stops.
constexpr u64 SpeedWindow = 16'000'000; // ns: respond / stop within about one 60 Hz frame

struct Motion {
    u64 ns;
    float dx, dy;
};
std::array<Motion, 256> motions{};
size_t next_motion = 0;
bool captured = false;
u32 buttons = 0;
float wheel = 0.0f; // scrolled since the last whole step (trackpads scroll in fractions)
double total_x = 0.0, total_y = 0.0;
BbGpuMouseSample input{};

u64 MonotonicNs() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return u64(ts.tv_sec) * 1'000'000'000 + u64(ts.tv_nsec);
}
// A cursor left still over the game hides after a moment (#93); moving or clicking shows it.
// (While captured, relative mouse mode hides it anyway.)
constexpr u64 IdleCursorNs = 3'000'000'000;
u64 last_pointer_ns = 0;
bool cursor_hidden = false;

void SetCursorHidden(bool hide) {
    if (cursor_hidden != hide) {
        cursor_hidden = hide;
        hide ? SDL_HideCursor() : SDL_ShowCursor();
    }
}

void Publish(BbGpuShared* s, float vx, float vy) {
    __atomic_store_n(&s->mouse_vx, s32(std::lround(vx)), __ATOMIC_RELAXED);
    __atomic_store_n(&s->mouse_vy, s32(std::lround(vy)), __ATOMIC_RELAXED);
    __atomic_store_n(&s->mouse_buttons, buttons, __ATOMIC_RELAXED);
    __atomic_store_n(&s->mouse_captured, captured ? 1u : 0u, __ATOMIC_RELEASE);
}

void SetCaptured(SDL_Window* window, BbGpuShared* s, bool on) {
    if (captured == on) {
        return;
    }
    SDL_SetWindowRelativeMouseMode(window, on);
    captured = on;
    buttons = 0;
    wheel = 0.0f;
    total_x = total_y = 0.0;
    ++input.epoch;
    input.captured = on;
    input.x = input.y = 0;
    input.motion_ns = 0;
    bbgpu_mouse_publish(&s->mouse_input, &input);
    motions.fill({});
    Publish(s, 0.0f, 0.0f);
}

} // namespace

void ConfigureInput() {
    SDL_SetHint(SDL_HINT_MAC_SCROLL_MOMENTUM, "0");
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, "0");
}

bool HandleEvent(SDL_Window* window, const SDL_Event& event) {
    BbGpuShared* s = BbSplit::Host() ? BbSplit::Shared() : nullptr;
    if (!s) {
        return false;
    }
    if (event.type == SDL_EVENT_MOUSE_MOTION || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
        event.type == SDL_EVENT_MOUSE_WHEEL) {
        last_pointer_ns = SDL_GetTicksNS();
        SetCursorHidden(false);
    }
    switch (event.type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (!captured) {
            // The click that captures the cursor is not also an attack.
            if (event.button.button == SDL_BUTTON_LEFT && BbSettings::Get().mouse_look &&
                !BbOverlay::CapturesInput()) {
                SetCaptured(window, s, true);
                return true;
            }
            return false;
        }
        buttons |= SDL_BUTTON_MASK(event.button.button);
        return true;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (!captured) {
            return false;
        }
        buttons &= ~SDL_BUTTON_MASK(event.button.button);
        return true;
    case SDL_EVENT_MOUSE_MOTION:
        if (!captured) {
            return false;
        }
        motions[next_motion++ % motions.size()] = {event.motion.timestamp, event.motion.xrel,
                                                   event.motion.yrel};
        {
            const auto& settings = BbSettings::Get();
            const double dx = double(event.motion.xrel) * settings.mouse_sensitivity;
            const double dy = double(event.motion.yrel) * settings.mouse_sensitivity *
                              settings.mouse_vertical * (settings.mouse_invert_y ? -1.0 : 1.0);
            if (std::isfinite(dx) && std::isfinite(dy)) {
                total_x += dx;
                total_y += dy;
                input.x = std::llround(total_x * BBGPU_MOUSE_POINT_SCALE);
                input.y = std::llround(total_y * BBGPU_MOUSE_POINT_SCALE);
                input.motion_ns = MonotonicNs();
                bbgpu_mouse_publish(&s->mouse_input, &input);
            }
        }
        return true;
    case SDL_EVENT_MOUSE_WHEEL: {
        if (!captured) {
            return false;
        }
        // Whole steps in the direction the wheel turned, whatever "natural scrolling" says.
        const float delta = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y;
        if (!std::isfinite(delta)) return true;
        if (delta * wheel < 0) wheel = 0; // reversing cannot inherit a fraction of the old direction
        wheel += delta;
        if (std::abs(wheel) >= 1.0f) {
            input.wheel_direction = wheel > 0 ? 1 : -1;
            ++input.wheel_serial; // a large accelerated OS event is one gesture, never a backlog
            input.wheel_ns = MonotonicNs();
            bbgpu_mouse_publish(&s->mouse_input, &input);
            __atomic_add_fetch(wheel > 0 ? &s->mouse_wheel_up : &s->mouse_wheel_down, 1u, __ATOMIC_RELAXED);
            wheel = std::fmod(wheel, 1.0f);
        }
        return true;
    }
    case SDL_EVENT_KEY_DOWN:
        // Cmd gives the cursor back (it also starts Cmd+, and Cmd+Tab); the key still reaches the
        // game's key state.
        if (captured && (event.key.key == SDLK_LGUI || event.key.key == SDLK_RGUI)) {
            SetCaptured(window, s, false);
        }
        return false;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        SetCaptured(window, s, false);
        return false;
    default:
        return false;
    }
}

void Update(SDL_Window* window) {
    BbGpuShared* s = BbSplit::Host() ? BbSplit::Shared() : nullptr;
    if (!s) {
        return;
    }
    const auto& settings = BbSettings::Get();
    if (captured && (!settings.mouse_look || BbOverlay::CapturesInput() ||
                     !(SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS))) {
        SetCaptured(window, s, false);
    }
    const u64 now = SDL_GetTicksNS();
    // The menu and the text dialog keep it (the menu consumes the mouse's events).
    SetCursorHidden(!captured && !BbOverlay::CapturesInput() &&
                    now - last_pointer_ns > IdleCursorNs);
    if (!captured) {
        return;
    }
    float dx = 0.0f, dy = 0.0f;
    for (const Motion& m : motions) {
        if (m.ns && m.ns <= now && now - m.ns < SpeedWindow) {
            dx += m.dx;
            dy += m.dy;
        }
    }
    const float scale = settings.mouse_sensitivity * (1e9f / float(SpeedWindow));
    Publish(s, dx * scale,
            dy * scale * settings.mouse_vertical * (settings.mouse_invert_y ? -1.0f : 1.0f));
}

} // namespace BbMouse
