// SPDX-FileCopyrightText: Copyright 2026 Mark Hall
// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

struct LauncherView: View {
    /// The window's content size (main.swift).
    static let size = CGSize(width: 920, height: 730)

    @ObservedObject var model: LauncherModel

    private enum Page: Hashable { case general, advanced, controls, online }
    @State private var page = Page.general

    var body: some View {
        VStack(spacing: 0) {
            // Two pages of two columns each so the whole window fits a 13" MacBook Air (1440 x 900
            // points, Dock shown) without scrolling, even with every optional row showing.
            // Not a TabView: on macOS 26.0.1 switching back to a tab re-inserts its cached view
            // graph and SwiftUI's accessibility nodes loop on a dependency cycle, printing it
            // forever with the window frozen (#47, sampled by the reporter).
            Picker("", selection: $page) {
                Text(L("General")).tag(Page.general)
                Text(L("Advanced")).tag(Page.advanced)
                Text(L("Controls")).tag(Page.controls)
                if model.edition == .multiplayer {
                    Text(L("Online")).tag(Page.online)
                }
            }
            .pickerStyle(.segmented)
            .labelsHidden()
            .fixedSize()
            .padding(.top, 10)
            Group {
                switch page {
                case .general: general
                case .advanced: advanced
                case .controls: ControlsView(model: model)
                case .online: onlineTab
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .disabled(model.running)
            Divider()
            HStack(spacing: 12) {
                Button(L("Mods & Cheats…")) { ModsWindow.show(launcher: model) }
                Text(model.modsSummary).font(.caption).foregroundStyle(.secondary).lineLimit(1)
                UpdateBadge(updater: model.updater)
                Spacer()
                if let progress = model.benchmark.progress {
                    Text(progress).foregroundStyle(.secondary).lineLimit(1)
                } else if let message = model.exitMessage {
                    Text(message).foregroundStyle(.red).lineLimit(2)
                    if model.crashReport != nil {
                        Button(L("Crash Report…")) { model.showingCrashReport = true }
                    } else {
                        Button(L("Show Log"), action: model.showLog)
                    }
                }
                if model.running && model.canStop {
                    Text(model.stopping ? L("Stopping…") : L("Game running"))
                        .foregroundStyle(.secondary)
                    Button(action: { model.stop() }) {
                        Label(L("Stop"), systemImage: "stop.fill").frame(minWidth: 80)
                    }
                    .controlSize(.large)
                    .disabled(model.stopping)
                    .help(L("Close the running game. Use the game's Exit Game menu first to finish saving."))
                } else {
                    Button(L("Benchmark…")) { model.startBenchmark() }
                        .controlSize(.large)
                        .disabled(!model.canPlay)
                        .help(model.benchmarkProblem ?? L("Measures how the game runs on this Mac: about 6 minutes, with its own save and settings"))
                    Button(action: model.play) {
                        Text(model.running ? L("Running…") : L("Play")).frame(minWidth: 80)
                    }
                    .keyboardShortcut(.defaultAction)
                    .controlSize(.large)
                    .disabled(!model.canPlay)
                }
            }
            .padding()
        }
        .frame(width: LauncherView.size.width, height: LauncherView.size.height)
        .id(Localization.language) // a new language draws every text again
        .sheet(isPresented: $model.showingCrashReport) {
            if let report = model.crashReport {
                CrashReportView(report: report, model: model)
            }
        }
        // (A second .sheet on the same view would never show: SwiftUI presents one per view.)
        .background {
            Color.clear.sheet(isPresented: Binding(get: { model.updater.showing }, set: { if !$0 { model.updater.dismiss() } })) {
                UpdateView(model: model, updater: model.updater)
            }
        }
        .background {
            Color.clear.sheet(isPresented: Binding(get: { model.benchmark.showingResult }, set: { model.benchmark.showingResult = $0 })) {
                if let result = model.benchmark.result {
                    BenchmarkResultView(result: result, runner: model.benchmark)
                }
            }
        }
    }

    private var general: some View {
        HStack(alignment: .top, spacing: 0) {
            Form {
                Section(L("Display")) {
                    Toggle(L("Ultrawide or 16:10"), isOn: $model.settings.ultrawide)
                    if model.settings.ultrawide {
                        Picker(L("Resolution"), selection: $model.settings.wideOutput) {
                            ForEach(Resolution.ultrawideOutputs, id: \.self) { Text(Resolution.wideLabel($0)).tag($0) }
                        }
                        if let warning = Resolution.ultrawideMemoryWarning(model.settings.wideOutput,
                            memoryBytes: ProcessInfo.processInfo.physicalMemory) {
                            Text(warning).font(.caption).foregroundStyle(.orange)
                        }
                        Text(L("Native resolution. Uses your existing game UI; no UI files are installed."))
                            .font(.caption).foregroundStyle(.secondary)
                    } else {
                        Picker(L("Output resolution"), selection: $model.settings.output) {
                            ForEach(model.outputChoices, id: \.self) { Text($0.description).tag($0) }
                        }
                    }
                    Picker(L("Window"), selection: $model.settings.fullscreen) {
                        Text(L("Windowed")).tag(false)
                        Text(L("Full screen")).tag(true)
                    }
                    .pickerStyle(.segmented)
                    if model.showsDisplayChoice {
                        Picker(L("Display"), selection: $model.settings.display) {
                            Text(L("Main display")).tag(String?.none)
                            ForEach(model.displayChoices) { Text($0.name).tag(Optional($0.uuid)) }
                        }
                        Text(L("The game opens on this display, in a window or full screen. If it isn't connected, the main display is used."))
                            .font(.caption).foregroundStyle(.secondary)
                    }
                    Picker(L("Frame rate cap"), selection: $model.settings.frameCap) {
                        ForEach(FrameCap.allCases) { Text($0.label).tag($0) }
                    }
                    .pickerStyle(.segmented)
                    if model.settings.frameCap.rawValue > 60 {
                        Text(L("Above 60 FPS is experimental: the game was made for 30. 120 needs a 120 Hz display."))
                            .font(.caption).foregroundStyle(.secondary)
                    }
                    Picker(L("Game timing"), selection: $model.settings.gameTiming) {
                        ForEach(GameTiming.allCases) { Text(L(dynamic: $0.label)).tag($0) }
                    }
                    .pickerStyle(.segmented)
                    .disabled(!model.settings.frameCap.hasFixedSteps)
                    Text(model.settings.frameCap.hasFixedSteps
                         ? L("Fixed steps: the game advances the same amount every frame, as on the PS4. If a scene can't hold the cap it slows down a little. Variable: keeps full speed when frames are late, but uneven frames can drop you through moving lifts and floors. Applies at the next start.")
                         : L("\(model.settings.frameCap.label) FPS always uses variable timing: there is no fixed-step patch for it. Uneven frames can drop you through moving lifts."))
                        .font(.caption).foregroundStyle(.secondary)
                }
                Section(L("Upscaler")) {
                    if model.settings.ultrawide {
                        Text(L("Ultrawide runs at the selected native resolution, with upscaling off."))
                            .font(.caption).foregroundStyle(.secondary)
                    }
                    Picker(L("Upscaler"), selection: $model.settings.upscaler) {
                        ForEach(model.upscalerChoices) { Text(L(dynamic: $0.label)).tag($0) }
                    }
                    .disabled(model.settings.ultrawide)
                    if model.settings.upscaler == .metalfx_spatial {
                        Text(L("Spatial: lighter on the GPU, shimmers more in motion."))
                            .font(.caption).foregroundStyle(.secondary)
                    } else if model.settings.upscaler == .metalfx {
                        Text(L("MetalFX temporal: recommended for a steadier image in motion."))
                            .font(.caption).foregroundStyle(.secondary)
                    }
                    Picker(L("Quality"), selection: $model.settings.preset) {
                        ForEach(Resolution.presetNames.indices, id: \.self) { Text(L(dynamic: Resolution.presetNames[$0])).tag($0) }
                    }
                    .disabled(model.settings.ultrawide || !model.settings.upscaler.usesPreset)
                    LabeledContent(L("Renders"), value: "\(model.settings.renderSize) → \(model.settings.displayOutput)")
                    if model.settings.upscaler != .off {
                        Toggle(L("Sharpening"), isOn: $model.settings.sharpen)
                        if model.settings.sharpen {
                            Slider(value: $model.settings.sharpness, in: 0...2) { Text(L("Strength")) }
                        }
                    }
                }
            }
            .formStyle(.grouped)
            Form {
                Section(L("Effects")) {
                    ForEach(GameSettings.visualEffects) { effect in
                        Toggle(L(dynamic: effect.label), isOn: effectBinding(effect.key))
                    }
                    Toggle(L("Show FPS"), isOn: $model.settings.showFPS)
                }
                Section(L("Launcher")) {
                    Picker(L("Language"), selection: $model.config.language) {
                        Text(L("System")).tag(Language?.none)
                        ForEach(Language.allCases) { Text($0.name).tag(Optional($0)) }
                    }
                    Toggle(L("Hide launcher while playing"), isOn: $model.config.hideWhilePlaying)
                        .help(L("The launcher window and its Dock icon disappear when the game starts and come back when the game ends."))
                }
                Section(L("Locations")) {
                    FolderRow(title: L("Game folder"), path: model.config.gameDir?.path ?? L("Not chosen"),
                              problem: model.checkingGame ? nil : model.gameProblem,
                              choose: model.chooseGameFolder, reset: model.resetGameFolder)
                    FolderRow(title: L("Save folder"), path: model.saveDir.path, problem: model.saveProblem,
                              choose: model.chooseSaveFolder, reset: model.resetSaveFolder)
                }
            }
            .formStyle(.grouped)
        }
    }

    /// The in-game menu's Graphics and Advanced options (gpu/shim/bbport_overlay.cpp), same wording.
    private var advanced: some View {
        let upscaler = model.settings.upscaler
        let temporal = upscaler != .off && upscaler != .taa // FSR and MetalFX use the reactive mask
        return HStack(alignment: .top, spacing: 0) {
            Form {
                Section(L("Detail")) {
                    Picker(L("Model detail"), selection: $model.settings.modelDetail) {
                        ForEach(ModelDetail.allCases) { Text(L(dynamic: $0.label)).tag($0) }
                    }
                    Text(L("Distance at which characters and objects switch to simpler models."))
                        .font(.caption).foregroundStyle(.secondary)
                    Picker(L("Live resolution changes"), selection: $model.settings.liveResolution) {
                        ForEach(LiveResolution.allCases) { Text(L(dynamic: $0.label)).tag($0) }
                    }
                    .pickerStyle(.segmented)
                    Text(L("On: output resolution and quality change in the game without a restart, but post-processing stays at 1080p (slower). Off: everything renders at the chosen size. Scene resolutions above 1080p always use it."))
                        .font(.caption).foregroundStyle(.secondary)
                    Toggle(L("Avoid white scenes (MetalFX)"), isOn: $model.settings.metalfxWhiteSceneFix)
                    Text(L("For MetalFX output above 1080p. Keeps post-processing at 1080p to avoid white scenes, but can reduce performance. Requires a restart."))
                        .font(.caption).foregroundStyle(.secondary)
                }
                Section(L("Shader compilation")) {
                    Picker(L("New shaders"), selection: $model.settings.shaderCompile) {
                        ForEach(ShaderCompile.allCases) { Text(L(dynamic: $0.label)).tag($0) }
                    }
                    Text(L("Shaders the game hasn't used on this Mac before compile in the background. Shaders seen before load at start either way."))
                        .font(.caption).foregroundStyle(.secondary)
                }
                Section(L("Performance")) {
                    Toggle(L("Texture compression"), isOn: $model.settings.textureCompression)
                    Text(L("Lets the GPU compress the game's textures and render targets without any loss of quality. They take less memory bandwidth, which is the main limit on base and Pro chips: up to 40% more FPS there, less on Max and Ultra chips. Turn it off if you see wrong colours or broken textures. Applies at the next start."))
                        .font(.caption).foregroundStyle(.secondary)
                }
                Section(L("Startup and debugging")) {
                    Toggle(L("Skip intro videos"), isOn: effectBinding("skip_intro"))
                    Toggle(L("Metal Performance HUD"), isOn: $model.settings.metalHUD)
                        .help(L("Apple's Metal HUD: FPS, GPU time and frame pacing, top right"))
                    ForEach(GameSettings.debugEffects) { effect in
                        Toggle(L(dynamic: effect.label), isOn: effectBinding(effect.key))
                    }
                    Text(L("Free camera: hold Cross and press L3 (keyboard: E + X). Debug menu: left touchpad / G; needs DbgFont14h.ccm and DbgFont14h.tpf in dvdroot_ps4/font from Nexus mod #253."))
                        .font(.caption).foregroundStyle(.secondary)
                }
            }
            .formStyle(.grouped)
            Form {
                Section(L("Upscaler")) {
                    if upscaler == .off {
                        Text(L("Choose an upscaler on the General tab to adjust these options."))
                            .font(.caption).foregroundStyle(.secondary)
                    }
                    Group {
                        Toggle(L("Subpixel jitter"), isOn: $model.settings.jitter)
                            .help(L("Offsets each frame by a fraction of a pixel so the upscaler gathers more detail over time."))
                        Toggle(L("Character motion vectors"), isOn: $model.settings.objectMotion)
                            .help(L("Precise vectors for animated objects: clothing and weapons break up less in motion."))
                    }
                    .disabled(upscaler == .off)
                }
                Section(L("Reactive mask")) {
                    if !temporal {
                        Text(upscaler == .taa ? L("TAA doesn't use a reactive mask. Choose FSR or MetalFX to adjust it.")
                                              : L("Choose FSR or MetalFX on the General tab to adjust these options."))
                            .font(.caption).foregroundStyle(.secondary)
                    }
                    Group {
                        Toggle(L("Enable mask"), isOn: $model.settings.reactive)
                        Text(L("Marks transparent effects (particles, haze) so the upscaler relies less on previous frames. Less trailing behind effects, but shimmering returns beneath them."))
                            .font(.caption).foregroundStyle(.secondary)
                        Group {
                            Slider(value: $model.settings.reactiveScale, in: 0...4) { Text(L("Scale")) }
                            Slider(value: $model.settings.reactiveThreshold, in: 0...1) { Text(L("Threshold")) }
                            Slider(value: $model.settings.reactiveMax, in: 0...1) { Text(L("Maximum")) }
                        }
                        .disabled(!model.settings.reactive)
                    }
                    .disabled(!temporal)
                }
            }
            .formStyle(.grouped)
        }
    }

    private func effectBinding(_ key: String) -> Binding<Bool> {
        Binding(get: { model.effect(key) }, set: { model.setEffect(key, $0) })
    }

    /// Online play: the bbport.ini keys scripts/launch_settings.sh reads (docs/ONLINE.md).
    private var onlineTab: some View {
        Form {
            Section(L("Online play")) {
                Toggle(L("Play online"), isOn: $model.online.online)
                Text(L("Messages, bloodstains and ghosts come from the community server; bells and summons go through the co-op pool below. Off, the game never touches the network. Setup and troubleshooting: docs/ONLINE.md."))
                    .font(.caption).foregroundStyle(.secondary)
            }
            Section(L("Co-op pool")) {
                Picker(L("Pool"), selection: $model.online.pool) {
                    ForEach(OnlinePool.allCases) { Text($0.label).tag($0) }
                }
                .pickerStyle(.segmented)
                if model.online.pool == .huntersdream {
                    huntersDreamRows
                }
            }
            if model.online.pool == .shadnet {
                shadnetWarning
                Section(L("shadNet account")) {
                    shadnetRows
                }
                .disabled(!model.online.shadnetAccepted)
            }
            Section(L("Connection")) {
                Toggle(L("Ask the router to forward the co-op port (UPnP)"), isOn: $model.online.upnp)
                Text(L("On by default: while the game runs on the shadNet pool, the router forwards the co-op UDP port to this Mac (an hour at a time, renewed; removed on exit). Off, forward the port by hand if bells are not answered."))
                    .font(.caption).foregroundStyle(.secondary)
                TextField(L("Co-op UDP port"), text: $model.p2pPortText)
                    .onChange(of: model.p2pPortText) { _, text in
                        let digits = String(text.filter { $0.isASCII && $0.isNumber }.prefix(5))
                        if digits != text { model.p2pPortText = digits }
                    }
                Text(portValid ? L("3658 is the PlayStation 4's port; 0 picks any free port. A busy port falls back to a free one.")
                               : L("A port is a number from 0 to 65535."))
                    .font(.caption).foregroundStyle(portValid ? AnyShapeStyle(.secondary) : AnyShapeStyle(.red))
            }
            if let problem = model.onlineProblem {
                Text(problem).foregroundStyle(.red)
            }
        }
        .formStyle(.grouped)
    }

    private var portValid: Bool { OnlineSettings.port(from: model.p2pPortText) != nil }

    @ViewBuilder private var huntersDreamRows: some View {
        if model.signedIn {
            HStack {
                Text(L("Account"))
                Spacer()
                Text(L("Signed in as \(model.online.thdOnlineId)")).foregroundStyle(.secondary)
                Button(L("Sign Out"), action: model.signOut)
            }
            Text(L("bbhost and Windows-port players are on this pool; it works through any router."))
                .font(.caption).foregroundStyle(.secondary)
        } else {
            switch model.signIn {
            case .idle, .failed:
                HStack {
                    Text(L("Account"))
                    Spacer()
                    Text(L("Not signed in")).foregroundStyle(.secondary)
                    Button(L("Sign in with Discord…"), action: model.startSignIn)
                }
                Text(L("Free: links a Discord account the way the Windows port's launcher does. bbhost and Windows-port players are on this pool; it works through any router."))
                    .font(.caption).foregroundStyle(.secondary)
                if case let .failed(message) = model.signIn {
                    Text(L("The sign-in failed: \(message)")).font(.caption).foregroundStyle(.red)
                }
            case .starting:
                HStack {
                    ProgressView().controlSize(.small)
                    Text(L("Asking \(model.communityServer) for a code…"))
                    Spacer()
                    Button(L("Cancel"), action: model.cancelSignIn)
                }
            case let .waiting(page, code):
                Text(L("Open the page, sign in with Discord and enter this code:"))
                HStack(spacing: 12) {
                    Text(code).font(.system(.title2, design: .monospaced)).textSelection(.enabled)
                    Button(L("Copy Code")) { model.copyToPasteboard(code) }
                    Spacer()
                    Link(page.absoluteString, destination: page).lineLimit(1).truncationMode(.middle)
                    Button(L("Open Page")) { NSWorkspace.shared.open(page) }
                }
                HStack {
                    ProgressView().controlSize(.small)
                    Text(L("Waiting for the approval…"))
                    Spacer()
                    Button(L("Cancel"), action: model.cancelSignIn)
                }
            }
        }
    }

    /// shadNet is opt-in: its protocol sends the password in clear, so the player reads why and
    /// accepts it before the account is used (`shadnet_insecure_ok=1`, launch_settings.sh).
    private var shadnetWarning: some View {
        Section(L("shadNet: read this first")) {
            Label {
                Text(L("shadNet sends your password unencrypted")).font(.headline)
            } icon: {
                Image(systemName: "exclamationmark.triangle.fill").foregroundStyle(.yellow)
            }
            Text(L("The shadNet protocol has no encryption. Your account name and password travel to the shadNet server as plain text, so anyone who can see your network traffic (public or shared Wi-Fi, your network provider, anything between you and the server) can read them."))
            Text(L("Make a shadNet account just for this game, with a username and password you have never used anywhere else: not your Apple Account, email, PlayStation, Discord, Steam or any other account. If you have used this password anywhere else, change it there."))
                .fontWeight(.semibold)
            Text(L("The Hunter's Dream pool doesn't have this problem: it signs in with Discord over an encrypted connection and never sees a password."))
                .font(.caption).foregroundStyle(.secondary)
            Toggle(L("I understand: use shadNet, with a password I use nowhere else"), isOn: $model.online.shadnetAccepted)
        }
    }

    @ViewBuilder private var shadnetRows: some View {
        TextField(L("Server"), text: $model.online.shadnetServer, prompt: Text(OnlineSettings.defaultShadnetServer))
        TextField(L("Account name"), text: $model.online.shadnetNpid, prompt: Text(L("Your shadNet online id")))
            .onChange(of: model.online.shadnetNpid) { model.refreshSecretState() }
        SecureField(L("Password"), text: $model.newShadnetPassword,
                    prompt: Text(model.shadnetPasswordStored ? L("Stored in your keychain; type to replace it") : L("Kept in your login keychain")))
            .disabled(model.online.shadnetNpid.trimmingCharacters(in: .whitespaces).isEmpty)
        if model.online.shadnetAccepted && model.online.online
            && model.online.shadnetNpid.trimmingCharacters(in: .whitespaces).isEmpty {
            Label(L("No account yet: the game will say you are not signed in to the PlayStation Network. Enter your shadNet account name, then the password."),
                  systemImage: "exclamationmark.circle")
                .font(.caption).foregroundStyle(.red)
        }
        Text(model.online.shadnetAccepted
             ? L("Register at shadnet.shadps4.net. The password goes to your login keychain when the settings are saved (Play, or quitting), never into the file or the log.")
             : L("Off until you accept the warning above: the game won't sign in to shadNet."))
            .font(.caption).foregroundStyle(.secondary)
    }
}

private struct FolderRow: View {
    let title: String
    let path: String
    let problem: String?
    let choose: () -> Void
    let reset: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack {
                Text(title)
                Spacer()
                Button(L("Choose…"), action: choose)
                Button(L("Reset to Default"), action: reset)
            }
            Text(path).font(.caption).foregroundStyle(.secondary).textSelection(.enabled)
                .lineLimit(1).truncationMode(.middle).help(path)
            if let problem {
                Text(problem).font(.caption).foregroundStyle(.red)
            }
        }
    }
}

/// The game stopped with an error: what happened, and a report ready to paste into an issue.
private struct CrashReportView: View {
    let report: CrashReport
    @ObservedObject var model: LauncherModel
    @Environment(\.dismiss) private var dismiss
    @State private var copied = false

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text(L("Bloodborne stopped")).font(.title2.bold())
            Text(report.summary)
            Text(L("Please report it so it can be fixed: Report on GitHub opens a new issue with this report filled in, and shows a diagnostics zip (the full log and macOS's crash reports) to drag onto it. Nothing is sent until you submit the issue. Your home folder is shown as ~; other paths (a drive under /Volumes) appear as they are."))
                .font(.callout).foregroundStyle(.secondary)
            ScrollView {
                Text(report.report)
                    .font(.system(.caption, design: .monospaced))
                    .textSelection(.enabled)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(8)
            }
            .background(Color(nsColor: .textBackgroundColor))
            .clipShape(RoundedRectangle(cornerRadius: 6))
            HStack(spacing: 8) {
                if model.preparingCrashBundle {
                    ProgressView().controlSize(.small)
                    Text(L("Collecting macOS's crash reports and the log…")).font(.caption).foregroundStyle(.secondary)
                } else if let bundle = model.crashBundle {
                    Image(systemName: "doc.zipper").foregroundStyle(.secondary)
                    Text(L("Diagnostics: \(bundle.lastPathComponent)")).font(.caption).foregroundStyle(.secondary)
                        .textSelection(.enabled)
                    Button(L("Show in Finder")) { model.showCrashBundle(bundle) }.controlSize(.small)
                }
            }
            HStack {
                Button(L("Show Log in Finder"), action: model.showLog)
                Spacer()
                if copied {
                    Text(L("Copied")).foregroundStyle(.secondary)
                }
                Button(L("Copy Report")) {
                    model.copyCrashReport()
                    copied = true
                }
                Button(L("Report on GitHub")) {
                    model.reportOnGitHub()
                    copied = true
                }
                .disabled(model.preparingCrashBundle) // (a few seconds: the zip goes with the issue)
                Button(L("Close")) { dismiss() }
                    .keyboardShortcut(.defaultAction)
            }
        }
        .padding(20)
        .frame(width: 760, height: 560)
    }
}
