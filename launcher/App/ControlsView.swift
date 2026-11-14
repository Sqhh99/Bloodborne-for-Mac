// SPDX-FileCopyrightText: Copyright 2026 Mark Hall
// SPDX-License-Identifier: GPL-2.0-or-later
import AppKit
import Combine
import Carbon.HIToolbox
import GameController
import SwiftUI

/// The launcher's Controls page: mouse camera settings and the keyboard and mouse bindings
/// (launcher/Model/Bindings.swift; the game reads them at start, src/runtime_bindings.c).
struct ControlsView: View {
    @ObservedObject var model: LauncherModel
    /// The slot waiting for a key or mouse button.
    @State private var capturing: Capture?
    @State private var showPadButtons = false

    struct Capture: Identifiable {
        let action: BindAction
        let slot: Int
        var id: String { "\(action.key)/\(slot)" }
    }

    var body: some View {
        HStack(alignment: .top, spacing: 0) {
            Form {
                Section(L("Mouse")) {
                    Toggle(L("Mouse camera"), isOn: $model.settings.mouseLook)
                        .help(L("Click in the game window to turn the camera with the mouse; \u{2318} gives the cursor back. Controllers always work."))
                    NumberSlider(label: L("Sensitivity"), value: $model.settings.mouseSensitivity, range: 0.1...10, format: "%.2f",
                                 help: L("How far the camera turns for a mouse movement."), logarithmic: true)
                    NumberSlider(label: L("Vertical speed"), value: $model.settings.mouseVertical, range: 0.25...4, format: "%.2f\u{00D7}",
                                 help: L("How fast the camera turns up and down, relative to left and right."))
                    Toggle(L("Invert vertical"), isOn: $model.settings.mouseInvertY)
                    Button(L("Reset mouse sensitivity")) {
                        model.settings.mouseSensitivity = GameSettings.defaultMouseSensitivity
                        model.settings.mouseVertical = 1
                        model.settings.mouseInvertY = false
                    }
                    Text(L("Free camera follows mouse distance. Adjust sensitivity here or live in the in-game menu (\u{2318},). Scroll up switches the right weapon; down switches quick items. 3 / Up adds blood bullets."))
                        .font(.caption).foregroundStyle(.secondary)
                }
                Section(L("Keyboard")) {
                    Text(L("Movement keys also navigate game menus. Q goes back; V uses a blood vial. Separate dodge and run keys are optional."))
                        .font(.caption).foregroundStyle(.secondary)
                    Toggle(L("Camera keys are the d-pad with the mouse camera"), isOn: $model.settings.cameraKeysDpad)
                        .disabled(!model.settings.mouseLook)
                        .help(L("While the mouse turns the camera, the camera keys (I, J, K and L by default) scroll menus and switch weapons and items instead."))
                }
                Section(L("Controller")) {
                    Toggle(L("L3 + R3 opens the in-game menu"), isOn: $model.settings.padMenuCombo)
                        .help(L("Pressing both sticks in opens the settings menu during play. Turn it off if you press them by accident; \u{2318}, still opens it."))
                    LabeledContent(model.settings.padBindings.isDefault ? L("Buttons as on a PlayStation pad") : L("Buttons remapped")) {
                        Button(L("Controller Buttons\u{2026}")) { showPadButtons = true }
                    }
                    .help(L("Choose what each controller button does in the game."))
                }
                groups(["Movement", "Camera"])
            }
            .formStyle(.grouped)
            Form {
                groups(["Combat", "Actions", "D-pad", "Menus"])
                Section {
                    HStack {
                        Text(L("A key can do several things; \u{2318} is kept for Mac shortcuts."))
                            .font(.caption).foregroundStyle(.secondary)
                        Spacer()
                        Button(L("Reset to Defaults")) { model.settings.bindings = Bindings() }
                            .disabled(model.settings.bindings.isDefault)
                    }
                }
            }
            .formStyle(.grouped)
        }
        .sheet(isPresented: $showPadButtons) {
            PadButtonsSheet(bindings: $model.settings.padBindings) { showPadButtons = false }
        }
        .sheet(item: $capturing) { capture in
            CaptureSheet(action: capture.action, slot: capture.slot,
                         current: capture.slot < model.settings.bindings[capture.action].count
                            ? model.settings.bindings[capture.action][capture.slot] : nil) { name in
                model.settings.bindings.set(capture.action, slot: capture.slot, to: name)
                capturing = nil
            } cancel: {
                capturing = nil
            }
        }
    }

    private func groups(_ names: [String]) -> some View {
        ForEach(names, id: \.self) { group in
            Section(L(dynamic: group)) {
                ForEach(Bindings.actions.filter { $0.group == group }) { action in
                    row(action)
                }
            }
        }
    }

    private func row(_ action: BindAction) -> some View {
        let names = model.settings.bindings[action]
        // Two slots always; one more empty one while there is room.
        let shown = min(Bindings.slots, max(2, names.count + (names.count >= 2 ? 1 : 0)))
        let shared = names.flatMap { model.settings.bindings.others(using: $0, besides: action) }
        return LabeledContent {
            HStack(spacing: 4) {
                if !shared.isEmpty {
                    Image(systemName: "exclamationmark.triangle").foregroundStyle(.orange)
                        .help(L("Also used by: ") + shared.map { "\(L(dynamic: $0.group)) \u{2013} \(L(dynamic: $0.label))" }.joined(separator: ", "))
                }
                ForEach(0..<shown, id: \.self) { slot in
                    let name = slot < names.count ? names[slot] : nil
                    Button { capturing = Capture(action: action, slot: slot) } label: {
                        Text(name.map(KeyLabel.text) ?? (slot == 2 || slot == 3 ? "+" : "\u{2013}"))
                            .frame(minWidth: slot < 2 ? 70 : 16)
                            .foregroundStyle(name == nil ? .secondary : .primary)
                    }
                    .help(name.map { L("\(KeyLabel.text($0)) \u{2013} click to change") } ?? L("Click, then press a key or mouse button"))
                }
            }
        } label: {
            Text(L(dynamic: action.label))
        }
    }
}

/// The controller bindings (launcher/Model/Bindings.swift PadBindings; bbport.ini bind_pad_ keys):
/// click a slot, then press the button on the controller. While the sheet is open the launcher
/// reads the controller with GameController, by the same positions as the game's SDL (South is
/// Cross). The L3 + R3 menu shortcut always uses the sticks themselves.
private struct PadButtonsSheet: View {
    @Binding var bindings: PadBindings
    let done: () -> Void
    @State private var listening: Listening?
    @State private var held: Set<String> = []
    @State private var controller: String?
    private let poll = Timer.publish(every: 1.0 / 60, on: .main, in: .common).autoconnect()

    struct Listening: Equatable {
        let action: PadAction
        let slot: Int
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text(L("Controller Buttons")).font(.headline)
            Text(controller.map { L("\($0): click a slot, then press the button you want for it.") }
                 ?? L("Connect a controller, then click a slot and press the button you want for it."))
                .font(.caption).foregroundStyle(controller == nil ? .orange : .secondary)
            Grid(alignment: .leading, verticalSpacing: 6) {
                ForEach(PadBindings.actions) { action in
                    GridRow {
                        Text(L(dynamic: action.label)).lineLimit(1).frame(width: 230, alignment: .leading)
                        row(action)
                    }
                }
            }
            HStack {
                Button(L("Reset to Defaults")) { listening = nil; bindings = PadBindings() }
                    .disabled(bindings.isDefault)
                if let listening {
                    Button(L("Clear Slot")) {
                        bindings.set(listening.action, slot: listening.slot, to: nil)
                        self.listening = nil
                    }
                    .disabled(listening.slot >= bindings[listening.action].count)
                }
                Spacer()
                Button(L("Done")) { listening = nil; done() }.keyboardShortcut(.defaultAction)
            }
        }
        .padding(20)
        .fixedSize()
        .onReceive(poll) { _ in sample() }
        .onExitCommand { listening = nil }
    }

    private func row(_ action: PadAction) -> some View {
        let names = bindings[action]
        let shown = min(Bindings.slots, max(2, names.count + (names.count >= 2 ? 1 : 0)))
        let shared = names.flatMap { bindings.others(using: $0, besides: action) }
        return HStack(spacing: 4) {
            ForEach(0..<shown, id: \.self) { slot in
                let name = slot < names.count ? names[slot] : nil
                let waiting = listening == Listening(action: action, slot: slot)
                Button {
                    listening = waiting ? nil : Listening(action: action, slot: slot)
                } label: {
                    Text(waiting ? L("Press\u{2026}") : name ?? (slot >= 2 ? "+" : "\u{2013}"))
                        .lineLimit(1)
                        .frame(width: name == nil && slot >= 2 && !waiting ? 16 : 84)
                        .foregroundStyle(waiting ? Color.accentColor : name == nil ? .secondary : .primary)
                }
                .help(name.map { L("\(L(dynamic: PadBindings.label($0))) \u{2013} click, then press a controller button") }
                      ?? L("Click, then press a controller button"))
            }
            Image(systemName: "exclamationmark.triangle").foregroundStyle(.orange)
                .opacity(shared.isEmpty ? 0 : 1)
                .help(L("Also used by: ") + shared.map { L(dynamic: $0.label) }.joined(separator: ", "))
        }
    }

    /// Binds the first button that goes down while a slot is waiting (one held when the slot was
    /// clicked has to be let go first).
    private func sample() {
        let pad = GCController.current ?? GCController.controllers().first
        controller = pad.map { $0.vendorName ?? L("Controller") }
        let now = pad.map(Self.pressed) ?? []
        if let listening, let name = PadBindings.buttons.first(where: { now.contains($0) && !held.contains($0) }) {
            bindings.set(listening.action, slot: listening.slot, to: name)
            self.listening = nil
        }
        held = now
    }

    /// The buttons held, by the game's names (SDL positions; DualSense Edge back buttons are SDL's
    /// right and left paddle 1, Xbox Elite P1-P4 its paddles in that order).
    private static func pressed(_ controller: GCController) -> Set<String> {
        var names = Set<String>()
        guard let pad = controller.extendedGamepad else { return names }
        let buttons: [(GCControllerButtonInput?, String)] = [
            (pad.buttonA, "Cross"), (pad.buttonB, "Circle"), (pad.buttonX, "Square"), (pad.buttonY, "Triangle"),
            (pad.leftShoulder, "L1"), (pad.rightShoulder, "R1"), (pad.leftTrigger, "L2"), (pad.rightTrigger, "R2"),
            (pad.leftThumbstickButton, "L3"), (pad.rightThumbstickButton, "R3"),
            (pad.buttonMenu, "Options"), (pad.buttonOptions, "Select"),
            (pad.dpad.up, "D-pad Up"), (pad.dpad.down, "D-pad Down"),
            (pad.dpad.left, "D-pad Left"), (pad.dpad.right, "D-pad Right"),
        ]
        for (button, name) in buttons where button?.isPressed == true { names.insert(name) }
        if let ds = pad as? GCDualSenseGamepad, ds.touchpadButton.isPressed { names.insert("Touchpad") }
        if let ds4 = pad as? GCDualShockGamepad, ds4.touchpadButton.isPressed { names.insert("Touchpad") }
        if let xbox = pad as? GCXboxGamepad {
            let extra: [(GCControllerButtonInput?, String)] = [
                (xbox.paddleButton1, "Paddle 1"), (xbox.paddleButton2, "Paddle 2"),
                (xbox.paddleButton3, "Paddle 3"), (xbox.paddleButton4, "Paddle 4"), (xbox.buttonShare, "Misc"),
            ]
            for (button, name) in extra where button?.isPressed == true { names.insert(name) }
        }
        let profile = controller.physicalInputProfile.buttons
        if profile[GCInputRightPaddle]?.isPressed == true { names.insert("Paddle 1") }
        if profile[GCInputLeftPaddle]?.isPressed == true { names.insert("Paddle 3") }
        return names
    }
}

/// A slider with its value, for the mouse settings.
private struct NumberSlider: View {
    let label: String
    @Binding var value: Double
    let range: ClosedRange<Double>
    let format: String
    let help: String
    var logarithmic = false

    var body: some View {
        LabeledContent(label) {
            HStack {
                if logarithmic {
                    Slider(value: Binding(get: { log2(max(range.lowerBound, value)) },
                                          set: { value = min(range.upperBound, max(range.lowerBound, pow(2, $0))) }),
                           in: log2(range.lowerBound)...log2(range.upperBound))
                } else {
                    Slider(value: $value, in: range)
                }
                Text(String(format: format, value)).monospacedDigit().frame(width: 44, alignment: .trailing)
            }
        }
        .help(help)
    }
}

/// Waits for one key, mouse button or wheel turn in its box; Clear empties the slot. A modifier
/// (Shift, Option, Control) binds when it is let go; another key or a mouse button pressed while
/// it is held binds the two as a chord ("Left Shift+Mouse Left").
private struct CaptureSheet: View {
    let action: BindAction
    let slot: Int
    let current: String?
    let done: (String?) -> Void
    let cancel: () -> Void
    @State private var message: String?
    @State private var monitor: Any?
    @State private var heldModifier: String?

    private func bind(_ name: String) {
        done(heldModifier.map { $0 + "+" + name } ?? name)
    }

    var body: some View {
        VStack(spacing: 14) {
            Text("\(L(dynamic: action.group)): \(L(dynamic: action.label))").font(.headline)
            CaptureArea { name in bind(name) } refuse: { message = $0 }
                .frame(width: 360, height: 120)
                .overlay {
                    VStack(spacing: 6) {
                        if let heldModifier {
                            Text(L("\(KeyLabel.text(heldModifier)): let go for this key alone, or press another key or mouse button with it"))
                                .multilineTextAlignment(.center)
                        } else {
                            Text(L("Press a key, click a mouse button or turn the wheel here"))
                        }
                        if let current { Text(L("Now: \(KeyLabel.text(current))")).foregroundStyle(.secondary) }
                    }
                    .allowsHitTesting(false)
                }
                .background(RoundedRectangle(cornerRadius: 8).fill(.quaternary))
            if let message { Text(message).font(.caption).foregroundStyle(.orange) }
            HStack {
                Button(L("Clear")) { done(nil) }.disabled(current == nil)
                Spacer()
                Button(L("Cancel"), action: cancel)
            }
        }
        .padding(20)
        .frame(width: 400)
        .onAppear {
            // Every key goes to the binding, Esc and Return too (Esc is the menu by default).
            monitor = NSEvent.addLocalMonitorForEvents(matching: [.keyDown, .flagsChanged]) { event in
                switch KeyLabel.name(for: event) {
                case .bound(let name): bind(name)
                case .modifier(let name): heldModifier = name; message = nil
                case .released(let name): if heldModifier == name { heldModifier = nil; done(name) }
                case .refused(let why): message = why
                case .ignored: break
                }
                return nil
            }
        }
        .onDisappear {
            if let monitor { NSEvent.removeMonitor(monitor) }
            monitor = nil
        }
    }
}

/// The box that takes mouse buttons and the wheel.
private struct CaptureArea: NSViewRepresentable {
    let bound: (String) -> Void
    let refuse: (String) -> Void

    final class View: NSView {
        var bound: (String) -> Void = { _ in }
        private var wheel = 0.0
        override func mouseDown(with event: NSEvent) { bound("Mouse Left") }
        override func rightMouseDown(with event: NSEvent) { bound("Mouse Right") }
        override func otherMouseDown(with event: NSEvent) {
            // NSEvent numbers: 2 middle, 3 back, 4 forward (SDL's X1 and X2).
            switch event.buttonNumber {
            case 2: bound("Mouse Middle")
            case 3: bound("Mouse Back")
            case 4: bound("Mouse Forward")
            default: break
            }
        }
        override func scrollWheel(with event: NSEvent) {
            // Physical direction, as the game counts it (bbport_mouse.cpp), not "natural scrolling".
            let dy = event.isDirectionInvertedFromDevice ? -event.scrollingDeltaY : event.scrollingDeltaY
            wheel += event.hasPreciseScrollingDeltas ? dy / 10 : dy
            if wheel >= 1 { bound("Wheel Up") } else if wheel <= -1 { bound("Wheel Down") }
        }
    }

    func makeNSView(context: Context) -> View { let v = View(); v.bound = bound; return v }
    func updateNSView(_ view: View, context: Context) { view.bound = bound }
}

/// Names for keys: the ini's SDL names and what the launcher shows.
enum KeyLabel {
    enum Result { case bound(String), modifier(String), released(String), refused(String), ignored }

    /// A key press as a binding: keyDown, or in flagsChanged a modifier going down (the first half
    /// of a chord, or the binding itself once it is let go).
    static func name(for event: NSEvent) -> Result {
        var modifier = false
        if event.type == .flagsChanged {
            let flag: NSEvent.ModifierFlags? = switch Int(event.keyCode) {
            case kVK_Shift, kVK_RightShift: .shift
            case kVK_Option, kVK_RightOption: .option
            case kVK_Control, kVK_RightControl: .control
            case kVK_Command, kVK_RightCommand: .command
            case kVK_CapsLock: nil // each press toggles it: take either
            default: .function
            }
            if let flag, !event.modifierFlags.contains(flag) {
                return KeyCodes.name(keyCode: event.keyCode).map(Result.released) ?? .ignored
            }
            modifier = flag != nil
        }
        guard let name = KeyCodes.name(keyCode: event.keyCode) else {
            return .refused(L("The game can't read that key."))
        }
        if Bindings.reserved.contains(name) { return .refused(L("\u{2318} is kept for Mac shortcuts (and gives the cursor back).")) }
        return modifier ? .modifier(name) : .bound(name)
    }

    private static let fixed: [String: String] = [
        "Mouse Left": "Left click", "Mouse Right": "Right click", "Mouse Middle": "Middle click",
        "Mouse Back": "Back button", "Mouse Forward": "Forward button",
        "Wheel Up": "Wheel up", "Wheel Down": "Wheel down",
        "Left Shift": "\u{21E7} Left", "Right Shift": "\u{21E7} Right",
        "Left Alt": "\u{2325} Left", "Right Alt": "\u{2325} Right",
        "Left Ctrl": "\u{2303} Left", "Right Ctrl": "\u{2303} Right",
        "Up": "\u{2191}", "Down": "\u{2193}", "Left": "\u{2190}", "Right": "\u{2192}",
        "Backspace": "Delete \u{232B}", "Delete": "Fwd Del \u{2326}", "Escape": "Esc",
        "Insert": "Help", "Keypad Enter": "Enter",
    ]

    /// What is printed on the key in the current keyboard layout (letters, digits, symbols), else
    /// a readable name.
    static func text(_ name: String) -> String {
        if let (held, key) = Bindings.chord(name) { return text(held) + " + " + text(key) }
        if let label = fixed[name] { return L(dynamic: label) }
        if name.count == 1, let code = KeyCodes.keyCode(name: name), let typed = typed(code) { return typed }
        return name
    }

    private static func typed(_ code: UInt16) -> String? {
        guard let source = TISCopyCurrentKeyboardLayoutInputSource()?.takeRetainedValue(),
              let raw = TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData) else { return nil }
        let data = Unmanaged<CFData>.fromOpaque(raw).takeUnretainedValue() as Data
        return data.withUnsafeBytes { bytes -> String? in
            guard let layout = bytes.baseAddress?.assumingMemoryBound(to: UCKeyboardLayout.self) else { return nil }
            var dead: UInt32 = 0
            var chars = [UniChar](repeating: 0, count: 4)
            var length = 0
            let status = UCKeyTranslate(layout, code, UInt16(kUCKeyActionDisplay), 0, UInt32(LMGetKbdType()),
                                        OptionBits(kUCKeyTranslateNoDeadKeysBit), &dead, chars.count, &length, &chars)
            guard status == noErr, length > 0 else { return nil }
            let s = String(utf16CodeUnits: chars, count: length).uppercased()
            return s.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty ? nil : s
        }
    }
}
