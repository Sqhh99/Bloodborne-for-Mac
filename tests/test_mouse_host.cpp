// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise the real SDL event handler without a window: raw displacement is never averaged,
// scroll acceleration is coalesced, and capture loss resets the motion epoch.
#include <cassert>
#include <atomic>
#include <thread>
#include "../gpu/shim/bbport_mouse.cpp"

static BbGpuShared block{};
namespace BbSplit {
Mode GetMode() { return Mode::Host; }
BbGpuShared* Shared() { return &block; }
}
namespace BbSettings {
Values& Get() { static Values value; return value; }
}
namespace BbOverlay {
bool CapturesInput() { return false; }
}
static void Reset() {
    BbMouse::captured=true;
    BbMouse::total_x=BbMouse::total_y=0;
    BbMouse::wheel=0;
    BbMouse::input={}; BbMouse::input.captured=1; BbMouse::input.epoch=1;
    block.mouse_wheel_up=block.mouse_wheel_down=0;
    bbgpu_mouse_publish(&block.mouse_input,&BbMouse::input);
}
static void Motion(float dx,float dy) {
    SDL_Event event{};
    event.type=SDL_EVENT_MOUSE_MOTION;
    event.motion.timestamp=SDL_GetTicksNS(); event.motion.xrel=dx; event.motion.yrel=dy;
    assert(BbMouse::HandleEvent(nullptr,event));
}
static BbGpuMouseSample Read() {
    BbGpuMouseSample sample{};
    assert(bbgpu_mouse_read(&block.mouse_input,&sample));
    return sample;
}
int main() {
    BbMouse::ConfigureInput();
    assert(!SDL_GetHintBoolean(SDL_HINT_MAC_SCROLL_MOMENTUM,true));
    assert(!SDL_GetHintBoolean(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE,true));
    auto& settings=BbSettings::Get();
    settings.mouse_sensitivity=2; settings.mouse_vertical=0.5f; settings.mouse_invert_y=true;
    Reset(); Motion(100,50);
    const auto fast=Read();
    assert(fast.x==int64_t(200*BBGPU_MOUSE_POINT_SCALE) && fast.y==int64_t(-50*BBGPU_MOUSE_POINT_SCALE));
    Reset(); for (int i=0;i<100;++i) Motion(1,0.5f);
    const auto slow=Read();
    assert(fast.x==slow.x && fast.y==slow.y); // all 100 events survive between game frames
    Motion(NAN,1); assert(Read().x==slow.x);
    SDL_Event scroll{}; scroll.type=SDL_EVENT_MOUSE_WHEEL; scroll.wheel.y=10000;
    assert(BbMouse::HandleEvent(nullptr,scroll));
    assert(Read().wheel_serial==1 && Read().wheel_direction==1 && block.mouse_wheel_up==1);
    scroll.wheel.y=1; scroll.wheel.direction=SDL_MOUSEWHEEL_FLIPPED;
    assert(BbMouse::HandleEvent(nullptr,scroll));
    assert(Read().wheel_serial==2 && Read().wheel_direction==-1);
    scroll.wheel.direction=SDL_MOUSEWHEEL_NORMAL; scroll.wheel.y=0.25f;
    for (int i=0;i<4;++i) assert(BbMouse::HandleEvent(nullptr,scroll));
    assert(Read().wheel_serial==3 && Read().wheel_direction==1);
    SDL_Event focus{}; focus.type=SDL_EVENT_WINDOW_FOCUS_LOST;
    BbMouse::HandleEvent(nullptr,focus);
    assert(!Read().captured && Read().epoch==2 && Read().x==0 && Read().y==0);

    // A reader on another CPU cannot observe half of a position update.
    BbGpuMouseInput transport{};
    std::atomic<bool> done{false};
    std::thread writer([&] {
        BbGpuMouseSample sample{};
        for (int i=1;i<=100000;++i) {
            sample.x=i*2; sample.y=i; sample.wheel_serial=i;
            bbgpu_mouse_publish(&transport,&sample);
        }
        done.store(true,std::memory_order_release);
    });
    do {
        BbGpuMouseSample sample;
        if (bbgpu_mouse_read(&transport,&sample)) {
            assert(sample.x==sample.y*2 && sample.wheel_serial==sample.y);
        }
    } while (!done.load(std::memory_order_acquire));
    writer.join();
    std::puts("PASS: host mouse distance, wheel normalization, capture epoch and concurrent publication");
}
