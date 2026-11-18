# Releases: one release, two editions

Every version of Bloodborne for Mac is published as one GitHub release, tag `v<version>`,
carrying both editions, built from the same commit by one CI run. (0.1.0 alone was published as
two releases, `v0.1.0-singleplayer` and `v0.1.0-multiplayer`.)

| | Single-player | Multiplayer |
|---|---|---|
| Download (both in release `v<version>`) | `Bloodborne-macOS-<version>-singleplayer.zip` | `Bloodborne-macOS-<version>-multiplayer.zip` |
| Online module (`Contents/Frameworks/libbbnet.dylib`) | not included | included |
| Launcher **Online** tab | hidden | shown |
| `online=1` in bbport.ini | ignored: run.sh exports `BB_ONLINE=0`, so mods, cheats and patch files stay on and no secret is read | online play ([ONLINE.md](ONLINE.md)) |
| Info.plist `BBEdition` | `singleplayer` | `multiplayer` |
| Licence of the bundle | GPL (see README) | GPL, including the online module's GPL-3.0-or-later code ([ONLINE_PROVENANCE.md](ONLINE_PROVENANCE.md)) |

Both editions use the same bundle identifier, data folder (saves, `bbport.ini`, `launch.conf`)
and logs, so a player can switch editions without losing anything.

## Updates

The launcher's update check (`launcher/Model/Update.swift`) reads this repository's releases
(tags `v<numbers>[-<suffix>]`) and downloads only its own edition's zip,
`Bloodborne-macOS-<version>-<edition>.zip`; a release without it is not offered. 0.1.0's
per-edition releases (tags `v0.1.0-<edition>`) are read too. The downloaded app must have the
same `BBEdition` before it replaces the installed one. Upstream's `mac-v*` tags are ignored.
**0.1.0 apps do not see later releases** (their check needs an edition in the tag): their
players download 0.1.1 once by hand; from 0.1.1 on, updates work. A version with a suffix
(`0.1.1-test1`) or a GitHub pre-release is a test build, offered only with **Include test
builds**. An app without `BBEdition` (a checkout's
`Bloodborne.app`) takes its edition from whether `out/gpu/libbbnet.dylib` exists.

## Workflows

- **`.github/workflows/build.yml`** runs on pushes to `master`, on pull requests and by hand.
  It builds the dependencies (cached in `deps/`), the port with the online module and the
  launcher, runs the launcher's tests (required) and the port's tests (informational: the
  runner is a VM), then packages both editions with `BB_EDITION=singleplayer|multiplayer
  scripts/macos/package_app.sh <version>`. It checks that only the multiplayer zip contains
  `libbbnet.dylib` and uploads both zips and `pkg_extract-macOS.zip` as an artifact.
- **`.github/workflows/release.yml`** calls `build.yml` with the version, then creates the
  release `v<version>` (Latest) with both zips and `pkg_extract-macOS.zip`. A version with a
  suffix is published as a pre-release. Notes: a table of the two downloads, install steps,
  then `docs/releases/v<version>.md` (or a pointer to the changelog).

## Publishing a version

1. Merge the release's changes into `master`, add `docs/releases/v<version>.md` and move the
   changelog's *Unreleased* section under the version.
2. Tag the merge commit and push the tag:

   ```sh
   git switch master && git pull
   git tag v0.1.1 && git push origin v0.1.1
   ```

   Or run **Release** from the Actions tab with the version (`0.1.1`); the tag is then created
   at the chosen branch's head.
3. The run takes about 40 minutes with a warm dependency cache (up to three hours without
   one) and publishes `v0.1.1`. Running it again for the same version replaces the zips.

## Locally

```sh
bash scripts/macos/bootstrap_deps.sh && BB_DEPS_ARCH=arm64 bash scripts/macos/bootstrap_deps.sh
bash build_macos.sh
BB_EDITION=singleplayer bash scripts/macos/package_app.sh 0.1.1
BB_EDITION=multiplayer bash scripts/macos/package_app.sh 0.1.1
```

Without `BB_EDITION`, `package_app.sh` makes the multiplayer edition when the online module
was built and the single-player one otherwise; the multiplayer edition refuses to package
without the module.

## Icons

Release bundles carry the app's own icon, `Resources/Bloodborne.icns`, an original drawing made
by `scripts/macos/make_icon.sh` (no game artwork is ever shipped). At run time the Dock shows the
game's own `sce_sys/icon0.png` from the player's game folder: the launcher sets it once it has
checked the folder, and the game window's process (`bbgpu-host`, a second Dock icon) sets it
from `BB_GAME_ICON` (run.sh; `gpu/shim/bbport_app_icon.mm`). A checkout's `Bloodborne.app`
(`make_app.sh`) uses icon0.png for its bundle icon when it knows the game folder.
