# Keyboard and mouse: bindings, layout, mouse speed

Owner decision 2026-10-27: full keyboard and mouse support with rebinding, on a separate launcher
page; fix the mouse camera feeling too slow (player feedback). Two-step plan for the mouse:
retune the stick emulation first, then a time-boxed spike on direct camera control.

## How input reaches the game

The game reads a DualShock 4 (`scePadReadState`, `src/runtime_pad.c`). Gamepads are polled by
SDL in bb-probe; the window, its keyboard and the mouse are in bbgpu-host, which publishes them
in the shared block (`host/bbgpu_shared.h`). `button_input` contains the keyboard and mouse
buttons from one completed event pump; `mouse_input` carries camera motion and wheel events.
Keyboard and mouse add to the gamepad's state.

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
  uses 1 2 3 4 and arrows. Shift + wheel up/down changes the right/left weapon. The plain wheel
  selects the previous/next quick item (2026-11-15, owner choice). In the game's menus the plain
  wheel still scrolls (menu pointer). Blood bullets remain 3 / Up, off the wheel to avoid health loss.
  Mouse left R1, Shift + left / back R2, Ctrl / right / Shift + right L2, forward L1. V keeps
  vials separate from Q lock-on and R quick items; F transforms instead of changing the grip.
  An ini with its own `bind_` lines keeps them; only absent keys get the new defaults.
- **Menu context:** `runtime_pad_set_image` checks the 1.09 list-update signature at image
  offset 0x1acff30; the menu-flow pointer at 0x5580e38, +0xa88 menu object and +0xb11 title flag
  are kernel-read safely (layout documented by bloodborne-mac's menu pointer module). In menus
  movement bindings press only the d-pad; elsewhere they move only the stick. `bind_menu_back`
  (Q) presses Circle only in menus, suppressing R3 until release. Escape switches to Back after
  its opening press is released. Missing/unsupported images disable contextual aliases.
  Back and Confirm consume any simultaneously held movement aliases until each is released:
  closing a headstone/travel list or an item's Yes/No dialog must not send Circle/Cross plus
  a d-pad press into gameplay. Confirm uses `bind_cross` (E/Enter by default). The latch
  survives the closing animation and parent-list transitions; outside menus the held key can
  still walk immediately. A list keeps the input focus while it fades out (0.25-0.35 s), so
  movement keys pressed within 0.5 s after Back or Confirm are consumed the same way; after that, fresh
  movement presses navigate normally. `BB_PAD_LOG=1` logs what the keyboard became (buttons,
  menu/choice/back/confirm, held and consumed movement) at every change.
  **A menu, for these keys, is the title menu or a menu object with something to choose from**
  (2026-11-14): a list with several items had the input focus in the last 250 ms, which the
  list-update hook of `src/runtime_menu_pointer.c` sees. A menu object alone is not enough:
  a message with one button ("Cannot be opened from this side") is a menu object with a focused
  list of one item, and the hunter moves and acts behind it (measured at the locked gate by the
  Central Yharnam lamp: the stick walks him with the message up). With the old rule the
  movement keys pressed the d-pad there, which the message does not take: weapons switched, the
  quick item cycled, W made blood bullets. In the game's real menus the reverse holds: the
  d-pad moves the cursor and the stick still walks the hunter, so WASD must be the d-pad there.
  Without the hook (another build, `BB_MENU_POINTER=0`) only the title menu counts.
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
- **Attack timing (2026-11-18):** keyboard and mouse buttons are published together by the window
  thread after one SDL event pump (`BbMouse::PublishButtons`, `host/bbgpu_button_input.h`). They
  previously came from a 4 ms keyboard bridge and a separate mouse publication, allowing a
  click to be combined with an older W/Shift state. A bounded sequence-counter read now takes
  one complete snapshot; if the writer is busy, the pad retains the previous complete snapshot
  instead of briefly releasing movement or mixing input generations. The shared protocol is 15.
  W is a held left-stick deflection and Shift + click is held R2 (without R1). A fresh forward
  press plus R2 reaches the game in the same pad read, while an already held W stays held during
  a heavy/charged attack. There are no synthetic stick releases, jump macros or added attack
  delays. Bloodborne decides its jump-attack/charge timing; Dark Souls III's internal timing
  window has not been measured or substituted. `test_attack_inputs` checks the actual pad path,
  and `test_mouse_host.cpp` checks publication and concurrent keyboard/mouse consistency.
- `camera_keys_dpad` (default 0 since the arrow keys are the d-pad by default; it was 1): while
  the mouse has the camera, keys bound to the camera press the d-pad instead.
- **Not ported from bloodborne-mac:** its key-as-stick ramps (`key_stick`) and short-tap latch.
- **Wheel:** physical direction, fractional trackpad steps accumulated. One accelerated OS event
  is one gesture. The latest event starts a 35 ms press / 35 ms release; extra same-direction
  steps during that interval are consumed, never queued or replayed. Reversing cancels the old
  direction. Each pulse latches its bound actions, so releasing Shift cannot change its meaning.
- **Previous quick item:** `bind_item_previous` (Wheel Up) is a separate action because the game
  exposes only next-item selection on its pad. `runtime_quick_items.c` queues an input edge;
  the main-thread Flipper update (1.09 offset `0x2034770`, verified prologue) consumes it. It walks
  the six native quick slots backwards, skips empty slots, wraps, writes only the selected
  inventory index, then calls the native EquipGameData normaliser (`0x14cf920`, signature
  checked). It leaves equipped-slot order and inventory contents alone. Menus, blocked input,
  missing local player and requests older than 150 ms discard the action. Fixture tests cover
  selection, wraparound, gaps, holds and cancellation. The owner confirmed gameplay testing
  passed on 2026-11-15.
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

## The mouse in the game's menus (2026-11-14)

Ported from bloodborne-mac's `gpu/games/bloodborne_menu_pointer.cpp` (bbport 0.6) to this
port's two processes:

- **Game process, `src/runtime_menu_pointer.c`:** `runtime_hooks.c` observes the entry of the
  game's list update (0x1acff30: component, input; its 20-byte prologue moves into the stub).
  For a focused list the hook hit-tests the pointer against the list's items by calling the
  game's own functions (item provider, child lookup, bounds, world matrix: direct calls, this
  code and the game are the same x86-64 System V code), moves the game's cursor to the item
  under the pointer (hover acts on a move only, so a resting pointer does not fight the keys),
  and turns a click into the confirm button, a slider's arrow or an option's side. The pad
  thread (`runtime_menu_pointer_pad`) presses those for 50 ms each, the right button's Back and
  the wheel's up / down, and publishes `menu_pointer` in the shared block. Guest memory is read
  with the kernel's checked copy; every function's prologue is checked before the hook goes in.
- **Host window, `gpu/shim/bbport_mouse.cpp`:** while `menu_pointer` is set the cursor is let
  go (warped to the window's middle) and its position, in the game's 1920x1080 stage, its
  clicks and wheel steps go to the game in `BbGpuPointerInput` (`host/bbgpu_pointer_input.h`,
  shared block version 14). No click captures the mouse or attacks then. When the menu closes
  the mouse is taken back for the camera if it was taken before, or if a menu was clicked with
  Mouse camera on (so the title menu leads straight into mouse look).
- **The wheel in menus (2026-11-14):** the wheel selects, so one notch is one item, never more,
  with no acceleration. macOS sends a quick flick of a wheel mouse as a bigger delta (3, 5, 10);
  the host ignores the size of a whole-number delta. A trackpad or Magic Mouse (fractions) gives
  one item per 6 units (60 points), at most one an event and 120 ms apart, nothing banked past an
  item. `up_serial`/`down_serial` count the items. The pad thread queues them (at most 2, dropped
  150 ms after the last event, turning back or a click drops them: the cursor never runs on
  after the wheel) and presses the d-pad once per item by the game's pad reads: 2 reads down,
  1 up. Before, items between two 90 ms presses merged into one. Hovering waits for the whole
  scroll and 300 ms after it, however the pointer moves meanwhile (a hand's nudge or a move
  towards the item scrolled to took the cursor to whatever the list had just scrolled under the
  pointer), then comes back on a 6-pixel move or a click. `BB_MENU_POINTER_LOG=1` logs each
  wheel event in the host ("Menu wheel: delta ... -> item"). Checked at the title menu with
  `BB_MENU_POINTER_FILE` ("x y down N" adds N steps; the cursor and each step are logged).
- A menu, for the pointer: the title menu, or a list with several items that had the focus in
  the last 400 ms. Not a message with one button (see Menu context): the mouse stays the camera.
- `BB_MENU_POINTER=0` off; `BB_MENU_POINTER_FILE=<file>` ("x y [click|right|up N|down N]" in stage
  pixels, applied when the file changes) drives it in tests; `BB_MENU_POINTER_LOG=1` logs the
  signals a menu is told by at each change.

## Button prompts (2026-11-14)

The game draws its key guide with 32x32 BC7 icons of PlayStation buttons (`KG_OK`, `KG_L1`, ...
in `menu/common.tpf.dcx`); its HUD packs three of them into the atlas `MENU_Common_00091`
("Examine", "Pick up", "Open"). Ported from bloodborne-mac (bbport 0.6):

- **Game process:** `runtime_pad.c` tracks the device whose input came last (0 a PlayStation
  pad, 1 another gamepad, 2 the keyboard and mouse) and publishes what each input is on it
  (`BbGpuPrompts`: a kind and a short label per input, in two variants: a key first for key
  clusters, a mouse button first for single icons while Mouse camera is on).
  `runtime_bindings_prompt` names a binding: a plain one before a chord, an arrow key for the
  d-pad (its prompts show in menus). Circle is its real button's key (Space), which is Back in
  menus and the dodge in play alike. The path of the icon file as the game opens it (a mod's
  copy included) goes into the shared block before the renderer starts.
- **GPU host:** `gpu/shim/bbport_button_prompts.cpp` hashes the icons and the atlas from that
  file and draws the replacement icons (key caps, a mouse with the button lit, another pad's
  buttons) with stb_truetype and the embedded DejaVu Sans. `TextureCache::RefreshImage` marks a
  texture that is one of the icons or the atlas by its bytes. `Rasterizer::BindTextures` then
  binds, instead of the game's texture, one from `Metal::PromptImages`
  (`renderer_metal/mtl_prompt_images.mm`): an RGBA8 texture per icon (64 px with mips), or a
  copy of the atlas with the icons in its three cells. The copy is drawn from the game's BC7
  texture on a command queue of its own 150 ms after the game's upload (the upload is on the
  renderer's queue), so the HUD shows the game's icons for the first frames. The port's pixels
  never go into guest memory. A PlayStation pad binds the game's textures as before.
- `BB_BUTTON_PROMPTS=0` off; `BB_PROMPT_TRACE=1` lists the icons found.

## Not now

Rebinding inside the in-game menu.
