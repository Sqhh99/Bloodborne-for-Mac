# Releases

Every version of Bloodborne for Mac is one GitHub release, tag `v<version>`, built by CI from
the tagged commit:

| Asset | What it is |
|---|---|
| `Bloodborne-macOS-<version>.zip` | the app, with the online module (`Contents/Frameworks/libbbnet.dylib`); online play stays off until the player turns it on (launcher **Online** tab, [ONLINE.md](ONLINE.md)) |
| `Bloodborne-macOS-<version>-multiplayer.zip` | the same zip under the name 0.1.1's update check looks for (below) |
| `pkg_extract-macOS.zip` | the PKG unpacker ([USAGE.md](USAGE.md)) |

The app includes the online module's GPL-3.0-or-later code ([ONLINE_PROVENANCE.md](ONLINE_PROVENANCE.md)).

History: 0.1.0 was two releases (`v0.1.0-singleplayer`, `v0.1.0-multiplayer`); 0.1.1 was one
release with a single-player and a multiplayer zip. From 0.1.2 there is only the one app, with
no edition in its name.

## Updates

The launcher's update check (`launcher/Model/Update.swift`) reads this repository's releases
(tags `v<numbers>[-<suffix>]`) and downloads `Bloodborne-macOS-<version>.zip`; a release without
it is not offered. A version with a suffix (`0.1.3-test1`) or a GitHub pre-release is a test
build, offered only with **Include test builds**. Upstream's `mac-v*` tags and 0.1.0's
per-edition tags are ignored. The downloaded app must have the same bundle identifier and the
release's version before it replaces the installed one.

Older apps:
- **0.1.1, multiplayer:** its check looks for `Bloodborne-macOS-<version>-multiplayer.zip` and
  installs only an app whose Info.plist says `BBEdition=multiplayer`. The release workflow
  uploads that copy and package_app.sh still writes the key, so these apps update by
  themselves. Drop both once 0.1.1 is no longer in use.
- **0.1.1, single-player, and 0.1.0:** cannot find the new releases; download once by hand.

An app without the online module (0.1.0/0.1.1's single-player edition, a checkout built
without it) hides the Online tab, and run.sh exports `BB_ONLINE=0` there, so `online=1` in a
shared bbport.ini is ignored.

## Workflows

- **`.github/workflows/build.yml`** runs on pushes to `master`, on pull requests and by hand.
  It builds the dependencies, the port with the online module and the launcher, runs the
  launcher's tests (required) and the port's tests (informational, and not for releases: the
  runner is a VM whose GPU cannot start the renderer), then packages the app with
  `scripts/macos/package_app.sh <version>`. It checks that the zip has the online module and
  the icon, and uploads it with `pkg_extract-macOS.zip` as an artifact.
- **Caches:** `deps/` (keyed on `bootstrap_deps.sh`, an hour to build from scratch) and
  ccache's objects for the CMake builds (the GPU library for x86-64 and arm64, the online
  module), restored from the previous run and saved under each commit. `ccache --show-stats`
  in the Build step's log shows the hit rate.
- **`.github/workflows/release.yml`** calls `build.yml` with the version, then creates the
  release `v<version>` (Latest) with the assets above. A version with a suffix is published as
  a pre-release. Notes: install steps, then `docs/releases/v<version>.md` (or a pointer to the
  changelog).

## Publishing a version

1. Merge the release's changes into `master`, add `docs/releases/v<version>.md` and give the
   changelog a section for the version.
2. Tag the merge commit and push the tag:

   ```sh
   git switch master && git pull
   git tag v0.1.2 && git push origin v0.1.2
   ```

   Or run **Release** from the Actions tab with the version (`0.1.2`); the tag is then created
   at the chosen branch's head.
3. The run publishes `v0.1.2`. Running it again for the same version replaces the zips.

## Locally

```sh
bash scripts/macos/bootstrap_deps.sh && BB_DEPS_ARCH=arm64 bash scripts/macos/bootstrap_deps.sh
bash build_macos.sh
bash scripts/macos/package_app.sh 0.1.2     # dist/Bloodborne-macOS-0.1.2.zip
```

`package_app.sh` refuses to package without the online module.

## Icons

Release bundles carry the app's own icon, `Resources/Bloodborne.icns`, an original drawing made
by `scripts/macos/make_icon.sh`: no game artwork is ever shipped. Once the launcher has checked
the player's game folder it uses the game's own `sce_sys/icon0.png`:
- in the Dock while it runs (in memory);
- as the bundle's custom icon (`NSWorkspace.setIcon`, the bundle's `Icon\r` file, what Finder's
  Get Info sets), so Finder, Launchpad and the Dock show it before launch too. Once per
  installed copy: an update replaces the bundle and the next check sets it again;
- the game window's process (`bbgpu-host`, a second Dock icon) sets it from `BB_GAME_ICON`
  (run.sh; `gpu/shim/bbport_app_icon.mm`).

A checkout's `Bloodborne.app` (`make_app.sh`) makes its bundle icon from icon0.png when it knows
the game folder (and says so in Info.plist, `BBGameIcon=1`).

Once the bundle's own icon is the game's (a custom icon from an earlier launch, or
`BBGameIcon`), neither the launcher nor the game window sets an icon in memory: every Dock tile
(the launcher, the game window, the app when not running) shows the one registered icon. Before,
the Dock switched between the bundle's icon and an in-memory copy at launch, when the game
window opened, and when the game ended (the launcher, hidden as an accessory app while playing,
gets a new Dock tile with the bundle's icon); an in-memory icon is now set again after each
such switch (`AppDelegate.showWindow`).
