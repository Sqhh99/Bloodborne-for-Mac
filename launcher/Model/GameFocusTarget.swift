// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation

/// Match this launcher's GPU host, including a checkout app's links into out/.
enum GameFocusTarget {
    static func matches(executable: URL, bundle: URL) -> Bool {
        let actual = executable.resolvingSymlinksInPath().standardizedFileURL
        return ["bbgpu-host", "bbgpu-host-macos27"].contains { name in
            bundle.appendingPathComponent("Contents/MacOS/" + name)
                .resolvingSymlinksInPath().standardizedFileURL == actual
        }
    }
}
