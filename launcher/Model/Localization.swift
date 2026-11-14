// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation

/// The launcher's languages. English is the source text: every string is looked up by it
/// (launcher/Model/Strings.swift), so a string without a translation shows in English.
enum Language: String, CaseIterable, Identifiable {
    case english = "en", japanese = "ja", chinese = "zh"

    var id: String { rawValue }
    /// Its own name, the same in every language (the Language picker).
    var name: String {
        switch self {
        case .english: "English"
        case .japanese: "日本語"
        case .chinese: "简体中文"
        }
    }

    /// The first of the player's preferred languages (Locale.preferredLanguages: "zh-Hans-CN",
    /// "ja-JP", ...) the launcher has; English otherwise. Traditional Chinese readers get the
    /// Simplified text, closer than English.
    static func preferred(_ identifiers: [String]) -> Language {
        for identifier in identifiers {
            let code = identifier.lowercased().split(whereSeparator: { $0 == "-" || $0 == "_" }).first.map(String.init) ?? ""
            if let language = Language(rawValue: code) { return language }
        }
        return .english
    }
}

/// A string to translate: `L("Version \(v) is available")` looks up "Version %@ is available" and
/// puts the values into the translation ("%@" in order, or "%1$@", "%2$@" where the order differs;
/// "%%" is a percent sign there). A literal without values is looked up as written.
struct LocalizedText: ExpressibleByStringInterpolation {
    struct StringInterpolation: StringInterpolationProtocol {
        var key = ""
        var values: [String] = []
        init(literalCapacity: Int, interpolationCount: Int) {}
        mutating func appendLiteral(_ literal: String) { key += literal.replacingOccurrences(of: "%", with: "%%") }
        mutating func appendInterpolation<T>(_ value: T) { key += "%@"; values.append(String(describing: value)) }
    }

    let key: String
    let values: [String]
    let formatted: Bool

    init(stringLiteral value: String) { key = value; values = []; formatted = false }
    init(stringInterpolation: StringInterpolation) {
        key = stringInterpolation.key
        values = stringInterpolation.values
        formatted = true
    }
}

enum Localization {
    /// The language in use: launch.conf BB_LANGUAGE, else the system's (LauncherModel sets it).
    nonisolated(unsafe) static var language = Language.preferred(Locale.preferredLanguages)

    /// `english` in `language`, or itself where there is no translation.
    static func lookup(_ english: String, in language: Language = language) -> String {
        guard language != .english, let entry = Strings.table[english] else { return english }
        return language == .japanese ? entry.ja : entry.zh
    }

    static func string(_ text: LocalizedText, in language: Language = language) -> String {
        let template = lookup(text.key, in: language)
        return text.formatted ? fill(template, text.values) : template
    }

    /// "%@" takes the next value, "%2$@" the second, "%%" is "%".
    static func fill(_ template: String, _ values: [String]) -> String {
        var out = ""
        var next = 0
        var rest = Substring(template)
        while let percent = rest.firstIndex(of: "%") {
            out += rest[..<percent]
            rest = rest[rest.index(after: percent)...]
            if rest.hasPrefix("%") {
                out += "%"
                rest = rest.dropFirst()
            } else if rest.hasPrefix("@") {
                if next < values.count { out += values[next] }
                next += 1
                rest = rest.dropFirst()
            } else if let dollar = rest.firstIndex(of: "$"), let index = Int(rest[..<dollar]),
                      rest[rest.index(after: dollar)...].hasPrefix("@") {
                if index >= 1 && index <= values.count { out += values[index - 1] }
                rest = rest[rest.index(dollar, offsetBy: 2)...]
            } else {
                out += "%"
            }
        }
        return out + rest
    }
}

/// The launcher's text in the player's language.
func L(_ text: LocalizedText) -> String { Localization.string(text) }
/// The same for text that is not a literal here (a label from the settings' tables).
func L(dynamic english: String) -> String { Localization.lookup(english) }
