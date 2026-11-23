#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Builds the app's own icon (scripts/macos/app_icon.png: original gothic-Victorian hunter &
# Blood Moon artwork, no game files) as an .icns. package_app.sh puts it in every release bundle;
# make_app.sh uses it for local checkout apps. Only the 256 and 512 point images are included:
# macOS 26/27 draw an icon's own 16/32/128 images as a smaller square on a white plate.
#
# usage: bash scripts/macos/make_icon.sh <out.icns>
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
out=${1:?usage: make_icon.sh <out.icns>}
src=scripts/macos/app_icon.png
[[ -f $src ]] || { echo "make_icon.sh: missing $src" >&2; exit 1; }
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
iconset="$work/Bloodborne.iconset"
mkdir "$iconset"
sips -s format png -z 256 256 "$src" --out "$iconset/icon_256x256.png" >/dev/null
sips -s format png -z 512 512 "$src" --out "$iconset/icon_256x256@2x.png" >/dev/null
cp "$iconset/icon_256x256@2x.png" "$iconset/icon_512x512.png"
sips -s format png -z 1024 1024 "$src" --out "$iconset/icon_512x512@2x.png" >/dev/null
mkdir -p "$(dirname -- "$out")"
iconutil -c icns "$iconset" -o "$out"
