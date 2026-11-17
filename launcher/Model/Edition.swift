// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation

/// The two builds a release is published as (docs/RELEASES.md): the single-player app has no
/// online module (libbbnet.dylib) and no Online tab; the multiplayer app has both. Each is
/// offered only its own edition's updates: release tags end in "-singleplayer" or
/// "-multiplayer" (v0.1.0-multiplayer), assets in "-<edition>.zip".
enum Edition: String, CaseIterable {
    case singleplayer
    case multiplayer

    var label: String { self == .multiplayer ? L("Multiplayer") : L("Single-player") }
    var tagSuffix: String { "-" + rawValue }

    /// Info.plist's BBEdition (package_app.sh, make_app.sh); without one, whether the online
    /// module is where bb-probe looks for it.
    static func detect(info: [String: Any], moduleExists: () -> Bool) -> Edition {
        if let raw = info["BBEdition"] as? String, let edition = Edition(rawValue: raw) { return edition }
        return moduleExists() ? .multiplayer : .singleplayer
    }

    /// A release tag split into its version tag and edition: "v0.1.0-test1-multiplayer" ->
    /// ("v0.1.0-test1", .multiplayer). nil for a tag without an edition.
    static func split(tag: String) -> (version: String, edition: Edition)? {
        for edition in allCases where tag.hasSuffix(edition.tagSuffix) {
            return (String(tag.dropLast(edition.tagSuffix.count)), edition)
        }
        return nil
    }
}

extension Layout {
    /// Where bb-probe loads the online module from (src/runtime_net.c).
    var onlineModule: URL {
        switch self {
        case let .packaged(contents): contents.appendingPathComponent("Frameworks/libbbnet.dylib")
        case let .checkout(repo, _): repo.appendingPathComponent("out/gpu/libbbnet.dylib")
        }
    }
}
