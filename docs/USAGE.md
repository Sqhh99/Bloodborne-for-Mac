# Installing and using the port

The downloadable app, preparing the game folder, launching from a checkout, saves, mods, the
benchmark, logs and the in-game menus. Moved here from the README.

## Download (no building needed)

Ready-made builds are attached to the [releases](https://github.com/Wooloomooloo2/bloodborne_mac_port/releases)
as `Bloodborne-macOS-<version>.zip` (about 50 MB). It needs an Apple Silicon Mac with macOS 15
or newer and **at least 16 GB of memory**. You still need **your own game dump** (see below): no
game files are included.

1. Unzip it and move **`Bloodborne.app`** to Applications (or anywhere).
2. **Release it from quarantine.** The app is not signed with an Apple developer certificate,
   so macOS blocks downloaded copies ("Apple could not verify…"). Either run, in Terminal:

   ```sh
   xattr -dr com.apple.quarantine /Applications/Bloodborne.app
   ```

   or open it once, then go to **System Settings → Privacy & Security** and click
   **Open Anyway** next to the Bloodborne message.
3. Open it. The launcher window opens: under **Locations**, choose your game folder (the
   `CUSA03173` folder containing `eboot.bin`); it is checked, and **Play** is enabled once it
   passes. Pick your settings and press Play. If Rosetta 2 is missing, it offers to install it.

The app contains everything else (the game process, the native GPU process, the libraries and
a small Python for the launch scripts). Saves and settings go to
`~/Library/Application Support/Bloodborne` (another save folder can be chosen in the launcher),
the logs to `~/Library/Logs/Bloodborne` (see [Logs](#logs)). To change the game folder, use the launcher's
**Locations**. The launcher opens every time you open the app; press **Play** to start the game.

**Updates.** The first time it opens, the app asks whether to check for updates automatically.
If you agree, the launcher checks the GitHub releases once a day when its window opens and
offers a newer version with its release notes. **Install
Update** downloads it, checks it (size, SHA-256 from GitHub, bundle identifier, version, code
signature), replaces `Bloodborne.app` where it is and opens it again; saves and settings are not
touched. **Bloodborne → Check for Updates…** checks at once; the update window has the
*Check for updates automatically* and *Include test builds* (pre-releases and `mac-v…-test1`
tags) options. The
app must be somewhere you can write (Applications is fine) and not run from the Downloads quarantine
location. **Bloodborne → About Bloodborne** shows the version, build and commit.

**Preparing the game folder from PKG files.** The app needs an unpacked folder, not `.pkg`
files. If you already play on shadPS4 you have one: use that folder, but first copy the
contents of the update folder next to it (`CUSA03173-patch` or similar) into it, replacing
files. To unpack decrypted (fake-signed) PKGs, download `pkg_extract-macOS.zip` from the same
release, unzip it and, in Terminal from that folder:

```sh
xattr -d com.apple.quarantine pkg_extract
./pkg_extract /path/to/base-game.pkg ~/Bloodborne        # creates ~/Bloodborne/CUSA03173
./pkg_extract /path/to/update-1.09.pkg ~/Bloodborne-update
ditto ~/Bloodborne-update/CUSA03173 ~/Bloodborne/CUSA03173   # merge the update over the game
```

Retail PKGs (from the PlayStation Store or a disc install) are encrypted with console keys and
must be dumped on a jailbroken PS4 first; no Mac tool can unpack them.

**The game files must be the original 1.09 executable.** A pre-patched `eboot.bin` (for
example a 60 FPS patch made for jailbroken consoles) is rejected: this port applies its own
60 FPS patches in memory at start, so use the unmodified `eboot.bin` from the 1.09 update.

## Launching

### As a Mac app (no Terminal window)

Once, in Terminal from the repository folder:

```sh
bash scripts/macos/make_app.sh
```

This creates **`Bloodborne.app`** in the repository folder, with the game's own icon (taken from
your game folder, so the app is made on your Mac and never uploaded). Double-click it, or drag
it to the Dock. It opens the same launcher window as the downloadable app (settings, game and
save folders; see 0.4.0 in the [release history](CHANGELOG.md)), runs the game from this checkout without a Terminal
window, and writes the game's logs to `~/Library/Logs/Bloodborne` (see [Logs](#logs)). If the
game stops with an error, the launcher offers a crash report and the log. Run the
script again if you move the repository folder. Its update check only tells you about new releases: update a
checkout with `git pull` and build again.

### From Finder with a Terminal window

Double-click **`Bloodborne.command`** in the repository folder. It opens a Terminal window and
starts the game, with its log in that window.

- **First launch:** it asks for your game folder (the `CUSA03173` folder containing
  `eboot.bin`) and remembers it in `launch.conf` next to it. If the game folder sits next to the
  repository (`../CUSA03173`), it is found without asking.
- **Keep the Terminal window open** while you play: closing it ends the game. The game's log
  appears there, and if the game exits with an error the window stays open so you can read it.
- macOS may ask you to allow Terminal to access the folder the game is in (Desktop, Documents,
  an external drive): allow it.
- To put it in the Dock or on the Desktop, ⌥⌘-drag `Bloodborne.command` there to make an alias.
- If macOS refuses to open it ("cannot be opened because it is from an unidentified
  developer"; this happens if the repository was downloaded as a zip rather than cloned),
  right-click it, choose **Open**, then **Open** again. If it says you lack permission, run
  `chmod +x Bloodborne.command` once.

`launch.conf` holds shell assignments and is not committed. Besides the game folder it can set
any other `run.sh` variable, such as `BB_DATA_DIR` (below):

```sh
BB_GAME_DIR=/Users/you/Bloodborne/CUSA03173
```

### Saves and settings

Saves, settings (`bbport.ini`) and shader caches live in
**`~/Library/Application Support/Bloodborne`**. The **`Saves`** shortcut in the repository folder
opens the save folder in Finder; your character is in `1/CUSA00207/SPRJ0005/` (the PS4's own
save layout, so saves can be copied to and from the Linux and Windows ports). Quit the game
before copying saves.

To keep them elsewhere, set `BB_DATA_DIR=/some/folder` in `launch.conf` (or the environment).
A checkout that already has saves in its own folder (`user/`, from before this default) keeps
using them; move `user/` and `bbport.ini` to the folder above to switch.

### Mods and cheats

**Mods & Cheats…** at the bottom left of the launcher opens the mod manager. The text next to the
button shows what's on ("No mods or cheats", "2 mods · 1 cheat on").

- **Mods** are folders in `~/Library/Application Support/Bloodborne/mods` (**Open Mods Folder**).
  A mod can hold `dvdroot_ps4/`, or the game's own top-level folders (`chr/`, `parts/`, ...),
  optionally inside one wrapper folder. **Add Mod…** takes a folder or a .zip and checks its layout
  straight away.
  - Mods later in the list win when two replace the same file. The **Conflicts** list shows each
    case.
  - Each launch gets a private merged view of the game made of links. Your game folder and the
    mod folders are never written to.
  - Replacements for `eboot.bin`, `sce_module` and `sce_sys` are refused.
  - The order and switches are saved in `mods.json`. [docs/MODS.md](MODS.md) has the folder
    rules.
- **Cheats & Tweaks** are patches from the community patch file (`patches/Bloodborne.xml`), each
  with its own note. Pairs that do opposite things switch each other off: rally decay or no rally,
  lighter or heavier corpse physics. The choices are stored as `cheats=` in `bbport.ini`. *Enemy
  Control* is not offered: it rewrites the same code as the frame-rate patches.
- **Patch Files** lists `.xml` patch files you put in `~/Library/Application
  Support/Bloodborne/patches`, each with an on/off switch.
- **Save backups:** before the first launch with a set of mods or cheats it hasn't seen,
  **Play** copies your save folder to `save-backups/<date and time>/`, keeping the newest five.
  Other folders in `save-backups` are never touched.

### Benchmark

**Benchmark…** in the launcher starts the game twice with its own save and settings, about 6
minutes in all. Your save and settings are not used or changed, and mods, patch files and cheats
are off, so results from different Macs compare like for like.

1. **Traversal:** a recorded walk through Central Yharnam at 1080p, capped at 60 FPS. New areas
   stream in and new shaders compile on the way, so this pass shows traversal stutter.
2. **Throughput:** 30 seconds standing still, uncapped, for the highest frame rate the Mac
   reaches.

The results window shows, for each pass:

- average and best FPS, 1% and 0.1% lows, the longest frame;
- frames over 33, 50 and 100 ms, and milliseconds lost to hitches per minute;
- the worst hitches with their time on the route and likely cause;
- GPU busy time, shader compiles, the game's CPU use and memory, swap, memory pressure and thermal
  state.

Each run is kept in `~/Library/Logs/Bloodborne/Benchmarks/<date and time>/`:

- `report.md` (the same text, ready to paste into an issue) and `summary.json`;
- a per-frame CSV for each pass, `system.csv`, and each pass's game log.

Please run it when you are reporting a performance problem. The benchmark's own save (a character
in Central Yharnam with the route cleared of enemies) and the recorded route are in `benchmark/`
and inside the app; your own saves are never used.
[docs/DESIGN_BENCHMARK.md](DESIGN_BENCHMARK.md) has the details.

### Logs

Each game session started from the launcher writes two logs to **`~/Library/Logs/Bloodborne`**,
named by the time it started. The last 20 sessions are kept, at most 256 MB in all, and one
session's logs stop at 64 MB (a normal session is well under 1 MB): past that only its last
1,000 lines are written, when the game ends, after a line saying how many were left out.

- `game-2026-10-22_14-03-05.log`: the game's output, each line with its time and a syslog
  severity (`CRIT`, `ERROR`, `WARN`, `NOTICE`, `INFO`, `DEBUG`):

  ```text
  2026-10-22T14:03:05.869+03:00 NOTICE launcher: Bloodborne for Mac 0.5.4 on Mac15,8 (Apple M3 Max), 64 GB, ...
  2026-10-22T14:03:06.284+03:00 WARN   GPU [Render.Vulkan] <Warning> vk_pipeline_serialization.cpp:369 ...
  2026-10-22T14:09:41.604+03:00 CRIT   Crash: the GPU process crashed (signal 11 in ...)
  ```

- `game-2026-10-22_14-03-05.jsonl`: the same lines as JSON, one object per line, for Grafana Loki
  (Alloy/Promtail), Elasticsearch (Filebeat), Vector and similar: `@timestamp` (UTC),
  `log.level`, `syslog.severity.code`, `log.logger` (who wrote the line: `runtime`, `bbgpu-host`,
  `gpu.render.vulkan`, ...), `message`, `session.id` and `service.version`.

`game.log` and `game.previous.log` link to the last two sessions' `.log` files. Warnings and
errors also go to the macOS system log (unified logging), with the line's source as its category:
open **Console.app** and search for `bloodborne-mac-port`, or in Terminal:

```sh
/usr/bin/log stream --predicate 'subsystem == "io.github.wooloomooloo2.bloodborne-mac-port"'
```

The game never depends on the logs: if the launcher quits or a log can't be written (a full
disk), the game keeps running and only those lines are lost.

The severity is read from the line itself: `Crash:` lines are critical, `STOP:` and `ERROR:` lines
errors, the GPU library's own levels (`<Warning>`, `<Error>`) are kept, and lines saying something
failed are warnings. Your home folder appears as `~` in the system log and in crash reports.

**Crash reports.** When the game stops with an error, the launcher shows what happened and, by
itself, puts a diagnostics zip in `~/Library/Logs/Bloodborne/Reports` (the last 10 are kept): the
crash report, the session's `.log` and `.jsonl`, `bbport.ini`, `launch.conf`, and macOS's own
crash reports (`.ips`) of the game's processes from that session. Their exception and crashed
thread also go into the report. Your home folder is shown as `~`, and the identifiers macOS puts
in `.ips` files (crash reporter key, sleep/wake and boot session ids) are removed. Other paths
stay as they are: a game folder on an external drive shows as `/Volumes/<drive name>/...`, so
check the report if a drive or folder name is private. **Report on
GitHub** opens a new issue with the report already filled in (copied to the clipboard as well,
for reports too long for the link) and shows the zip in Finder to drag onto the issue. Nothing is
sent until you submit the issue.

### Typing text

Text the game asks for (the character's name, the network password under *Network Settings*)
is typed on the keyboard: a box over the frame shows the prompt and the text (as dots for the
password), Backspace deletes, Enter confirms, Esc cancels and leaves the field as it was. The
field gets exactly what was typed; the game's own length limit applies (the log line
`ImeDialog opened (..., up to N chars)` shows it).

### From Terminal

```sh
cd bloodborne_mac_port
BB_GAME_DIR=/path/to/CUSA03173 bash run.sh
```

`run.sh` prepares the game image and rebuilds anything whose sources changed. It uses the same
saves and settings as the Finder launcher.

### In the game

- **System → Mac Settings:** one native game-menu entry for renderer, quality, effects and
  launch settings. Use Left/Right on **Page**, or L1/R1 (keyboard: F / left click), to change groups.
  Changes save to the same `bbport.ini`; `*` marks changes requiring a restart. On the final
  page, enable **Confirm restart**, then **Restart now**. Native ultrawide remains a launcher
  setting. The menu uses English captions in every supported game language.
  Its assets are generated privately from your game files after user mods; originals stay
  untouched. Set `BB_MAC_SETTINGS=0` to disable it. Unsupported assets/code leave it disabled.
- **Exit Game:** the last entry in the native title menu closes the game and returns to
  the launcher when launched from it. All six menu choices are visible, including in the
  DLC title menus. The in-world System menu's save-and-return-to-title action is unchanged.
  Set `BB_EXIT_GAME=0` to disable this entry independently of Mac Settings.
- **Settings overlay:** `⌘,` (or L3 + R3 on a controller): FPS, resolution, upscaler (MetalFX,
  FSR 3.1 or off), shader compile mode. Some changes need a restart. The launcher shows the
  same settings before the game starts.
- **Full screen:** ⌃⌘F or the green window button.
- **Input:** game controllers (through SDL), the keyboard, and the mouse for the camera (turn on
  **Mouse camera**). The keyboard's defaults are Dark Souls III's PC layout (the table under
  "What this fork changes" in the [README](../README.md)). The launcher's **Controls** page rebinds keys, mouse buttons and the
  wheel (up to four per action, chords such as ⇧ + click) and sets the mouse speed; changes apply
  at the next start. Scroll up selects the previous equipped quick item; scroll down selects
  the next, wrapping around and skipping empty slots. Shift + scroll up/down switches the
  right/left weapon. The **Previous quick item** action can also be rebound in Controls.
- **First launch and new areas** compile shaders, so the first minutes stutter a little; later
  sessions load them from the cache at start. The first launch after a rebuild takes ~20 s to
  open the window.
