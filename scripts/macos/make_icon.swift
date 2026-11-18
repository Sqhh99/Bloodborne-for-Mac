// SPDX-License-Identifier: GPL-2.0-or-later
// The app's own icon (scripts/macos/make_icon.sh): an original drawing in a gothic-Victorian
// mood, a pale moon in a misty blood-tinted night over a city of spires, made here so no game
// artwork is shipped. The launcher always shows it; the game's sce_sys/icon0.png is the game
// window's only (gpu/shim/bbport_app_icon.mm). Full-bleed square: macOS rounds it.
//
// usage: make_icon <out.png> <pixels>
import AppKit
import CoreGraphics

let args = CommandLine.arguments
guard args.count == 3, let size = Int(args[2]), size > 0 else {
    FileHandle.standardError.write("usage: make_icon <out.png> <pixels>\n".data(using: .utf8)!)
    exit(2)
}
let s = CGFloat(size)
let space = CGColorSpace(name: CGColorSpace.sRGB)!
guard let ctx = CGContext(data: nil, width: size, height: size, bitsPerComponent: 8, bytesPerRow: 0,
                          space: space, bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { exit(1) }
func rgb(_ r: CGFloat, _ g: CGFloat, _ b: CGFloat, _ a: CGFloat = 1) -> CGColor {
    CGColor(colorSpace: space, components: [r / 255, g / 255, b / 255, a])!
}
func gradient(_ stops: [(CGFloat, CGColor)]) -> CGGradient {
    CGGradient(colorsSpace: space, colors: stops.map(\.1) as CFArray, locations: stops.map(\.0))!
}
/// Points given in 0...1 units, y up.
func p(_ x: CGFloat, _ y: CGFloat) -> CGPoint { CGPoint(x: x * s, y: y * s) }
/// Deterministic pseudo-random numbers: the icon is the same at every build.
var seed: UInt64 = 0x9E3779B97F4A7C15
func random() -> CGFloat {
    seed = seed &* 6364136223846793005 &+ 1442695040888963407
    return CGFloat(seed >> 33) / CGFloat(UInt64(1) << 31)
}

// Night: blue-black above, a dim blood haze along the horizon.
ctx.drawLinearGradient(gradient([(0, rgb(52, 14, 20)), (0.32, rgb(30, 12, 18)), (0.62, rgb(14, 12, 20)),
                                 (1, rgb(7, 7, 12))]),
                       start: p(0, 0), end: p(0, 1), options: [])

// Faint stars in the upper sky.
for _ in 0..<28 {
    let x = random(), y = 0.62 + random() * 0.36, r = 0.0015 + random() * 0.0025
    ctx.setFillColor(rgb(230, 220, 205, 0.25 + random() * 0.45))
    ctx.fillEllipse(in: CGRect(x: (x - r) * s, y: (y - r) * s, width: 2 * r * s, height: 2 * r * s))
}

// The moon: large and pale, a warm bone white, with a wide blood-tinted halo.
let moon = (x: CGFloat(0.58), y: CGFloat(0.63), r: CGFloat(0.215))
let centre = p(moon.x, moon.y)
ctx.drawRadialGradient(gradient([(0, rgb(232, 200, 180, 0.42)), (0.35, rgb(170, 70, 70, 0.20)),
                                 (1, rgb(90, 20, 30, 0))]),
                       startCenter: centre, startRadius: moon.r * s * 0.95, endCenter: centre,
                       endRadius: moon.r * s * 2.6, options: [])
let disc = CGRect(x: (moon.x - moon.r) * s, y: (moon.y - moon.r) * s, width: 2 * moon.r * s, height: 2 * moon.r * s)
ctx.saveGState()
ctx.addEllipse(in: disc)
ctx.clip()
ctx.drawRadialGradient(gradient([(0, rgb(246, 238, 222)), (0.7, rgb(226, 212, 190)), (1, rgb(196, 168, 150))]),
                       startCenter: p(moon.x - 0.06, moon.y + 0.06), startRadius: 0, endCenter: centre,
                       endRadius: moon.r * s * 1.05, options: [.drawsAfterEndLocation])
// Its maria: soft darker patches.
let maria: [(CGFloat, CGFloat, CGFloat)] = [(-0.075, 0.035, 0.10), (0.085, -0.075, 0.075), (0.11, 0.075, 0.05)]
for (dx, dy, r) in maria {
    let c = p(moon.x + dx, moon.y + dy)
    ctx.drawRadialGradient(gradient([(0, rgb(160, 136, 124, 0.22)), (1, rgb(160, 136, 124, 0))]),
                           startCenter: c, startRadius: 0, endCenter: c, endRadius: r * s, options: [])
}
ctx.restoreGState()

// Thin clouds drifting across the moon's lower half.
func cloud(_ y: CGFloat, _ x0: CGFloat, _ x1: CGFloat, _ h: CGFloat, _ alpha: CGFloat) {
    let path = CGMutablePath()
    path.move(to: p(x0, y))
    path.addCurve(to: p(x1, y + h * 0.3), control1: p(x0 + (x1 - x0) * 0.3, y + h), control2: p(x0 + (x1 - x0) * 0.7, y - h * 0.4))
    path.addCurve(to: p(x0, y), control1: p(x0 + (x1 - x0) * 0.6, y - h * 0.9), control2: p(x0 + (x1 - x0) * 0.2, y - h * 0.5))
    ctx.addPath(path)
    ctx.setFillColor(rgb(20, 10, 16, alpha))
    ctx.fillPath()
}
cloud(0.505, 0.26, 0.90, 0.04, 0.50)
cloud(0.565, 0.52, 1.02, 0.022, 0.38)
cloud(0.45, 0.02, 0.58, 0.028, 0.42)

// A gothic skyline: rooftops, gables and spires.
func skyline(_ parts: [(kind: Int, x: CGFloat, w: CGFloat, h: CGFloat)], base: CGFloat, colour: CGColor) {
    let path = CGMutablePath()
    path.move(to: p(0, 0))
    path.addLine(to: p(0, base))
    for part in parts {
        let left = part.x - part.w / 2, right = part.x + part.w / 2
        switch part.kind {
        case 0: // a flat roof block
            path.addLine(to: p(left, base)); path.addLine(to: p(left, base + part.h))
            path.addLine(to: p(right, base + part.h)); path.addLine(to: p(right, base))
        case 1: // a gable
            path.addLine(to: p(left, base)); path.addLine(to: p(left, base + part.h * 0.6))
            path.addLine(to: p(part.x, base + part.h)); path.addLine(to: p(right, base + part.h * 0.6))
            path.addLine(to: p(right, base))
        default: // a spire: a tower and a tall needle with two pinnacles
            let tower = part.h * 0.45
            path.addLine(to: p(left, base)); path.addLine(to: p(left, base + tower))
            path.addLine(to: p(left - part.w * 0.12, base + tower))
            path.addLine(to: p(left, base + tower + part.h * 0.10))
            path.addLine(to: p(part.x, base + part.h))
            path.addLine(to: p(right, base + tower + part.h * 0.10))
            path.addLine(to: p(right + part.w * 0.12, base + tower))
            path.addLine(to: p(right, base + tower)); path.addLine(to: p(right, base))
        }
    }
    path.addLine(to: p(1, base))
    path.addLine(to: p(1, 0))
    path.closeSubpath()
    ctx.addPath(path)
    ctx.setFillColor(colour)
    ctx.fillPath()
}

// Far city, in the haze.
skyline([(1, 0.06, 0.08, 0.06), (2, 0.15, 0.035, 0.20), (0, 0.24, 0.09, 0.05), (2, 0.33, 0.03, 0.15),
         (1, 0.42, 0.10, 0.07), (2, 0.74, 0.04, 0.22), (0, 0.84, 0.10, 0.06), (2, 0.93, 0.03, 0.16)],
        base: 0.25, colour: rgb(46, 20, 30))
// Mist between the far and the near city.
ctx.drawLinearGradient(gradient([(0, rgb(120, 70, 80, 0.0)), (0.5, rgb(130, 80, 90, 0.22)), (1, rgb(120, 70, 80, 0.0))]),
                       start: p(0, 0.20), end: p(0, 0.36), options: [])
// Near city, black: a cathedral with a tall central spire.
skyline([(1, 0.05, 0.10, 0.07), (0, 0.13, 0.06, 0.04), (2, 0.20, 0.05, 0.22), (1, 0.29, 0.09, 0.08),
         (2, 0.40, 0.045, 0.30), (2, 0.50, 0.075, 0.52), (2, 0.60, 0.045, 0.30), (1, 0.70, 0.09, 0.08),
         (0, 0.79, 0.07, 0.05), (2, 0.87, 0.045, 0.19), (1, 0.96, 0.08, 0.06)],
        base: 0.12, colour: rgb(8, 5, 8))

// A few dim lamplit windows.
let windows: [(CGFloat, CGFloat)] = [(0.27, 0.16), (0.31, 0.155), (0.50, 0.22), (0.50, 0.28), (0.72, 0.16), (0.09, 0.145)]
for (x, y) in windows {
    let r = CGRect(x: (x - 0.006) * s, y: y * s, width: 0.012 * s, height: 0.02 * s)
    ctx.setFillColor(rgb(214, 150, 80, 0.85))
    ctx.fill(r)
    let c = CGPoint(x: r.midX, y: r.midY)
    ctx.drawRadialGradient(gradient([(0, rgb(220, 140, 70, 0.30)), (1, rgb(220, 140, 70, 0))]),
                           startCenter: c, startRadius: 0, endCenter: c, endRadius: 0.03 * s, options: [])
}

// Vignette, then a fine grain.
let middle = p(0.5, 0.55)
ctx.drawRadialGradient(gradient([(0, rgb(0, 0, 0, 0)), (0.65, rgb(0, 0, 0, 0.10)), (1, rgb(0, 0, 0, 0.55))]),
                       startCenter: middle, startRadius: 0, endCenter: middle, endRadius: s * 0.78,
                       options: [.drawsAfterEndLocation])
let grains = size * size / 220
for _ in 0..<grains {
    let x = random() * s, y = random() * s, light = random() > 0.5
    ctx.setFillColor(light ? rgb(255, 240, 230, 0.035) : rgb(0, 0, 0, 0.06))
    ctx.fill(CGRect(x: x, y: y, width: max(1, s / 512), height: max(1, s / 512)))
}

guard let image = ctx.makeImage(),
      let png = NSBitmapImageRep(cgImage: image).representation(using: .png, properties: [:]) else { exit(1) }
do { try png.write(to: URL(fileURLWithPath: args[1])) } catch {
    FileHandle.standardError.write("make_icon: \(error)\n".data(using: .utf8)!)
    exit(1)
}
