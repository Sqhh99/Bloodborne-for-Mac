// SPDX-License-Identifier: GPL-2.0-or-later
import CoreGraphics
import Foundation

/// The game's square artwork (sce_sys/icon0.png) as a macOS app icon for the Dock: Apple's icon
/// grid (a 1024 canvas, the 824 tile centred in it) with continuous corners and the soft shadow
/// native icons have. Set as the application icon image, the plain square showed square corners
/// beside rounded icons. (The bundle's .icns is rounded by macOS itself: make_app.sh.)
enum AppIcon {
    static let canvas = 1024
    static let tile: CGFloat = 824

    /// The tile's outline: a superellipse |x|^5 + |y|^5 = 1, close to Apple's continuous corners.
    static func tilePath(in rect: CGRect) -> CGPath {
        let path = CGMutablePath()
        let n = 5.0, steps = 720
        for i in 0...steps {
            let t = Double(i) / Double(steps) * 2 * .pi
            let c = cos(t), s = sin(t)
            let x = copysign(pow(abs(c), 2 / n), c), y = copysign(pow(abs(s), 2 / n), s)
            let p = CGPoint(x: rect.midX + CGFloat(x) * rect.width / 2, y: rect.midY + CGFloat(y) * rect.height / 2)
            i == 0 ? path.move(to: p) : path.addLine(to: p)
        }
        path.closeSubpath()
        return path
    }

    /// The bundle's own icon is the game's: Info.plist's BBGameIcon (make_app.sh made the .icns
    /// from the game folder) or a custom icon, which lives in the bundle's "Icon\r" file (its
    /// resource fork; what the launcher's NSWorkspace.setIcon writes).
    static func bundleShowsGameIcon(bundleURL: URL, info: [String: Any]) -> Bool {
        if info["BBGameIcon"] as? String == "1" { return true }
        return FileManager.default.fileExists(atPath: bundleURL.appendingPathComponent("Icon\r").path)
    }

    static func rounded(_ image: CGImage) -> CGImage? {
        guard let context = CGContext(data: nil, width: canvas, height: canvas, bitsPerComponent: 8, bytesPerRow: 0,
                                      space: CGColorSpace(name: CGColorSpace.sRGB)!,
                                      bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { return nil }
        context.interpolationQuality = .high
        let inset = (CGFloat(canvas) - tile) / 2
        let rect = CGRect(x: inset, y: inset, width: tile, height: tile)
        let path = tilePath(in: rect)
        // The shadow under the tile (the template's: 12 down, 28 blur, black at 30%).
        context.saveGState()
        context.setShadow(offset: CGSize(width: 0, height: -12), blur: 28,
                          color: CGColor(gray: 0, alpha: 0.3))
        context.addPath(path)
        context.setFillColor(CGColor(gray: 0, alpha: 1))
        context.fillPath()
        context.restoreGState()
        context.addPath(path)
        context.clip()
        context.draw(image, in: rect)
        return context.makeImage()
    }
}
