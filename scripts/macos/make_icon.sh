#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Builds the app's own icon (scripts/macos/make_icon.swift: an original drawing, no game artwork)
# as an .icns. package_app.sh puts it in every release bundle; make_app.sh uses it when it does
# not know the game folder yet. Only the 256 and 512 point images, as make_app.sh's: macOS 26/27
# draw an icon's own 16/32/128 images as a smaller square on a white plate.
#
# usage: bash scripts/macos/make_icon.sh <out.icns>
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
out=${1:?usage: make_icon.sh <out.icns>}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
xcrun swiftc -O -o "$work/make_icon" scripts/macos/make_icon.swift
mkdir "$work/Bloodborne.iconset"
for size in 256 512; do
    "$work/make_icon" "$work/Bloodborne.iconset/icon_${size}x${size}.png" "$size"
    "$work/make_icon" "$work/Bloodborne.iconset/icon_${size}x${size}@2x.png" $((size * 2))
done
mkdir -p "$(dirname -- "$out")"
iconutil -c icns "$work/Bloodborne.iconset" -o "$out"
