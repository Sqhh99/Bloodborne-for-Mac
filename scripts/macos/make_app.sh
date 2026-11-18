#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright 2026 Mark Hall
# Builds Bloodborne.app in the repository folder: double-click (or keep in the Dock) to play
# without a Terminal window. The app is the launcher window (launcher/) for this checkout: it
# shares launch.conf with Bloodborne.command, runs run.sh (which rebuilds what changed) and writes
# the game's logs to ~/Library/Logs/Bloodborne (one dated .log and .jsonl per session; game.log links
# to the newest). If the game stops with an error, the launcher shows a crash report and the log.
#
# The game window runs from inside the bundle (a link to out/bbgpu-host), so the Dock shows
# "Bloodborne". The app is built locally and never committed (Bloodborne.app is in .gitignore);
# its icon is the app's own (make_icon.sh), and the game window shows the game's artwork.
#
# usage: bash scripts/macos/make_app.sh     (again after moving the repository)
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
repo=$PWD
app=$repo/Bloodborne.app
rm -rf "$app"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"

# Version for the About panel and the update check (Updates in launcher/): 0.5.2-5-g0b50589 from
# a checkout is offered the next numbered release.
version=$(git describe --tags --match 'v[0-9]*' --exclude '*-singleplayer' --exclude '*-multiplayer' --dirty 2>/dev/null | sed 's/^v//' || true)
version=${version:-dev}
commit=$(git rev-parse --short HEAD 2>/dev/null || true)
repo_xml=$(printf '%s' "$repo" | sed -e 's/&/\&amp;/g' -e 's/</\&lt;/g' -e 's/>/\&gt;/g')
cat > "$app/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>Bloodborne</string>
    <key>CFBundleDisplayName</key><string>Bloodborne</string>
    <key>CFBundleIdentifier</key><string>io.github.wooloomooloo2.bloodborne-mac-port</string>
    <key>CFBundleExecutable</key><string>Bloodborne</string>
    <key>CFBundleIconFile</key><string>Bloodborne</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>${version%%-*}</string>
    <key>CFBundleVersion</key><string>$version</string>
    <key>BBCommit</key><string>$commit</string>
    <key>NSHumanReadableCopyright</key><string>bloodborne_mac_port contributors. GPL-2.0-or-later.</string>
    <key>LSMinimumSystemVersion</key><string>15.0</string>
    <key>LSApplicationCategoryType</key><string>public.app-category.role-playing-games</string>
    <key>NSHighResolutionCapable</key><true/>
    <key>BBRepository</key><string>$repo_xml</string>
</dict>
</plist>
EOF

# The launcher window (launcher/, Swift). The checkout's path is in Info.plist (BBRepository).
bash scripts/macos/build_launcher.sh
install -m755 out/launcher/Bloodborne "$app/Contents/MacOS/Bloodborne"
ln -s "$repo/out/bbgpu-host" "$app/Contents/MacOS/bbgpu-host"
ln -s "$repo/out/bbgpu-host-macos27" "$app/Contents/MacOS/bbgpu-host-macos27"

# The app's own icon, as in release apps (scripts/macos/make_icon.sh). The game's artwork is the
# game window's only (gpu/shim/bbport_app_icon.mm): the launcher never uses it.
bash scripts/macos/make_icon.sh "$app/Contents/Resources/Bloodborne.icns"
# Sign the completed bundle: adding its icon or executable links after signing invalidates
# the resource seal even though the launcher executable itself has not changed.
codesign --force --sign - "$app"
# This checkout app intentionally links to out/. Strict validation rejects external symlinks;
# the self-contained release bundle in package_app.sh is verified with --strict instead.
codesign --verify --deep "$app"
touch "$app"
# The bundle was deleted and made again at the same path: Finder and the Dock keep the generic
# icon they cached while it was incomplete until LaunchServices registers it afresh.
lsregister=/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister
[[ -x $lsregister ]] && "$lsregister" -f "$app" || true
echo "Built $app"
