// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise the real SDL event handler without a window: raw displacement is never averaged,
// scroll acceleration is coalesced, and capture loss resets the motion epoch.
#include <cassert>
#include <atomic>
#include <thread>
#include "../gpu/shim/bbport_mouse.cpp"

static BbGpuShared block{};
static unsigned cursor_changes;
static bool cursor_hidden;
namespace BbCursor {
void SetHiddenUntilMouseMoves(bool hidden) { ++cursor_changes; cursor_hidden=hidden; }
}
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
static void TestButtonSnapshots() {
    bool keys[BBGPU_INPUT_KEYS]{};
    keys[SDL_SCANCODE_W]=keys[SDL_SCANCODE_LSHIFT]=true;
    BbMouse::captured=true; BbMouse::buttons=SDL_BUTTON_LMASK;
    BbMouse::PublishButtons(&block,keys,BBGPU_INPUT_KEYS,true);
    BbGpuButtonSample sample{};
    assert(bbgpu_buttons_read(&block.button_input,&sample));
    assert(sample.keys[SDL_SCANCODE_W] && sample.keys[SDL_SCANCODE_LSHIFT]);
    assert(sample.mouse_buttons==SDL_BUTTON_LMASK && sample.mouse_captured);
    // A short keyboard array is zero-padded; unfocused input is completely neutral.
    BbMouse::PublishButtons(&block,keys,1,true);
    assert(bbgpu_buttons_read(&block.button_input,&sample));
    assert(!sample.keys[SDL_SCANCODE_W] && !sample.keys[SDL_SCANCODE_LSHIFT]);
    BbMouse::PublishButtons(&block,keys,BBGPU_INPUT_KEYS,false);
    assert(bbgpu_buttons_read(&block.button_input,&sample));
    assert(!sample.keys[SDL_SCANCODE_W] && !sample.mouse_buttons && !sample.mouse_captured);

    BbGpuButtonInput transport{};
    sample.mouse_buttons=123;
    assert(!bbgpu_buttons_read(&transport,&sample) && sample.mouse_buttons==123); // not published
    std::atomic<bool> done{false};
    std::thread writer([&] {
        for (unsigned i=1;i<=20000;++i) {
            BbGpuButtonSample next{};
            const uint8_t bit=i&1;
            for (auto& key : next.keys) key=bit;
            next.mouse_buttons=bit ? SDL_BUTTON_LMASK : 0;
            next.mouse_captured=bit;
            bbgpu_buttons_publish(&transport,&next);
        }
        done.store(true,std::memory_order_release);
    });
    do {
        if (bbgpu_buttons_read(&transport,&sample)) {
            for (const auto key : sample.keys) assert(key==sample.mouse_captured);
            assert(sample.mouse_buttons==(sample.mouse_captured ? SDL_BUTTON_LMASK : 0));
        }
    } while (!done.load(std::memory_order_acquire));
    writer.join();
    assert(bbgpu_buttons_read(&transport,&sample) && !sample.mouse_captured && !sample.mouse_buttons);
    BbMouse::captured=false; BbMouse::buttons=0;
}
int main() {
    TestButtonSnapshots();
    BbMouse::SetCursorHidden(true);
    assert(cursor_hidden && cursor_changes==1);
    BbMouse::SetCursorHidden(true);
    assert(cursor_changes==1);
    for (const auto event_type : {SDL_EVENT_WINDOW_FOCUS_GAINED, SDL_EVENT_WINDOW_MOUSE_ENTER, SDL_EVENT_WINDOW_SHOWN}) {
        BbMouse::SetCursorHidden(true);
        SDL_Event shown{}; shown.type=event_type;
        BbMouse::HandleEvent(nullptr,shown);
        assert(!cursor_hidden && !BbMouse::cursor_hidden);
    }
    BbMouse::SetCursorHidden(true);
    BbMouse::last_pointer_ns=0;
    BbMouse::Update(nullptr); // no focused window: never keep a global cursor hidden
    assert(!cursor_hidden);
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
    // Shift + wheel: macOS sends a wheel mouse's notch sideways (SDL x = -deltaX), still up / down.
    SDL_MouseWheelEvent sideways{}; sideways.x=-1;
    assert(BbMouse::WheelDelta(sideways,true)==1 && BbMouse::WheelDelta(sideways,false)==0);
    sideways.x=1; sideways.direction=SDL_MOUSEWHEEL_FLIPPED; // natural scrolling: both inverted
    assert(BbMouse::WheelDelta(sideways,true)==1);
    sideways.x=-1; sideways.y=-2; sideways.direction=SDL_MOUSEWHEEL_NORMAL;
    assert(BbMouse::WheelDelta(sideways,true)==-2); // a vertical step wins over a sideways one
    SDL_SetModState(SDL_KMOD_LSHIFT);
    scroll.wheel.y=0; scroll.wheel.x=1; // Shift + wheel down
    assert(BbMouse::HandleEvent(nullptr,scroll));
    assert(Read().wheel_serial==4 && Read().wheel_direction==-1);
    SDL_SetModState(SDL_KMOD_NONE);
    assert(BbMouse::HandleEvent(nullptr,scroll) && Read().wheel_serial==4); // sideways alone: nothing
    scroll.wheel.x=0;
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

    // A game menu is open: the cursor stays free, a click is the menu's (never the capture that
    // gives the mouse to the camera, never an attack), the right button and the wheel count.
    settings.mouse_look=true;
    assert(!BbMouse::captured);
    __atomic_store_n(&block.menu_pointer,1u,__ATOMIC_RELEASE);
    const auto pointer=[] {
        BbGpuPointerSample sample{};
        assert(bbgpu_pointer_read(&block.pointer,&sample));
        return sample;
    };
    SDL_Event button{}; button.type=SDL_EVENT_MOUSE_BUTTON_DOWN; button.button.button=SDL_BUTTON_LEFT;
    assert(BbMouse::HandleEvent(nullptr,button) && !BbMouse::captured && !BbMouse::buttons);
    assert(pointer().click_serial==1 && pointer().left_held==1 && pointer().click_ns!=0);
    button.type=SDL_EVENT_MOUSE_BUTTON_UP;
    assert(BbMouse::HandleEvent(nullptr,button) && pointer().left_held==0 && pointer().click_serial==1);
    button.type=SDL_EVENT_MOUSE_BUTTON_DOWN; button.button.button=SDL_BUTTON_RIGHT;
    assert(BbMouse::HandleEvent(nullptr,button) && pointer().back_serial==1 && pointer().click_serial==1);
    // One notch is one item: macOS's accelerated deltas (a quick flick's 3 or 40) are not more.
    BbMouse::menu_wheel=0; BbMouse::precise_until_ns=0; BbMouse::precise_item_ns=0;
    scroll.wheel.direction=SDL_MOUSEWHEEL_NORMAL; scroll.wheel.y=-3;
    assert(BbMouse::HandleEvent(nullptr,scroll) && pointer().down_serial==1 && pointer().up_serial==0);
    scroll.wheel.y=-40;
    assert(BbMouse::HandleEvent(nullptr,scroll) && pointer().down_serial==2);
    scroll.wheel.y=1;
    assert(BbMouse::HandleEvent(nullptr,scroll) && pointer().up_serial==1);
    // A trackpad (fractions): one item per 6 units (60 points), one an event, 120 ms apart,
    // nothing banked past an item.
    scroll.wheel.y=1.5f;
    for (int i=0;i<3;++i) assert(BbMouse::HandleEvent(nullptr,scroll));
    assert(pointer().up_serial==1); // 4.5 units: not yet an item
    assert(BbMouse::HandleEvent(nullptr,scroll) && pointer().up_serial==2);
    for (int i=0;i<4;++i) assert(BbMouse::HandleEvent(nullptr,scroll));
    assert(pointer().up_serial==2); // another 6 units at once: too soon after the last item
    scroll.wheel.y=30.5f;
    assert(BbMouse::HandleEvent(nullptr,scroll) && pointer().up_serial==2);
    BbMouse::precise_item_ns=0; // 120 ms later
    assert(BbMouse::HandleEvent(nullptr,scroll) && pointer().up_serial==3); // a big one: one item
    scroll.wheel.y=-1.5f; // turning back drops the travel left over
    for (int i=0;i<3;++i) assert(BbMouse::HandleEvent(nullptr,scroll));
    BbMouse::precise_item_ns=0;
    assert(BbMouse::HandleEvent(nullptr,scroll) && pointer().down_serial==3);
    const auto wheel_before=Read().wheel_serial; // the game's wheel bindings saw none of it
    assert(wheel_before==Read().wheel_serial && !BbMouse::captured);
    // The menu closed: a left click takes the mouse for the camera again.
    __atomic_store_n(&block.menu_pointer,0u,__ATOMIC_RELEASE);
    button.button.button=SDL_BUTTON_LEFT;
    assert(BbMouse::HandleEvent(nullptr,button) && pointer().click_serial==1);
    std::puts("PASS: host mouse distance, wheel normalization, capture epoch, concurrent publication, menu pointer");
}
