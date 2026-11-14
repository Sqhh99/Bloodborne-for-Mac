# Bloodborne for Mac

*Bloodborne* (PS4, CUSA03173 v1.09) running on Apple Silicon Macs. The game's own x86-64 code
runs directly under **Rosetta 2**, a small HLE runtime in C replaces the PS4 system libraries,
and a **shadPS4-derived video core** draws through **native Metal** in a separate arm64 process.
It is not an emulator and contains no game code.

This repository is a fork of
[Wooloomooloo2/bloodborne_mac_port](https://github.com/Wooloomooloo2/bloodborne_mac_port)
(0.5.15) with a different keyboard layout, a launcher in three languages and a faster start.

> **No game files or copyrighted assets are included, and none will ever be distributed.**
> You must provide your own legally dumped and decrypted copy of Bloodborne (CUSA03173 v1.09).
> This project is not affiliated with or endorsed by Sony Interactive Entertainment,
> FromSoftware, or AMD.

## Status

Playable, work in progress. On an Apple M4 Max the game holds a locked 60 FPS at 1080p, 1440p
and 4K native, or upscaled with MetalFX, FSR 3.1 or TAA; 90 and 120 FPS caps are there for
ProMotion displays. It needs an Apple Silicon Mac with macOS 15 or newer and at least 16 GB of
memory. Expect occasional rendering glitches ([Known issues](#known-issues)).

- Release history: [docs/CHANGELOG.md](docs/CHANGELOG.md)
- Installing, launching, saves, mods, benchmark, logs: [docs/USAGE.md](docs/USAGE.md)
- Roadmap: [docs/ROADMAP.md](docs/ROADMAP.md) · backlog: [docs/MACOS_BACKLOG.md](docs/MACOS_BACKLOG.md)
  · measurements: [docs/FINDINGS.md](docs/FINDINGS.md)

## What this fork changes

- **Keyboard and mouse default to Dark Souls III's PC layout**, the one
  [bloodborne-mac](https://github.com/bloodborne-native/bloodborne-mac) uses. Every binding
  can still be changed on the launcher's **Controls** page.

  | | Keys |
  |---|---|
  | Move | W A S D; hold Left ⌥ to walk |
  | Camera | the mouse (turn on **Mouse camera**), or I J K L |
  | Dodge / run / jump | Space / hold Left ⇧ / Space while running |
  | Attack (R1) / charged attack (R2) | left click / ⇧ + left click, or the mouse's back button |
  | Firearm (L2) / transform weapon (L1) | right click / E, or the mouse's forward button |
  | Lock on (R3) | C or middle click |
  | Interact, confirm (Cross) | F, Return |
  | Quick item (Square) / Blood Vial (Triangle) | R or Tab / Q |
  | Switch weapons, quick items (d-pad) | 1 left hand, 2 right hand, 3 up, 4 down; the arrow keys; the wheel (up, down) |
  | Menu (Options) / gestures (touchpad) | Esc / G (right side: Delete ⌫) |
  | L3 | Left ⌃ |

  In the game's menus the arrow keys move, F or Return confirms, and Space (or Left ⇧) goes
  back. A binding can now be a chord, `bind_r2=Left Shift+Mouse Left`: in the launcher, hold the
  modifier and press the key or click. `bind_dodge` and `bind_sprint` are new actions; an older
  `bbport.ini` with its own `bind_` lines keeps them.
- **The launcher speaks English, 日本語 and 简体中文.** It follows the Mac's language; **General →
  Launcher → Language** chooses one (`BB_LANGUAGE=en|ja|zh` in `launch.conf`). The in-game
  overlay (⌘,), the game's Mac Settings page, crash reports and logs stay in English.
- **A faster start.** From a checkout, Play used to build at every start, and the freshly linked
  programs then lost macOS's and Rosetta's caches: about 4 to 6.5 s before the game's process
  did anything. `run.sh` now builds only when a source file changed (`BB_BUILD=always` /
  `never`), and keeps the prepared game image between starts (`BB_PREPARE_CACHE=0` turns that
  off; about 1 s, release app included). Measurements: `docs/FINDINGS.md`.

## Getting the game running

You need an unpacked, decrypted `CUSA03173` folder with update 1.09 merged in and the original
`eboot.bin` (a pre-patched one is rejected). [docs/USAGE.md](docs/USAGE.md) explains how to make
one from PKG files.

**Ready-made app.** Upstream publishes `Bloodborne-macOS-<version>.zip` on its
[releases page](https://github.com/Wooloomooloo2/bloodborne_mac_port/releases); those builds do
not contain this fork's changes. Unzip it, run
`xattr -dr com.apple.quarantine /Applications/Bloodborne.app`, open it and choose your game
folder under **Locations**.

**From this fork (build it).** Requirements: Rosetta 2 (`softwareupdate --install-rosetta`),
Xcode Command Line Tools, Python 3 and
`brew install cmake ninja nasm glslang pkgconf boost cryptopp`.

```sh
git clone --recursive https://github.com/Sqhh99/Bloodborne-for-Mac.git
cd Bloodborne-for-Mac
bash scripts/macos/bootstrap_deps.sh                       # x86-64 dependencies (once)
BB_DEPS_ARCH=arm64 bash scripts/macos/bootstrap_deps.sh    # arm64 dependencies (once)
bash build_macos.sh                                        # the game process and the GPU host
bash scripts/macos/make_app.sh                             # Bloodborne.app for this checkout
```

Open `Bloodborne.app` (run `make_app.sh` again if you move the folder), or start from Terminal
with `BB_GAME_DIR=/path/to/CUSA03173 bash run.sh`. Launching builds again only when a source
file changed. Saves and settings live in `~/Library/Application Support/Bloodborne`, logs in
`~/Library/Logs/Bloodborne`.

In the game: `⌘,` (or L3 + R3) opens the settings overlay, **System → Mac Settings** is the same
in the game's own menu, ⌃⌘F toggles full screen, and ⌘ gives the mouse cursor back.

## Known issues

- **Missing geometry:** floors, walls and props vanishing until a reload (Metal's 32-bit index
  offsets) are fixed in 0.5.4, and trees and other instanced tessellated objects that never drew
  (Forbidden Woods, #35) in 0.5.4.1. If something still goes missing, reports with a screenshot
  and `game.log` are welcome on #35.
- **Occasional judder at a steady 60 FPS** (some game frames take 20–45 ms): much rarer since
  2026-10-15 (about 6x fewer late frames).
- **Uncapped, an occasional single long frame** (20–40 ms) on the Metal renderer; capped play
  (60/90/120) is not affected.
- **Above 1080p, the selected item's icon in the HUD is missing** (an empty frame; #14). It is
  there at 1080p.
- **At 16:10, full-screen menus keep their 16:9 artwork** (grey bars at the sides), and the first
  moments of loading a level can glitch. The HUD itself is placed correctly. A UI mod such as
  "16x10 and 4x3 UI Fixes" (Nexus Mods) fixes the menus and loads through **Mods & Cheats…**.

## How it works

- **Native execution under Rosetta 2.** The eboot is converted offline into a flat memory
  image; PS4 libc and libSceFios2 functions are linked directly into it. There is no CPU
  emulation layer of our own — Rosetta 2 translates the x86-64 code (game + runtime) once.
  The build targets `x86_64-apple-macos` with deployment target 15.0 (Rosetta AVX2 support).
- **HLE runtime** (`src/`): kernel, memory, threads, audio, input (SDL3), save data and other
  PS4 libraries. macOS specifics: guest TLS via a reserved pthread TSD slot, guest-visible
  memory kept below 1 TiB, Mach-based thread inspection.
- **GPU** (`gpu/`): the shadPS4 video core and shader recompiler, built as
  `libbbgpu.dylib`, rendering through native Metal (`renderer_metal/`). It runs in
  `bbgpu-host`, a **native arm64 process** that maps the game's memory at the same addresses
  (`host/`, `docs/DESIGN_NATIVE_GPU_PROCESS.md`). If it cannot start, the game stops with a
  "Crash:" line that the launcher shows in a crash report.
- **Upscalers:** MetalFX temporal upscaling, FSR 3.1 (AMD FidelityFX on a Metal back end,
  `gpu/shim/ffx_metal/`) and TAA. Planned: zero-copy guest memory on unified memory.
- Inherited from upstream: high-frame-rate community patches, synthetic motion vectors and
  temporal upscaling (FSR 3.1), a two-stage multi-threaded draw pipeline, and an in-game
  settings overlay (`⌘,` or `L3 + R3`; L3 + R3 can be turned off on the Controls page).

## Repository layout

| Path | Contents |
|---|---|
| `src/` | Loader (`probe.c`) and PS4 HLE runtime |
| `gpu/` | shadPS4-derived video core with the native Metal renderer, draw pipeline, upscalers |
| `host/` | Native arm64 GPU host process (`bbgpu-host`) and the shared rings |
| `scripts/` | Eboot preparation, linking, patch compiler (`patches.py`); `scripts/macos/` dependency bootstrap |
| `patches/` | Community XML patches for Bloodborne 1.09 |
| `Bloodborne.command` | Finder launcher (runs `run.sh` in Terminal) |
| `scripts/macos/make_app.sh` | Builds `Bloodborne.app`, a launcher without a Terminal window (local, not committed) |
| `scripts/macos/package_app.sh` | Builds the self-contained `dist/Bloodborne.app` and its zip for releases (CI: `.github/workflows/macos-release.yml`) |
| `Saves` | Shortcut to the save folder (created on first launch, not committed) |
| `launcher/` | The macOS launcher window (Swift: `Model/` with tests, `App/`; built by `scripts/macos/build_launcher.sh`) |
| `tools/` | Developer utilities, including `pkg_extract` (fpkg unpacker) |
| `docs/` | macOS plan and backlog, plus technical notes inherited from upstream |

## Credits

This port stands entirely on the work of others:

- **deadinside28** — author of `bbport`, the original native Linux port of Bloodborne: the flat memory image approach, the HLE
  runtime, the two-stage Vulkan draw pipeline, synthetic motion vectors and the upscaler
  integration.
- **Mrsuss60** — [bloodborne_pc_windows_port](https://github.com/Mrsuss60/bloodborne_pc_windows_port),
  the Windows port this repository was forked from, including its stability, audio, memory
  and frame-pacing fixes (documented in that repository's `CHANGES.md`).
- **Paleblood contributors** — system-module argument validation evidence from
  [Paleblood, branch `codex/runtime-sysmodule-wire`](https://github.com/evill33t/Paleblood/tree/codex/runtime-sysmodule-wire),
  commit `f9c5e5556db987d9fb4fa60b837d8a54817a0718`; provenance and limits in
  [`docs/SYSMODULE_WIRE.md`](docs/SYSMODULE_WIRE.md).
- **[shadPS4](https://github.com/shadps4-emu/shadPS4)** project — the Vulkan video core and
  shader recompiler (`gpu/shadps4/`, GPL-2.0-or-later).
- **FireBurn** — [FSR-Vulkan](https://github.com/FireBurn/FSR-Vulkan), the native Vulkan
  FSR 3.1 / FSR 4 runtime the port used until 0.5.0; FSR 3.1 is still built from its AMD
  FidelityFX SDK sources (MIT).
- **Omar Cornut** — [Dear ImGui](https://github.com/ocornut/imgui), used for the in-game overlay (MIT).
- **[evill33t/bloodborne_mac](https://github.com/evill33t/bloodborne_mac/tree/bbacb95c3338efcd627c0edaec05ff7534e555da)**
  and **[droogie/bbhost](https://github.com/droogie/bbhost/tree/566184a74f071166299bb656c4af479e5bbe2771)** —
  selected native menu builders and asset-generation techniques, adapted to this port's
  existing settings and process bridge. [Provenance and validation](docs/NATIVE_MAC_SETTINGS.md).
- **Alex Barney (Thealexbarney)** — [LibAtrac9](https://github.com/Thealexbarney/LibAtrac9),
  AT9 audio decoding (MIT).
- **sirit** — SPIR-V emitter used by the shader recompiler (BSD-3-Clause), with Khronos
  **SPIRV-Headers** (MIT-style Khronos license).
- **Christian Rau** — [half](https://half.sourceforge.net/), IEEE 754 half-precision
  library (MIT).
- **AMD** — GCN PM4/register headers in `gpu/third_party/gcn/` (MIT-style AMD license).
- **DejaVu fonts** (Bitstream Vera license) — overlay font.
- **[MoltenVK](https://github.com/KhronosGroup/MoltenVK)** (Apache-2.0), which ran the renderer
  until 0.5.0, and the dependencies built by `scripts/macos/bootstrap_deps.sh`: SDL3,
  Vulkan-Headers, SPIRV-Cross, fmt, xxHash, miniz, FFmpeg, magic_enum, robin-map, Boost.
- **Community patch authors** — Kyo, Lance McDonald, illusion, emoose, auser1337 and others
  for the 60 FPS, camera and engine patches in `patches/Bloodborne.xml`.
- **zackcage6** — testing and bug reports for the Windows port.

Everyone who sent pull requests to the port: [docs/CONTRIBUTORS.md](docs/CONTRIBUTORS.md).
Each third-party component keeps its own license file in its directory.

## License

GNU General Public License v2.0 or later (GPL-2.0-or-later), because of the shadPS4
components. See [LICENSE](LICENSE). The native-menu adapters retain GPL-3.0-or-later;
the combined runtime containing them is GPL-3.0-or-later. See
[the retained license](LICENSES/GPL-3.0-or-later.txt).
