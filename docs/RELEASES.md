# Releases: two editions per version

Every version of Bloodborne for Mac is published as two GitHub releases, built from the same
commit by one CI run:

| | Single-player | Multiplayer |
|---|---|---|
| Release tag | `v<version>-singleplayer` (marked Latest) | `v<version>-multiplayer` |
| Download | `Bloodborne-macOS-<version>-singleplayer.zip` | `Bloodborne-macOS-<version>-multiplayer.zip` |
| Online module (`Contents/Frameworks/libbbnet.dylib`) | not included | included |
| Launcher **Online** tab | hidden | shown |
| `online=1` in bbport.ini | ignored: run.sh exports `BB_ONLINE=0`, so mods, cheats and patch files stay on and no secret is read | online play ([ONLINE.md](ONLINE.md)) |
| Info.plist `BBEdition` | `singleplayer` | `multiplayer` |
| Licence of the bundle | GPL (see README) | GPL, including the online module's GPL-3.0-or-later code ([ONLINE_PROVENANCE.md](ONLINE_PROVENANCE.md)) |

Both editions use the same bundle identifier, data folder (saves, `bbport.ini`, `launch.conf`)
and logs, so a player can switch editions without losing anything.

## Updates

The launcher's update check (`launcher/Model/Update.swift`) reads this repository's releases
and offers only its own edition: tags `v<numbers>[-<suffix>]-<edition>` with an asset named
`Bloodborne-macOS-<version>-<edition>.zip`. The downloaded app must have the same `BBEdition`
before it replaces the installed one. Upstream's `mac-v*` tags and tags without an edition are
ignored. A version with a suffix (`0.1.1-test1`) or a GitHub pre-release is a test build,
offered only with **Include test builds**. An app without `BBEdition` (a checkout's
`Bloodborne.app`) takes its edition from whether `out/gpu/libbbnet.dylib` exists.

## Workflows

- **`.github/workflows/build.yml`** runs on pushes to `master`, on pull requests and by hand.
  It builds the dependencies (cached in `deps/`), the port with the online module and the
  launcher, runs the launcher's tests (required) and the port's tests (informational: the
  runner is a VM), then packages both editions with `BB_EDITION=singleplayer|multiplayer
  scripts/macos/package_app.sh <version>`. It checks that only the multiplayer zip contains
  `libbbnet.dylib` and uploads both zips and `pkg_extract-macOS.zip` as an artifact.
- **`.github/workflows/release.yml`** calls `build.yml` with the version, then creates the
  multiplayer release and then the single-player one, so the single-player release is
  Latest. A version with a suffix is published as a pre-release on both. Notes: an edition
  header, install steps, then `docs/releases/v<version>.md` (or a pointer to the changelog).

## Publishing a version

1. Merge the release's changes into `master`, add `docs/releases/v<version>.md` and move the
   changelog's *Unreleased* section under the version.
2. Tag the merge commit and push the tag:

   ```sh
   git switch master && git pull
   git tag v0.1.0 && git push origin v0.1.0
   ```

   Or run **Release** from the Actions tab with the version (`0.1.0`); the release tags then
   point at the chosen branch's head.
3. The run takes about an hour with a warm dependency cache (up to three without one). It
   creates `v0.1.0-multiplayer` and `v0.1.0-singleplayer`. Running it again for the same
   version replaces the zips on existing releases.

## Locally

```sh
bash scripts/macos/bootstrap_deps.sh && BB_DEPS_ARCH=arm64 bash scripts/macos/bootstrap_deps.sh
bash build_macos.sh
BB_EDITION=singleplayer bash scripts/macos/package_app.sh 0.1.0
BB_EDITION=multiplayer bash scripts/macos/package_app.sh 0.1.0
```

Without `BB_EDITION`, `package_app.sh` makes the multiplayer edition when the online module
was built and the single-player one otherwise; the multiplayer edition refuses to package
without the module.
