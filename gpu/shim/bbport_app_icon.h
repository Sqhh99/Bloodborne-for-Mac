// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

namespace BbAppIcon {
// The game window's Dock icon: the game's own artwork (BB_GAME_ICON, run.sh: the game folder's
// sce_sys/icon0.png), rounded like the launcher's (launcher/Model/AppIcon.swift), unless the
// bundle's own icon is already the game's. Without it the bundle's icon stays (release apps:
// scripts/macos/make_icon.sh's until the launcher sets the game's as a custom icon).
void ApplyGameIcon();
} // namespace BbAppIcon
