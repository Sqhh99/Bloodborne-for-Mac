// SPDX-License-Identifier: GPL-2.0-or-later
import AppKit

/// The standard About panel: the app's icon and name, "Version <short> (<build>)" from
/// Info.plist, the credits below and NSHumanReadableCopyright.
@MainActor
enum About {
    static func show() {
        let info = Bundle.main.infoDictionary ?? [:]
        let short = info["CFBundleShortVersionString"] as? String ?? "dev"
        let build = info["CFBundleVersion"] as? String ?? short
        let details = build == short ? "" : build
        NSApp.orderFrontStandardAboutPanel(options: [
            .applicationName: "Bloodborne",
            .applicationVersion: short,
            .version: details,
            .credits: credits(commit: info["BBCommit"] as? String),
        ])
        NSApp.activate()
    }

    private static func credits(commit: String?) -> NSAttributedString {
        let center = NSMutableParagraphStyle()
        center.alignment = .center
        center.paragraphSpacing = 6
        let base: [NSAttributedString.Key: Any] = [
            .font: NSFont.systemFont(ofSize: NSFont.smallSystemFontSize),
            .foregroundColor: NSColor.labelColor,
            .paragraphStyle: center,
        ]
        let text = NSMutableAttributedString()
        func add(_ s: String, link: URL? = nil, secondary: Bool = false) {
            var attributes = base
            if let link { attributes[.link] = link }
            if secondary { attributes[.foregroundColor] = NSColor.secondaryLabelColor }
            text.append(NSAttributedString(string: s, attributes: attributes))
        }
        add(L("macOS port of the native Bloodborne PC port: the original PS4 game code runs through Rosetta 2, the graphics on Metal.") + "\n")
        add(L("Built on bbport by deadinside28, the Windows port by Mrsuss60 and the shadPS4 video core.") + "\n")
        add(L("A fork of bloodborne_mac_port, with code from bloodborne-mac.") + "\n")
        if let commit, !commit.isEmpty {
            add(L("Commit") + " ", secondary: true)
            add(commit, link: Updates.repositoryPage.appendingPathComponent("commit/\(commit)"))
            add(" · ", secondary: true)
        }
        add(L("Source code"), link: Updates.repositoryPage)
        add(" · ", secondary: true)
        add(L("Releases"), link: Updates.repositoryPage.appendingPathComponent("releases"))
        add("\n" + L("You need your own copy of the game. Not affiliated with FromSoftware or Sony Interactive Entertainment."),
            secondary: true)
        return text
    }
}
