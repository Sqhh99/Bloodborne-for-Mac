// SPDX-License-Identifier: GPL-2.0-or-later
#include "bbport_mouse.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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

// The game's own menus (src/runtime_menu_pointer.c): while one is open the cursor is free and
// points at its items. Where it is goes to the game in the stage's pixels (1920x1080, centred in
// the frame shown and scaled by its height), with its clicks and wheel steps.
BbGpuPointerSample pointer{};
bool released_for_pointer = false; // the camera's mouse was let go for a menu: taken back after

// The wheel in a menu selects: one notch is one item, never more, however fast it turns. macOS
// accelerates a wheel mouse's notches (a quick flick arrives as a delta of 3, 5, 10: SDL rounds
// them up to whole numbers, SDL_cocoamouse.m); that is for scrolling pages, and here it skipped
// over items, so the size of a whole-number delta is ignored. A trackpad or Magic Mouse sends its
// pixels / 10 in fractions: one item per MenuPreciseStep of them (60 points of travel), at most
// one an event and MenuPreciseGapNs apart, nothing banked past an item, so its own acceleration
// cannot run through a list either. A fraction seen in the last second marks the device precise.
constexpr float MenuPreciseStep = 6.0f;
constexpr u64 MenuPreciseGapNs = 120'000'000;
constexpr u64 PreciseStickyNs = 1'000'000'000;
float menu_wheel = 0.0f; // precise travel since the last item
u64 precise_until_ns = 0, precise_item_ns = 0;

void MenuWheel(BbGpuShared* s, float delta) {
    const u64 now = MonotonicNs();
    if (delta == 0.0f) {
        return;
    }
    if (delta != std::trunc(delta)) {
        precise_until_ns = now + PreciseStickyNs;
    }
    const bool precise = now < precise_until_ns;
    int item = 0;
    if (!precise) {
        item = delta > 0 ? 1 : -1;
    } else {
        if (delta * menu_wheel < 0) {
            menu_wheel = 0; // turning back starts afresh
        }
        menu_wheel += delta;
        if (std::abs(menu_wheel) >= MenuPreciseStep) {
            if (now - precise_item_ns >= MenuPreciseGapNs) {
                item = menu_wheel > 0 ? 1 : -1;
                precise_item_ns = now;
            }
            menu_wheel = 0;
        }
    }
    static const bool log = [] {
        const char* e = std::getenv("BB_MENU_POINTER_LOG");
        return e && e[0] == '1';
    }();
    if (log) {
        std::printf("Menu wheel: delta %.3f (%s) -> %d\n", delta, precise ? "precise" : "notch", item);
    }
    if (!item) {
        return;
    }
    ++(item > 0 ? pointer.up_serial : pointer.down_serial);
    bbgpu_pointer_publish(&s->pointer, &pointer);
}

bool PointerWanted(const BbGpuShared* s) {
    return __atomic_load_n(&s->menu_pointer, __ATOMIC_ACQUIRE) != 0 && !BbOverlay::CapturesInput();
}

void SetPointer(SDL_Window* window, BbGpuShared* s, float wx, float wy) {
    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);
    if (w <= 0 || h <= 0) {
        return;
    }
    const auto& settings = BbSettings::Get();
    const float aspect = settings.ultrawide_width > 0 && settings.ultrawide_height > 0
                             ? float(settings.ultrawide_width) / float(settings.ultrawide_height)
                             : 16.0f / 9.0f;
    const float frame_h = std::min(float(h), float(w) / aspect);
    const float scale = frame_h / 1080.0f;
    pointer.x = (wx - float(w) * 0.5f) / scale + 960.0f;
    pointer.y = (wy - float(h) * 0.5f) / scale + 540.0f;
    pointer.inside = 1;
    bbgpu_pointer_publish(&s->pointer, &pointer);
}

void SetCaptured(SDL_Window* window, BbGpuShared* s, bool on) {
    if (captured == on) {
        return;
    }
    SDL_SetWindowRelativeMouseMode(window, on);
    captured = on;
    buttons = 0;
    wheel = 0.0f;
    menu_wheel = 0.0f;
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
    // A game menu is open: the mouse points and clicks there (never the camera, never an attack).
    if (!captured && PointerWanted(s)) {
        switch (event.type) {
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            SetPointer(window, s, event.button.x, event.button.y);
            if (event.button.button == SDL_BUTTON_LEFT) {
                pointer.left_held = 1;
                ++pointer.click_serial;
                pointer.click_ns = MonotonicNs();
                // Out of the menu the mouse turns the camera again without another click.
                released_for_pointer = BbSettings::Get().mouse_look;
            } else if (event.button.button == SDL_BUTTON_RIGHT) {
                ++pointer.back_serial;
            }
            bbgpu_pointer_publish(&s->pointer, &pointer);
            return true;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_LEFT) {
                pointer.left_held = 0;
                bbgpu_pointer_publish(&s->pointer, &pointer);
            }
            return true;
        case SDL_EVENT_MOUSE_WHEEL: {
            const float delta = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y;
            if (std::isfinite(delta)) MenuWheel(s, delta);
            return true;
        }
        default:
            break;
        }
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
            if (event.button.button == SDL_BUTTON_LEFT && pointer.left_held) {
                pointer.left_held = 0;
                bbgpu_pointer_publish(&s->pointer, &pointer);
            }
            return false;
        }
        buttons &= ~SDL_BUTTON_MASK(event.button.button);
        return true;
    case SDL_EVENT_MOUSE_MOTION:
        if (!captured) {
            SetPointer(window, s, event.motion.x, event.motion.y);
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
        [[fallthrough]];
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        pointer.inside = 0;
        pointer.left_held = 0;
        bbgpu_pointer_publish(&s->pointer, &pointer);
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
    // The game's menus: the camera's mouse is let go for the pointer, which starts in the window's
    // middle (not wherever the camera's mouse was left), and is taken back when they close.
    const bool focused = (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0;
    if (PointerWanted(s)) {
        if (captured) {
            SetCaptured(window, s, false);
            released_for_pointer = true;
            int w = 0, h = 0;
            SDL_GetWindowSize(window, &w, &h);
            SDL_WarpMouseInWindow(window, float(w) * 0.5f, float(h) * 0.5f);
            SetPointer(window, s, float(w) * 0.5f, float(h) * 0.5f);
            last_pointer_ns = SDL_GetTicksNS();
            SetCursorHidden(false);
        }
    } else if (released_for_pointer && !BbOverlay::CapturesInput()) {
        released_for_pointer = false;
        if (settings.mouse_look && focused && !captured) {
            SetCaptured(window, s, true);
        }
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
