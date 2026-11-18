// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation

extension Layout {
    /// Where bb-probe loads the online module from (src/runtime_net.c). Release apps always carry
    /// it (0.1.2 on; 0.1.0 and 0.1.1 also had a single-player edition without it); a checkout has
    /// it once build_macos.sh built it. Without it the launcher hides the Online tab.
    var onlineModule: URL {
        switch self {
        case let .packaged(contents): contents.appendingPathComponent("Frameworks/libbbnet.dylib")
        case let .checkout(repo, _): repo.appendingPathComponent("out/gpu/libbbnet.dylib")
        }
    }
}
