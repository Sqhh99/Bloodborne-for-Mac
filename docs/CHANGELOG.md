# Release history

What each release of the macOS port changed, newest first. Moved here from the README.

## New in 0.5.15: remappable controller buttons

- **Controller buttons can be remapped** in the launcher: **Controls > Controller > Controller
  Buttons…**. Click a slot and press the button you want; each action takes up to four buttons,
  including Select/Share/View, the back paddles (Xbox Elite, DualSense Edge) and the Misc button.
  **Reset to Defaults** puts everything back. Triggers keep their analog pull when bound to L2/R2.
  The settings are stored as `bind_pad_` lines in `bbport.ini`; the L3 + R3 menu shortcut always
  uses the sticks.
- **Builds on macOS 15 with Xcode 16** (#141, from [@byebyevlad](https://github.com/byebyevlad)).

## New in 0.5.14: long sessions no longer freeze

- **Fixed: the freeze after about 1.5 to 2 hours of play** (#127, #139). The game's occlusion
  queries never finished on the port: the result was written and then wiped by the game's own
  clear of the same memory, which ran later. The list of waiting queries grew every frame until a
  small per-frame memory area filled, and the game froze or crashed (the #99/#113 crashes were the
  same thing). The results now arrive in order; a 2.5-hour test session stayed at 60 FPS with
  that memory area at 3% full. No need to restart every 90 minutes any more.
- **M5 Macs on macOS 27.2: a fix for the GPU process crash about 40 seconds after start** (#129,
  confirmed by the reporter). The Metal pipeline archive keeps only render pipelines there (`BB_METAL_ARCHIVE=0` turns the archive off,
  `=render` forces render-only elsewhere). Other Macs are unchanged.

## New in 0.5.13: Mac Settings in the game's menu

- **Mac Settings in the game's own System menu** (#76, from [@evill33t](https://github.com/evill33t)):
  the renderer, quality, effects and launch settings in the game's native menu, in nine groups
  (Left/Right on **Page**, or L1/R1). It shares `bbport.ini` with the launcher and the ⌘,
  overlay; `*` marks changes that need a restart. The title menu also gets an **Exit Game**
  entry. Its menu assets are generated from your own game files; `BB_MAC_SETTINGS=0` and
  `BB_EXIT_GAME=0` turn them off.
- **Avoid white scenes (MetalFX)** (#116, #117, from @evill33t): an opt-in switch for the
  Forbidden Woods scene that turns white with MetalFX above 1080p. Off by default because it
  costs some GPU time; starting with the upscaler off and turning MetalFX on in game also avoids it.
- **A conflicting patch file no longer stops the game from starting** (#79, from @evill33t):
  a third-party patch that overlaps a built-in one is skipped with a log line, and patches can
  no longer overwrite the port's own runtime hooks.
- **A game stuck before its first frame now ends with a report** (#121): after 3 minutes on a
  black screen it stops and writes the thread list, so the launcher can show why.
- Long sessions could freeze after 1.5 to 2 hours (#127) and M5 Macs on macOS 27.2 could crash at
  start (#129): both addressed in 0.5.14.

## New in 0.5.12

- **Fixed: the crash after long sessions, again** (#113; "signal 11 at guest offset 0x216a08c",
  after about 90 minutes). 0.5.11 skipped the game's render bind for a resource that was gone,
  but the game then also released it and asked for its value. Both are now skipped as well
  (#114, from [@evill33t](https://github.com/evill33t)).
- **Fewer false "the GPU stopped making progress" exits** (#106): the GPU watchdog now also
  watches the GPU's own completion signal, so CPU-side delays no longer count as a GPU stall. A
  real GPU stall still ends the game, so it cannot freeze the Mac's display (#107, from
  @evill33t).
- **L3 + R3 can be switched off** (#104): the launcher's **Controls** page and the in-game menu
  (**Controller**) have a switch for it, for players who press both sticks by accident. ⌘, still
  opens the menu.
- **Known:** above 1920×1080 with the upscaler off, the game's own anti-aliasing doesn't reach
  the picture (#115), so edges can look sharper and more jagged than at 1080p. Until that's
  fixed, try **MetalFX** with the **Native AA** preset at those resolutions.

## New in 0.5.11: 5120×2160 fixed, a long-session crash fixed

- **5120×2160 now draws the level** (#93). In 0.5.10 the game ran out of its own graphics memory
  at this size: the city had no buildings or ground, or the game crashed while loading. It now
  gets a bigger graphics heap and about 2 GB more memory, at this resolution only (about 50 FPS
  uncapped on an M4 Max in Central Yharnam). The launcher warns on Macs with less than 32 GB.
- **Fixed: the crash in the Hunter's Dream after long sessions** (#99, #101, #102; "signal 11 at
  guest offset 0x2169edc", after about 90 minutes). The game queued a render bind for a resource
  that was gone; it now skips that one bind instead of crashing (#100, from
  [@evill33t](https://github.com/evill33t)).
- **Fewer GPU-process crashes from stale graphics data** (#98, from @evill33t): if a draw reads
  graphics data the game has already freed, the renderer reads zeros instead of crashing. If
  0.5.11 runs slower than 0.5.10 on your Mac, please open an issue with your Mac and settings.
- **Video guide:** Andrew Tsai's [guide to this port](https://www.youtube.com/watch?v=_QgmfFyZbYo).

## New in 0.5.10: rebindable controls, a crash fix, choose the display

- **Rebindable keys and mouse buttons.** The launcher has a new **Controls** page: every action
  (movement, camera, buttons, d-pad, Options, touch pad) can take keys or mouse buttons, two per
  action, and the mouse wheel can be bound too. There is a **walk** key (Left Alt by default:
  movement keys push the stick half way), a vertical mouse speed, and mouse sensitivity now goes
  up to 10. The defaults are the 0.5.9 layout, so nothing changes until you rebind something.
- **Fixed: the GPU process crash in fights** (#74; "signal 11 in StageSpecialization", seen
  fighting Ebrietas and in the Forbidden Woods). The game sometimes leaves a resource table
  empty; the port read through it and crashed. It now reads zeros there, as the PS4's GPU does.
- **Choose the display the game opens on** (#60): with a second display connected, the launcher
  shows a **Display** picker. Full screen opens there too. If that display isn't connected, the
  game opens on the main one.
- **5120×2160 (21:9)** native resolution (#93), under **Ultrawide or 16:10** in the launcher
  (it did not draw the level until 0.5.11).
- **The mouse cursor hides** after 3 seconds without moving over the game window (#93), and
  comes back as soon as you move or click.
- **Updates no longer leave the old launcher open** (#96): after **Install Update**, the old
  copy stayed on "Installing…" beside the new one. This is fixed in 0.5.10's updater, so it
  takes effect from the *next* update: when updating from 0.5.9, the old window can still stay
  open once. Quit it with ⌥⌘Esc (Force Quit); the update itself is installed.

## New in 0.5.9: 16-18 GB Macs run at full speed

- **16-18 GB Macs no longer slow down to 10-20 FPS** (#69, #83). The cause was memory, not the
  GPU: the port kept 6.5 GB of GPU memory pinned (it can never be swapped out), and on a 16 or 18
  GB Mac that left too little for the game and macOS, so the Mac swapped and the frame rate
  collapsed. On Macs with 24 GB or less, the game's memory is now mapped to the GPU only where the
  game actually uses it (3.5 GB pinned instead of 6.5 GB). This is
  [@MojahedAbushawish](https://github.com/MojahedAbushawish)'s work in #85 and #86.
  - On a Mac limited to behave like an 18 GB Mac: 15-17 FPS before, 58-60 after (1080p, 60 cap).
  - Players: an M4 MacBook Air 16 GB went from 11-16 FPS to 57-60 (#83), and the M3 Pro 18 GB
    from #69 plays smoothly.
  - Macs with more memory keep the previous layout, unchanged. `BB_ARENA_LAYOUT=legacy` in
    `launch.conf` brings it back on a smaller Mac, if ever needed.
- If you added `BB_ARENA_GIB` or `BB_GC_BUDGET_MB` to `launch.conf` while testing, remove them.
- Thank you to @lifeos-creator, @danielsirbubusiness and @khhan0703 for testing the test build on
  their Macs.

## New in 0.5.8: the opening-movie freeze and crash fixed

- **New games no longer freeze or crash in the opening movie** (#46, #50, #51, #24, #84). Both
  showed up on slower or busier Macs, which is why they were hard to reproduce on a fast one.
  - **The freeze** ("no new frame for 30 seconds"): when a movie ended and the game closed the movie
    player at the same moment, the stop request could miss a decoder thread that was waiting for a
    free frame buffer, and the game waited for it forever.
  - **The crash** (signal 11 at guest offset 0x26c01c3): during the movie, the game's main thread
    drew into the render thread's GPU context from a path the port didn't lock. It now runs under
    the same lock as the others.
  - On a Mac slowed down on purpose, 5 of 5 new games froze or crashed before the fix; 4 of 4
    reached character creation after it. Normal play and the benchmark are unchanged (60 FPS).
- **A hang report:** if the game ever stops responding, the log now lists where every game thread
  is waiting, so freeze reports can be read from `game.log` alone.
- **Co-op stability** (from [@noelmiller](https://github.com/noelmiller), #11): two renderer stops
  seen in co-op (a render-pass submit and a recording-chunk race) are now survivable.
- **Safety fixes in the PS4 runtime** (from [@evill33t](https://github.com/evill33t), #80, #81,
  #82): semaphore handles kept at full width, system-module IDs checked in full before lookup, and
  guest thread-local storage checked before the runtime is entered.

## New in 0.5.7: mouse look, a Stop button, keyboard fixes

- **Mouse look** (from [@MojahedAbushawish](https://github.com/MojahedAbushawish), #20). Turn on
  **Mouse camera** in the launcher (General → **Controls**) or in the in-game menu (⌘,).
  - Click once in the game window to turn the camera with the mouse; ⌘ gives the cursor back.
  - Left click R1, right click L2, middle click R3 (lock on), side buttons R2 and L1.
  - While Mouse camera is on, the **arrow keys are the d-pad**: they scroll menus, and in game they
    switch weapons, tools and quick items.
  - The launcher's **Controls** section lists every keyboard and mouse binding.
- **A Stop button** (from [@DiAvisoo](https://github.com/DiAvisoo), #31). While a game started with
  **Play** runs, the launcher stays behind it with a **Stop** button; quitting the launcher stops
  the game too. Exit Game in the game's own menu is still the safe way out, because it finishes
  saving.
- **The keyboard works with a controller connected.** Before, a connected controller switched the
  keyboard off, so Enter or Space could not press Play Offline.
- **⌘ shortcuts no longer reach the game:** ⌘Q drank a Blood Vial. ⌘W no longer quits the game
  (from #20).
- **A safety fix in the frame rate patches.** The community 30/60/90/Uncap FPS++ patches wrote a
  return instruction into the middle of the game's condition wait. Any timed wait reaching it
  would have crashed; it never showed in play, but now the game's own code runs there. Found by an
  assessment comparing the port with [Paleblood](https://github.com/Yharnam-Hunters/Paleblood),
  verified and fixed independently.
- **Enemy Control** (patch files only) is refused together with a frame rate patch, because the two
  change the same code.

## New in 0.5.6: mod manager and cheats, benchmark, 16:10, better crash reports

- **Mods & Cheats** (asked for in #16 by @noelmiller): a **Mods & Cheats…** button in the
  launcher opens its own window. See [Mods and cheats](USAGE.md#mods-and-cheats).
  - **Mods:** your loose-file mods in load order, with on/off switches. You can reorder them, add
    one from a folder or a .zip, or remove one (it goes to the Trash). A mod that can't load says
    why, and a conflicts list shows which mod wins each file. Game files are never changed.
  - **Cheats & Tweaks:** 12 switches for patches the game already had: can't die, enemies don't
    see or hear you, rally, physics, analog sensitivity, camera distance and auto-rotation, text
    size, all game languages.
  - **Patch Files:** on/off switches for your own shadPS4/GoldHEN `.xml` patch files.
  - Before the first launch with a new set of mods or cheats, the launcher **backs up your save**.
- **Benchmark** (in the launcher): a recorded walk through Central Yharnam at 60 FPS, then 30
  seconds uncapped, with a results window. It measures average and best FPS, 1% and 0.1% lows,
  hitches during traversal and their likely cause (shader compiles, streaming, GPU, game CPU),
  plus CPU, GPU, memory, swap and thermal state. It's meant for comparing Macs and for reports
  about slowdowns. See [Benchmark](USAGE.md#benchmark).
- **Dated session logs and fuller crash reports** (from [@satperm](https://github.com/satperm), #40):
  one timestamped log per session, plus a JSON version for log tools. Warnings go to the macOS
  system log. Crash reports include macOS's own crash files and a diagnostics zip. See [Logs](USAGE.md#logs).
- **About panel and update check** (from @satperm, #23), and the **game's own icon in the Dock**
  (from [@soulripper13](https://github.com/soulripper13), #66; made in memory from your game
  folder, never saved).
- **16:10 resolutions** (#75, asked for by @Frosty2804): 1280×800, 1920×1200 and 2560×1600 under
  **Ultrawide or 16:10** in the launcher, using Kyo's patches (correct HUD and lock-on positions).
  That is closer to the shape of MacBook displays, so full screen has thinner black bars.
- **Audio dropout diagnostics** (from [@DiAvisoo](https://github.com/DiAvisoo), #32): opt-in
  logging of the audio signal, queue and decoder, for reports of crackling or gaps.
- **Fixes:**
  - The arrow keys no longer also press the d-pad. Turning left swapped the left-hand item and
    broke a sprint (#61, reported by @IlyaDylko). IJKL are still the d-pad.
  - A GPU process stuck on hung GPU work no longer leaves the game hanging after it's killed, so
    the launcher can start the game again (#71, reported by @wgorecodes).
  - F9 pad recording (`BB_PAD_RECORD`) works again.

## New in 0.5.5: lifts, crash and upscaling fixes, no more red flashes

0.5.5 is a full release of the 0.5.4.3 to 0.5.4.5 pre-releases, plus one new fix. The first launch
rebuilds the shader cache once (a minute or so of stutter).

- **Red flashes on breath and other effects** (new in 0.5.5). In cold areas such as Cainhurst,
  the hunter's breath could flash as a bright red ball every few seconds. The PS4's GPU
  has float instructions that turn infinities and invalid values into 0 or the largest normal
  number. The shader translator turned several of them into ordinary maths, and Metal's fast maths
  removed the checks for invalid values, so one bad pixel bloomed into a red ball. Those
  instructions now follow the PS4's rules: in testing, flashes went from about 1 frame in 60 to
  fewer than 1 in 500 with the first half of the fix, and the second half targets the rest. If you
  still see one, please report where.
- **Fixed game timing by default, so lifts no longer drop you through the floor.** The port always
  advanced the game by each frame's measured time, so on uneven frames the character could sink
  through a moving lift and die. At the 60 and 90 FPS caps the game now runs in fixed steps (the
  60/90 FPS++ patches); 30 runs the game unpatched, and 40 and 120 stay variable. If a scene can't
  hold the cap, the game slows down a little instead. **Game timing** under the launcher's frame
  rate cap (`game_timing` in `bbport.ini`) switches back to variable steps.
- **The rare "memmove" crash is fixed** (#34, #37, #38, #54, #57). The game's render thread builds
  each frame's commands in a fixed 256 KB pool. It normally uses about 3% of it, but on rare frames
  it needs more, and the game then copied into a null address and crashed. When the pool runs out,
  the port now hands the game memory from a 16 MB spare pool and logs a "Frame pool: … full" line
  in `game.log`.
- **MetalFX and FSR 3.1 no longer turn hazy while moving** (#48, found and measured by
  @MojahedAbushawish). The camera motion vectors pointed the wrong way vertically on the Metal
  renderer, so the upscalers blended in history from the mirrored position.
- **Native AA works again with texture compression on**: since 0.5.4 it, and upscaling above
  1080p, silently ran without the upscaler.
- **The launcher no longer freezes on macOS 26.0.1** when switching from Advanced back to General
  (#47): General and Advanced are now a switch at the top instead of tabs, which hit a SwiftUI
  bug in that macOS version.

## New in 0.5.4.2: GPU stall crashes fixed

- **"The GPU stopped making progress" crashes, and the frame rate sinking over a session, are
  fixed** (#42, #43, #44, #45, #49, #55). The extra Metal views of GPU memory added in 0.5.4 cost
  no memory, but macOS's GPU driver counted each one as memory it had to keep in place, until it
  paged on every frame and the GPU stalled. They are gone; meshes high in GPU memory are copied to
  a small buffer instead (@DiAvisoo's original fix). Found and measured by @noelmiller.

## New in 0.5.4.1: missing trees

- **Trees and other objects that never drew are back** (#35). The Metal renderer skipped every
  tessellated draw with more than one instance, which some areas use for whole trees: in the
  Forbidden Woods, about 1,900 draws every 5 seconds were dropped. Each instance is now drawn on
  its own; 60 FPS holds there on an M4 Max.

## New in 0.5.4: the biggest speed-up since native Metal

The same spot in Hemwick Charnel Lane, M4 Max, full screen, upscaler off, frame rate uncapped,
read from Apple's Metal Performance HUD:

| Output | 0.5.3 | 0.5.4 | |
|---|---|---|---|
| 4K native (3840×2160) | 51.3 FPS, GPU 20.1 ms | **70.1 FPS, GPU 14.9 ms** | **+37%** |
| 4K native, another view | 80.7 FPS, GPU 12.6 ms | **109.1 FPS, GPU 9.3 ms** | **+35%** |

And in alternating test runs (M4 Max, uncapped; the presenter stops at 120 FPS):

| Scene | Before | After |
|---|---|---|
| Hunter's Dream, 1440p native, texture compression off → on | 101.7–102.1 | **119.6–119.8** (at the 120 limit, so at least +18%) |
| Hunter's Dream, 1440p native, invariant positions (below) | 85.7 | **98.9** (+15%) |
| M3 Pro 18 GB, compression off → on ([@MojahedAbushawish](https://github.com/MojahedAbushawish)) | 37.6 / 49.7 / 28.5 | **44.3 / 62.2 / 40.2** (+18% / +25% / +41%) |

GPU time is what decides whether a slower Mac holds 60 FPS (16.7 ms per frame), so these gains
matter most on base and Pro chips. What changed:

- **Lossless texture compression** (from [@MojahedAbushawish](https://github.com/MojahedAbushawish), #33).
  Apple GPUs compress textures and render targets without any loss, but only when they are
  created without two usage flags. Every texture had both, so nothing was compressed and memory
  bandwidth was the largest cost of a frame. Textures now get those flags only when something
  needs them. Launcher: Advanced → Performance → **Texture compression** (on; turn it off if you
  see wrong colours or broken textures).
- **Invariant positions instead of precise math in vertex shaders** (from Mojahed's #20). Positions
  stay identical between passes through Metal's invariance, without the precise-math mode that
  blocked the shader compiler's optimisations. The shader cache rebuilds once after updating.
- **Vanishing floors, walls and props fixed** (found by [@DiAvisoo](https://github.com/DiAvisoo), #36; #35, #18).
  Metal's indexed and indirect draws use only the low 32 bits of their buffer offset, on every
  Apple GPU tested (M2 Max, M4 Max): when streaming placed a large mesh more than 4 GiB into the
  game's GPU memory, the draw read the wrong data and the mesh disappeared until a reload. Such
  draws now go through a second Metal buffer over the same memory, at a low offset, with no copy.
- **Smoother full screen** (#25): three frame buffers instead of two in full screen, where macOS
  shows the game's frames directly. Late frames per test run: 10–18 → 0–2.
- **Native ultrawide** (from [@DiAvisoo](https://github.com/DiAvisoo), #30, building on Kyo's
  resolution patches): 2560×1080 and 3440×1440 (21:9), 2560×720, 3840×1080 and 5120×1440 (32:9)
  in the launcher, with the camera and HUD adjusted. 3840×1080 holds 60 FPS on an M4 Max; the
  1440-tall modes need a lot of GPU memory, and the launcher warns on Macs with less than 24 GB.
- **Optional frame pacing** (in-game menu: Display → Frame pacing): each frame stays on screen for
  one period of the FPS cap, for even motion on ProMotion displays, at the cost of about one
  frame of input latency. Off by default.
- The pipeline archive is written less often while many new shaders appear (less competition with
  the game on Macs with fewer cores).

## New in 0.5.3: fixes, crash reports

- **The black loading screen** (#18, found by Mojahed in #20) and **very low frame rates on 16 GB
  Macs** with an upscaler or at 720p (#22, #19) are fixed.
- **Crash reports:** the launcher shows what happened and opens a GitHub issue with the log.
- **Advanced tab** in the launcher (#15); +5–15% FPS from deferred copies and 10–22% less CPU
  (Mojahed's #20); MoltenVK and the Vulkan loader are gone from the app.

## Benchmarks: 0.4.0 → 0.5.0

Apple M4 Max (48 GB), frame rate uncapped, FPS while standing still (30 s), 2026-10-17/18. Each
cell is mac-v0.4.0 (MoltenVK) → mac-v0.5.0 (native Metal); "Quality" upscales 1.5× (for example
1280×720 → 1920×1080).

| Scene | 1080p native | 1440p native | 1440p MetalFX Quality | 1440p TAA | 1080p FSR 3.1 Quality | 1440p FSR 3.1 Quality |
|---|---|---|---|---|---|---|
| Central Yharnam, first lamp | 97.5 → **97.7** | 66.2 → **75.1** | 95.1 → **95.7** | 61.9 → **69.9** | 117.0 → **119.5** | 95.6 → **98.0** |
| Cathedral Ward | 103.2 → **104.8** | 72.0 → **82.7** | 99.5 → **102.3** | 67.0 → **78.3** | 122.0 → **126.6** | 98.8 → **108.6** |
| Upper Cathedral Ward | 78.5 → **83.2** | 57.2 → **65.2** | 76.7 → **81.0** | 54.4 → **61.7** | 92.0 → **97.7** | 75.7 → **84.8** |
| Hunter's Dream, headstone | 98.0 → **104.3** | 69.5 → **82.0** | 92.6 → **100.9** | 64.9 → **76.1** | 108.0 → **120.2** | 92.6 → **99.0** |

0.5.0 is ahead in every case, by **12–18% at 1440p**. With the 60 FPS cap both hold a steady 60
(every frame 16.7 ms). Uncapped, 0.5.0 occasionally has a single longer frame (20–40 ms) that
0.4.0 does not; it does not occur with a frame cap (backlog).

## From 22 to 120 FPS

The first playable build (2026-10-11) ran the first level at **~22 FPS** at native 1080p. What
got it to 60 and beyond, in order:

| Version | Change | Effect |
|---|---|---|
| 0.3.0 | **MetalFX temporal upscaling** (via `VK_EXT_metal_objects`) alongside FSR 3.1 | Render at 720p, upscale to 1080p: 22 → ~30 FPS at the time |
| 0.3.0 | **Native arm64 GPU process.** The game still runs as x86-64 under Rosetta 2, but all rendering moved into a separate native `bbgpu-host` process sharing the game's memory (Mach memory entry, ring buffers, cross-process page-protection relays) | MoltenVK's Vulkan→Metal encoding costs **1.75× less CPU** natively; now the default |
| 0.3.0 | **Tiny render area for passes with no output.** ~20 vertex-only passes per frame were tiled over a 16384×16384 area | 27.5 → 33.9 FPS in the city |
| 0.3.0 | **Upload hoisting.** Buffer uploads recorded ahead of the submission instead of splitting render passes | 33.9 → 34.8 FPS, fewer G-buffer pass splits |
| 0.3.0 | **Pipeline cache kept between sessions.** It was wiped every launch (~500 shader compiles each session; the "stutter when attacking") | 2 compiles per session instead of ~500 |
| 0.3.0 | **No "precise" float math in pixel/compute shaders.** SPIR-V `NoContraction` became `[[clang::optnone]]` helper calls for every float operation in Metal (223 of 308 shaders) | City respawn spot **39 → 60 FPS**, GPU 19.6 → 12.1 ms per frame |
| 0.3.0 | Shader permutation fix (Metal rejected ~6 pipelines per session, their draws vanished); unnamed pooled command buffers (MoltenVK labelled every encoder, ~4% CPU) | Correct image; less CPU per frame |
| 0.3.0 | **Shaders compile on background threads** (4 low-priority threads). Option `shader_compile=wait` (brief hitch, complete image), `skip` (no hitch, an object may pop in) or `balanced` (default: waits up to 6 ms per frame, then skips) in `bbport.ini` and the in-game menu | Level load with an empty cache: render thread compile time 440 → 130 ms; worst frame 248 → 76 ms with `skip`. Walking into new areas with slow (cold) compiles: 15 frames frozen 120–190 ms with `wait`, one 41 ms frame with `balanced` |
| 0.3.1 | **MoltenVK encodes while we record** (prefilled Metal command buffers) instead of encoding the whole frame inside the flip's submit (~13 ms) | Judder: late game frames **~6× fewer**; the "solid 60" feel |
| 0.3.3 | **No Metal argument buffers in MoltenVK** (push descriptors bound directly; the tip came from [Huax-1839/bloodborne-macos](https://github.com/Huax-1839/bloodborne-macos)) | **+13–16%** everywhere: 1080p native 82 → 94, 1440p native 59 → 66 (above 60 for the first time), FSR/MetalFX 92 → 107 |
| 0.5.0 | **Native Metal renderer.** The video core encodes Metal directly (no Vulkan, no MoltenVK): untracked guest buffers with our own ordering, a pipeline archive kept between sessions, MetalFX on our own textures, FSR 3.1 through a Metal back end for AMD's FidelityFX interface | **1440p native 69.5 → 82.0**, FSR 3.1 Quality 1080p **108 → 120** (Hunter's Dream; table above) |
| 0.5.3 | **Copies inside a render pass wait until it ends** (Mojahed, #20) | +5–7% at 1440p, up to +15% at 1080p |
| 0.5.4 | **Invariant positions** instead of precise vertex math (Mojahed, #20) | Hunter's Dream 1440p **85.7 → 98.9** |
| 0.5.4 | **Lossless texture compression** (Mojahed, #33) | Hunter's Dream 1440p **102 → 120** (cap); 4K native in Hemwick **51 → 70** together with the line above |

Also fixed along the way: a MoltenVK crash from two threads sharing one command pool, and
geometry corruption at 1440p (scenes above 1080p now use live resolution changes).

## New in 0.5.2: 16 GB Macs, controller menu

- **Less GPU memory, for 16 GB Macs** (#19). The game's GPU memory block is now 6 GB on every
  Mac, sized to what the game actually uses (it was 8 GB on 16 GB Macs and up to 16 GB on larger
  ones), and textures are budgeted around it. On 16 GB Macs the game sat right at macOS's GPU
  memory limit, which can make it very slow. With a 16 GB Mac's limits simulated on an M4 Max,
  peak GPU memory drops from ~11.5 GB to 9.6 GB at a steady 60 FPS.
- **A possible fix for the black loading screen** (#18): the order of the game's compute
  submissions could leave the GPU waiting on work that was never queued.
- **The in-game menu works with a controller again.** L3+R3 opens it, the D-pad and
  Cross/Circle move around it, and the D-pad moves sliders such as Sharpness.
- **The launcher can be turned back on from the game**: the in-game menu's Display tab has
  "Show the launcher when Bloodborne opens".

## New in 0.5.1: fixes

- **macOS 27: the GPU process works again** (thanks to [Noel Miller](https://github.com/noelmiller)).
  On macOS 27 the system's memory allocator takes addresses in the range the GPU process needs
  before the program starts, so it fell back to running the game in one process. A second build
  of the GPU process (`bbgpu-host-macos27`) reserves the range when macOS loads it; the game
  starts it on macOS 27 and later only (earlier versions keep the existing one).
- **Typing your character's name works in full screen** (thanks to Noel Miller): a box over the
  game shows the prompt and what you type. Backspace deletes, Enter confirms, Esc cancels; the
  name is exactly what you typed (an empty or cancelled entry no longer becomes "Hunter").
  Password fields show dots.
- **A game folder without the game's data is refused with an explanation** instead of crashing
  (exit code 139): the 1.09 update on its own is not enough, the folder needs the whole base game
  with the update copied over it.
- The log's `GPU arena:` line says what decided the arena's size.

## New in 0.5.0: native Metal

- **A native Metal renderer replaces MoltenVK.** Faster in every scene and setting measured
  (above), most at 1440p. MetalFX, FSR 3.1 (AMD's FidelityFX code on a Metal back end; its output
  matches the Vulkan version bit for bit on test frames), TAA, FSR 1 (`BB_FSR1=1`), the settings
  overlay and frame capture all run on it.
- **Memory leak fixed (game process).** The game process grew about **4.5 GB per hour** of play
  (2.5 → 6.2 GB in a 45-minute walk): freed small blocks of the runtime's allocator were filed
  under the wrong size class. It now stays flat at about 2.5 GB.
- **Macs with less memory** (thanks to [Noel Miller](https://github.com/noelmiller)): the GPU
  memory arena is sized to the Mac (a quarter of its memory, 8 GiB minimum, 16 GiB at most);
  a 16 GiB arena left the GPU waiting on a 36 GB Mac (~7 FPS). A rare freeze after long
  sessions (two threads recording GPU commands at once) is fixed, touchpad touches behave like a
  DualShock 4's, and a missing GPU hint no longer stops the game.
- **Rendering fixes:** three full-screen passes that Metal refused are drawn (the light behind
  the cage bars at the first lamp; the figure in the cage is back).
- **Fallback:** if the GPU process cannot start, the game runs in one process on Metal (60 FPS
  here).

## New in 0.4.0: the launcher

`Bloodborne.app` now opens a launcher window (native SwiftUI) before the game, like a Steam
launcher, with every setting on one screen (no scrolling, even on a 13" MacBook Air):

- **Display:** output resolution, windowed or full screen, frame rate cap (30, 40, 60, 90, 120).
- **Upscaler:** off, MetalFX or FSR 3.1, its quality level, and the resolution the game
  actually renders at (for example 1280 × 720 → 1920 × 1080); FSR sharpening.
- **Effects:** depth of field, motion blur, chromatic aberration, ambient occlusion, shadows
  from dynamic lights, screen-space reflections, the game's own anti-aliasing, the FPS counter,
  and **skip intro videos** (straight to the title screen, no Sony and FromSoftware logos).
- **Locations:** the game folder (checked before Play is enabled) and the save folder (it
  offers to copy your saves to a new folder; the originals stay where they were).
- **Show launcher every time:** untick it to start the game directly next time; hold **⌥
  (Option)** while opening the app to get the window back.

Settings are shared with the in-game overlay (`⌘,`) and stored in `bbport.ini`. When the game
quits, the launcher comes back; if it stopped with an error, the launcher says so and offers
the log.

## Earlier changes (0.3.x, 2026-10-14)

- **Load crash fixed.** The intermittent crash while loading an area (the GPU window closing
  during a loading screen) was a race in the game itself: the main thread records
  cloth/texture compute jobs into the render thread's command stream without a lock, and the
  render thread's next loading-screen frame could start in the middle of them. On a PS4 the
  main thread is done before the next vsync; under Rosetta it can take ~7 ms longer, more when
  the Mac is busy. The port now hooks both game functions and makes them take turns (the main
  thread waits well under a millisecond); in 15 test loads the race was caught 10 times and
  never corrupted anything. As a safety net, corrupted GPU commands are logged and skipped
  instead of ending the game.
- **Sharper on Retina displays:** the window uses the display's real pixels (full screen on a
  MacBook was rendered at half resolution and stretched).
- **Full screen works** (⌃⌘F or the green window button). It used to freeze the picture.
- **Lower display latency:** two swapchain images instead of three, so the time from the
  game presenting a frame to it reaching the screen went from 48.5 to 31.7 ms.
- **Game file reads no longer fail** when they land in memory the GPU is watching. macOS
  failed such reads outright, so the game could lose an asset load; a possible cause of the
  missing-geometry reports below.
- **Shader compile mode `balanced`** is the default: new shaders compile in the background
  and a frame waits at most 6 ms for them, so new areas no longer freeze for 100+ ms.
- **Less judder:** late game frames (the cause of judder at a steady 60) dropped about 6x with
  the upscaler off or FSR: MoltenVK now encodes each frame's Metal commands while they are
  recorded instead of all at once at the flip, which the game was waiting behind.
- **Diagnostics:** `BB_FRAME_STATS=1` prints per-stage timings of every late frame ("Spike"
  lines, with each game thread's CPU time), and corrupted GPU command streams are reported
  with the game threads and code involved.
