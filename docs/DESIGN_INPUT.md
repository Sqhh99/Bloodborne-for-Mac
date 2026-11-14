# Keyboard and mouse: bindings, layout, mouse speed

Owner decision 2026-10-27: full keyboard and mouse support with rebinding, on a separate launcher
page; fix the mouse camera feeling too slow (player feedback). Two-step plan for the mouse:
retune the stick emulation first, then a time-boxed spike on direct camera control.

## How input reaches the game

The game reads a DualShock 4 (`scePadReadState`, `src/runtime_pad.c`). Gamepads are polled by
SDL in bb-probe; the window, its keyboard and the mouse are in bbgpu-host, which publishes them
in the shared block (`host/bbgpu_shared.h`: `keys[512]`, `mouse_vx/vy`, `mouse_buttons`,
`mouse_captured`, `mouse_wheel_up/down`). Keyboard and mouse add to the gamepad's state.

## Bindings (done, branch `input-bindings`)

- **Actions** = pad inputs, labelled for Bloodborne: movement (4 directions + walk), camera
  (4 directions), Cross, Circle, Square, Triangle, R1, R2, L1, L2, R3, L3, d-pad (4), Options,
  touchpad left/right. `src/runtime_bindings.c` and `launcher/Model/Bindings.swift` list the same
  actions, ini keys and defaults; `tests/test_pad.c` checks they match.
- **Storage:** `bbport.ini` `bind_<action>=Name,Name` (up to 4). Names are SDL scancode names
  (key positions: other layouts keep the shape; the launcher shows the printed label via
  `UCKeyTranslate`) or `Mouse Left/Right/Middle/Back/Forward`, `Wheel Up/Down`. Absent key =
  default, empty = unbound. The launcher writes only changed actions. Cmd is reserved.
  The in-game menu's Save keeps these keys (not owned).
- **Defaults follow Dark Souls III's actual PC layout** (2026-11-13), with Bloodborne
  compatibility: WASD move, IJKL camera, Space Circle (tap dodge / hold sprint / release and
  repress while sprinting to jump), Left Alt walk, E / Return Cross, V Triangle, R / Tab Square,
  F L1, Q / middle click R3, X L3, Escape Options, G / Backspace touchpad left / right. The d-pad
  uses 1 2 3 4 and arrows. Wheel up changes the right weapon, down the quick item; Shift + wheel
  down changes the left weapon. Blood bullets remain 3 / Up, off the wheel to avoid health loss.
  Mouse left R1, Shift + left / back R2, Ctrl / right / Shift + right L2, forward L1. V keeps
  vials separate from Q lock-on and R quick items; F transforms instead of changing the grip.
  An ini with its own `bind_` lines keeps them; only absent keys get the new defaults.
- **Menu context:** `runtime_pad_set_image` checks the 1.09 list-update signature at image
  offset 0x1acff30; the menu-flow pointer at 0x5580e38, +0xa88 menu object and +0xb11 title flag
  are kernel-read safely (layout documented by bloodborne-mac's menu pointer module). In menus
  movement bindings press only the d-pad; elsewhere they move only the stick. `bind_menu_back`
  (Q) presses Circle only in menus, suppressing R3 until release. Escape switches to Back after
  its opening press is released. Missing/unsupported images disable contextual aliases.
- **Optional separate dodge, run, jump** (`bind_dodge`, `bind_sprint`, both unbound by default;
  `keyboard_circle` in `src/runtime_pad.c`):
  Circle by what it does. The dodge is a 90 ms Circle press however long the key is held (held,
  the game would run); the run holds Circle; the dodge key while running is the jump (Circle
  let go 70 ms, pressed 70 ms, let go 70 ms, held again). `bind_circle` (the plain button, for
  "back" in menus) is Space by default, passed through to the game's native tap/hold handling.
- **Chords:** `Left Shift+Mouse Left` counts while both are held, and the second input's plain
  bindings do not count meanwhile (Shift + click is R2, not R2 and R1; Shift is no longer a
  sprint binding). `Keypad +` is one key (nothing follows its plus). The launcher's capture sheet binds a
  modifier when it is let go, or the chord when another key or a mouse button is pressed with it.
- `camera_keys_dpad` (default 0 since the arrow keys are the d-pad by default; it was 1): while
  the mouse has the camera, keys bound to the camera press the d-pad instead.
- **Not ported from bloodborne-mac:** mouse pointer clicks in game menus. Its
  key-as-stick ramps (`key_stick`) and
  short-tap latch are not ported either. See MACOS_BACKLOG (Mac GUI).
- **Wheel:** physical direction, fractional trackpad steps accumulated. One accelerated OS event
  is one gesture. The latest event starts a 35 ms press / 35 ms release; extra same-direction
  steps during that interval are consumed, never queued or replayed. Reversing cancels the old
  direction. Each pulse latches its bound actions, so releasing Shift cannot change its meaning.
- Mouse buttons and the wheel reach the game only while the window holds the cursor (Mouse
  camera on, after a click), as before.
- **Launcher:** third page "Controls" (the page switcher is not a TabView, #47): Mouse
  (camera, sensitivity 0.1–10, vertical speed 0.25–4×, invert), Keyboard (camera keys as d-pad),
  then the bindings by group with two slots (a third/fourth appears when used), a capture sheet
  (any key incl. Esc/Return, mouse buttons and wheel in its box; Clear, Cancel), a warning when
  one input drives several actions, Reset to Defaults. Bindings apply at the next start.

## Mouse speed

The free camera now consumes cumulative mouse displacement, not speed. `bbport_mouse.cpp`
publishes scaled fixed-point totals (65536 units/point), capture epoch, monotonic event time and
the latest wheel event under a bounded seqlock (`host/bbgpu_mouse_input.h`, shared version 13).
Every SDL motion event contributes, even between game frames. Sensitivity / vertical ratio /
inversion are applied once, at the producer. Default sensitivity remains 2.00; saved values stay.
Before SDL video starts, scroll momentum and relative system acceleration are disabled for the
game window (`SDL_HINT_MAC_SCROLL_MOMENTUM`, `SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE`).

`src/runtime_mouse.c` validates all 16 bytes at image offset 0x143cde0, displaces the three AVX
stick-speed instructions, then adds 0.0015 radians per scaled point to xmm1's pitch/yaw. The
game retains its angle wrapping and pitch clamp; input-activity flags prevent recenter fighting
the mouse. The detour preserves the red zone, flags, GPRs and all 16 full YMM registers. This
site / activity fields follow bbport 0.6 and bloodborne-mac's documented native mouse hook.

Totals are consumed once. Menus, the overlay, capture loss and stale updates discard motion
instead of replaying it later. If the free-camera site has not run for 100 ms (lock-on,
cinematics, debug camera), the pad path keeps the stick fallback for target switching; it uses
the existing 16 ms window / 450 scaled points per second. Unsupported hook bytes or
`BB_NATIVE_MOUSE=0` also keep the fallback. The game's Camera speed affects that fallback,
not displacement-based free look.

`test_mouse_host.cpp` exercises the production event handler, fast versus slow packetization,
accelerated wheel events, focus loss and concurrent publication. `test_mouse_look.c` executes
the actual x86 detour and checks distance invariance, no replay, stale/menu input, hook guards,
flags / GPRs / upper YMM lanes. `test_pad.c` checks bounded wheel pulses and modifier latching.

## Controller buttons (2026-11-08, released in 0.5.15)

Owner decisions 2026-11-08: controller buttons can be remapped too, on the keyboard's model, and
the launcher listens for the button (a list of names fails players whose pad is not labelled
like a PlayStation one, and misses paddles).
- **Storage:** `bbport.ini` `bind_pad_<input>=Button,Button` (up to 4) for the 16 pad inputs
  (`bind_pad_cross` ... `bind_pad_dpad_right`, `bind_pad_options`, `bind_pad_touchpad`). Buttons
  are named by position as on a PlayStation pad (SDL's layout: South = `Cross`), plus `Select`
  (SDL Back: Share/Create/View), `Paddle 1`-`4` (SDL right paddle 1, right 2, left 1, left 2:
  Xbox Elite P1-P4; DualSense Edge back buttons = Paddle 1 and 3) and `Misc` (SDL Misc1: mic,
  capture, Xbox Share). Absent = default (the same name; Touchpad = `Touchpad,Select`, the
  0.5.14 behaviour), empty = unbound, unknown names logged and ignored. Sticks are not remappable.
- **Triggers:** pressed past 30/255. A trigger bound to L2/R2 keeps its analog value; any other
  button bound there presses it fully; a trigger bound elsewhere presses that input.
- `remap_buttons` (`src/runtime_pad.c`) applies it to the controller only; keyboard bindings are
  separate. The L3 + R3 menu shortcut (host side) always uses the physical sticks.
- **Launcher:** Controls > Controller > "Controller Buttons…" sheet: rows per input like the
  keyboard's; click a slot, press the button. The launcher reads the controller with Apple's
  GameController (frontmost app only) and names buttons by SDL's positions; Nintendo pads may
  differ there (GameController names A/B by label, SDL by position). `PadBindings` in
  `launcher/Model/Bindings.swift`; `tests/test_pad.c` checks its tables against the C ones and
  tests the mapping on SDL's virtual controller (`test_controller_remap`).

## Not now

Rebinding inside the in-game menu; keyboard glyphs in
the game's prompts (it only has PlayStation ones).
