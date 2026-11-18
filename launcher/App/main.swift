// SPDX-FileCopyrightText: Copyright 2026 Mark Hall
// SPDX-License-Identifier: GPL-2.0-or-later
// Bloodborne.app's main executable: the launcher window (docs/superpowers/specs/2026-10-16-launcher-design.md).
import AppKit
import SwiftUI

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate, NSMenuItemValidation {
    private var model: LauncherModel!
    private var window: NSWindow?
    private var quitAfterStop = false
    private let gameFocus = GameFocus()

    func applicationDidFinishLaunching(_ notification: Notification) {
        guard let layout = Layout.detect(bundleURL: Bundle.main.bundleURL,
                                         info: Bundle.main.infoDictionary ?? [:]) else {
            let alert = NSAlert()
            alert.messageText = L("This copy of Bloodborne.app is incomplete")
            alert.informativeText = L("Download the app again, or run scripts/macos/make_app.sh in the repository.")
            alert.runModal()
            NSApp.terminate(nil)
            return
        }
        model = LauncherModel(layout: layout)
        model.onStarted = { [weak self] in
            ModsWindow.hide()
            guard let self else { return }
            self.gameFocus.start()
            if self.model.config.hideWhilePlaying && !self.model.benchmark.isRunning {
                // Out of the way while the game runs (its settings are locked then), Dock icon
                // too: the game's window has its own "Bloodborne" icon, and a second one (or ⌘Tab
                // to it) only brought the hidden launcher back. Back when the game ends (gameEnded
                // -> showWindow, which makes it a regular app again).
                self.window?.orderOut(nil)
                NSApp.setActivationPolicy(.accessory)
            } else {
                // Keep the launcher available behind the game, including its Stop button.
                self.showWindow(activate: false)
            }
        }
        model.onExit = { [weak self] status in self?.gameEnded(status) }
        model.onLanguageChanged = { [weak self] in self?.buildMenu() }
        model.load()
        model.removeGameIconFromBundle()
        buildMenu() // (launch.conf's language is known now)
        // --benchmark: show the window and run the benchmark at once (scripted comparisons).
        if CommandLine.arguments.contains("--benchmark") {
            showWindow()
            model.startBenchmark(confirm: false)
            return
        }
        model.askAboutUpdatesIfNeeded()
        showWindow()
    }

    private func gameEnded(_ status: Int32) {
        gameFocus.stop()
        if quitAfterStop {
            NSApp.reply(toApplicationShouldTerminate: true)
            return
        }
        if status != 0 {
            // The crash report comes first; the footer still links the update.
            model.updater.showing = false
        }
        showWindow()
    }

    private func showWindow(activate: Bool = true) {
        NSApp.setActivationPolicy(.regular)
        if window == nil {
            let window = NSWindow(contentRect: NSRect(origin: .zero, size: LauncherView.size),
                                  styleMask: [.titled, .closable, .miniaturizable], backing: .buffered, defer: false)
            window.title = "Bloodborne"
            window.contentView = NSHostingView(rootView: LauncherView(model: model))
            window.isReleasedWhenClosed = false
            window.center()
            self.window = window
        }
        if activate {
            window?.makeKeyAndOrderFront(nil)
            NSApp.activate()
            model.checkForUpdatesIfDue()
        } else {
            window?.orderBack(nil)
        }
    }

    /// The app menu, in the launcher's language.
    func buildMenu() {
        let menu = NSMenu()
        let appItem = NSMenuItem()
        menu.addItem(appItem)
        let appMenu = NSMenu()
        appMenu.addItem(withTitle: L("About Bloodborne"), action: #selector(AppDelegate.showAbout(_:)), keyEquivalent: "").target = self
        appMenu.addItem(withTitle: L("Check for Updates…"), action: #selector(AppDelegate.checkForUpdates(_:)), keyEquivalent: "").target = self
        appMenu.addItem(.separator())
        appMenu.addItem(withTitle: L("Hide Bloodborne"), action: #selector(NSApplication.hide(_:)), keyEquivalent: "h")
        appMenu.addItem(.separator())
        appMenu.addItem(withTitle: L("Quit Bloodborne"), action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        appItem.submenu = appMenu
        NSApp.mainMenu = menu
    }

    @objc func showAbout(_ sender: Any?) { About.show() }

    @objc func checkForUpdates(_ sender: Any?) {
        showWindow()
        model.checkForUpdatesNow()
    }

    func validateMenuItem(_ item: NSMenuItem) -> Bool {
        item.action != #selector(checkForUpdates(_:)) || (model != nil && !model.running && !model.updater.busy)
    }

    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool {
        showWindow()
        return false
    }

    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        // Quitting the launcher stops a game it started; the benchmark's launches end with the launcher.
        guard model != nil && model.running && model.stop() else { return .terminateNow }
        quitAfterStop = true
        return .terminateLater
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        !model.running
    }

    func applicationWillTerminate(_ notification: Notification) {
        if model != nil && !model.running { model.save() }
    }
}

// Top-level code runs on the main thread; AppKit and the delegate are main-actor isolated.
MainActor.assumeIsolated {
    let app = NSApplication.shared
    let delegate = AppDelegate() // app.delegate is weak: this local lives while app.run() runs
    app.delegate = delegate
    delegate.buildMenu()
    app.setActivationPolicy(.regular)
    app.run()
}
