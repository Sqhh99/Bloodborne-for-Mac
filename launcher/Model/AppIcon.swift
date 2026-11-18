// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation

/// The launcher's icon is the app's own (scripts/macos/make_icon.sh), never the game's artwork,
/// which only the game window shows (gpu/shim/bbport_app_icon.mm).
enum AppIcon {
    /// The bundle has a custom icon: Finder keeps it in the bundle's "Icon\r" file (its resource
    /// fork). 0.1.2 and 0.1.3 set the game's artwork there; LauncherModel.removeGameIconFromBundle
    /// takes it away.
    static func hasCustomIcon(bundleURL: URL) -> Bool {
        FileManager.default.fileExists(atPath: bundleURL.appendingPathComponent("Icon\r").path)
    }
}
