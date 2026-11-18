# Releases

Every version of Bloodborne for Mac is one GitHub release, tag `v<version>`, built by CI from
the tagged commit, with **one file**: `Bloodborne-macOS-<version>.zip`, the app. It carries the
online module (`Contents/Frameworks/libbbnet.dylib`; online play stays off until the player
turns it on on the launcher's **Online** tab, [ONLINE.md](ONLINE.md)) and the PKG unpacker
(`Contents/Resources/tools/pkg_extract`, [USAGE.md](USAGE.md)). The app includes the online
module's GPL-3.0-or-later code ([ONLINE_PROVENANCE.md](ONLINE_PROVENANCE.md)).

History: 0.1.0 was two releases (`v0.1.0-singleplayer`, `v0.1.0-multiplayer`); 0.1.1 had a
single-player and a multiplayer zip; 0.1.2 had the app, the same zip again as
`-multiplayer.zip` for 0.1.1's update check, and `pkg_extract-macOS.zip`. From 0.1.3 the
release is the one zip.

## Updates

The launcher's update check (`launcher/Model/Update.swift`) reads this repository's releases
(tags `v<numbers>[-<suffix>]`) and downloads `Bloodborne-macOS-<version>.zip`; a release without
it is not offered. A version with a suffix (`0.1.3-test1`) or a GitHub pre-release is a test
build, offered only with **Include test builds**. Upstream's `mac-v*` tags and 0.1.0's
per-edition tags are ignored. The downloaded app must have the same bundle identifier and the
release's version before it replaces the installed one. Apps older than 0.1.2 cannot find
these releases (their check looks for edition names): download once by hand.

An app without the online module (0.1.0/0.1.1's single-player edition, a checkout built
without it) hides the Online tab, and run.sh exports `BB_ONLINE=0` there, so `online=1` in a
shared bbport.ini is ignored.

## Workflows

- **`.github/workflows/build.yml`** runs on pushes to `master`, on pull requests and by hand.
  It builds the dependencies, the port with the online module and the launcher, runs the
  launcher's tests (required) and the port's tests (informational, and not for releases: the
  runner is a VM whose GPU cannot start the renderer), then packages the app with
  `scripts/macos/package_app.sh <version>`. It checks that the zip has the online module, the
  icon and pkg_extract, and uploads it as an artifact.
- **Caches:** `deps/` (keyed on `bootstrap_deps.sh`, an hour to build from scratch) and
  ccache's objects for the CMake builds (the GPU library for x86-64 and arm64, the online
  module), restored from the previous run and saved under each commit. `ccache --show-stats`
  in the Build step's log shows the hit rate.
- **`.github/workflows/release.yml`** calls `build.yml` with the version, then creates the
  release `v<version>` (Latest) with the zip. A version with a suffix is published as
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

Two icons, kept apart (owner, 2026-11-18):
- **The launcher (the app) always has its own icon**, `Resources/Bloodborne.icns`, an original
  drawing by `scripts/macos/make_icon.sh` (no game artwork is ever shipped): in Finder,
  Launchpad, the Dock and window thumbnails, before and while it runs. A checkout's
  `Bloodborne.app` (`make_app.sh`) uses the same icon.
- **The game window** (`bbgpu-host`, its own Dock tile while playing) shows the game's
  `sce_sys/icon0.png` from the player's game folder, never the app's icon: run.sh passes it as
  `BB_GAME_ICON` and `gpu/shim/bbport_app_icon.mm` sets it before SDL makes the process a
  regular app, so its tile starts with it.

0.1.2 and 0.1.3 set the game's artwork as the bundle's custom icon (its `Icon\r` file); from
0.1.4 the launcher removes such a custom icon at start (`LauncherModel.removeGameIconFromBundle`)
and re-registers the app with LaunchServices.
