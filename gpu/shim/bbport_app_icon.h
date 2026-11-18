// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

namespace BbAppIcon {
// The game window's Dock icon: the game's own artwork (BB_GAME_ICON, run.sh: the game folder's
// sce_sys/icon0.png), rounded on Apple's icon grid. Only the game window shows it; the launcher
// keeps the app's own icon (scripts/macos/make_icon.sh). Call before SDL's video init.
void ApplyGameIcon();
} // namespace BbAppIcon
