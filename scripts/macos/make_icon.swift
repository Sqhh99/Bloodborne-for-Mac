// SPDX-License-Identifier: GPL-2.0-or-later
// The app's own icon for release bundles (scripts/macos/make_icon.sh): an original drawing, a
// pale crescent moon over a crimson night, made here so no game artwork is shipped (the game's
// sce_sys/icon0.png is the player's, used only at run time and in checkout apps). Full-bleed
// square like icon0.png: macOS rounds it as it does make_app.sh's icon.
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

// Night sky: crimson near the horizon, almost black at the top.
let sky = CGGradient(colorsSpace: space, colors: [rgb(10, 4, 8), rgb(46, 6, 14), rgb(122, 14, 24)] as CFArray,
                     locations: [0, 0.55, 1])!
ctx.drawLinearGradient(sky, start: CGPoint(x: 0, y: s), end: CGPoint(x: 0, y: 0), options: [])

// A few faint stars (fixed positions: the icon is the same at every build).
let stars: [(CGFloat, CGFloat, CGFloat)] = [(0.16, 0.86, 0.006), (0.30, 0.74, 0.004), (0.82, 0.90, 0.005),
                                            (0.90, 0.66, 0.004), (0.12, 0.60, 0.003), (0.64, 0.94, 0.003)]
ctx.setFillColor(rgb(255, 236, 220, 0.75))
for (x, y, r) in stars { ctx.fillEllipse(in: CGRect(x: x * s - r * s, y: y * s - r * s, width: 2 * r * s, height: 2 * r * s)) }

// The moon's glow, then the moon.
let centre = CGPoint(x: 0.55 * s, y: 0.60 * s), radius = 0.25 * s
let glow = CGGradient(colorsSpace: space, colors: [rgb(255, 214, 190, 0.35), rgb(255, 190, 170, 0.28), rgb(255, 120, 110, 0)] as CFArray,
                      locations: [0, 0.35, 1])!
// Centred on the lit side, so the cut-out part stays sky.
let lit = CGPoint(x: centre.x - 0.35 * radius, y: centre.y - 0.05 * radius)
ctx.drawRadialGradient(glow, startCenter: lit, startRadius: 0, endCenter: lit, endRadius: radius * 1.8,
                       options: [])
let moon = CGRect(x: centre.x - radius, y: centre.y - radius, width: 2 * radius, height: 2 * radius)
// The shadowed part is cut out (the sky shows through): a crescent.
let bite = 0.86 * radius
let biteRect = CGRect(x: centre.x - bite + 0.42 * radius, y: centre.y - bite + 0.18 * radius,
                      width: 2 * bite, height: 2 * bite)
ctx.saveGState()
ctx.addEllipse(in: moon)
ctx.clip()
ctx.addRect(CGRect(x: 0, y: 0, width: s, height: s))
ctx.addEllipse(in: biteRect)
ctx.clip(using: .evenOdd)
let face = CGGradient(colorsSpace: space, colors: [rgb(250, 240, 222), rgb(214, 196, 176)] as CFArray, locations: [0, 1])!
ctx.drawRadialGradient(face, startCenter: CGPoint(x: centre.x - 0.3 * radius, y: centre.y + 0.3 * radius), startRadius: 0,
                       endCenter: centre, endRadius: radius * 1.2, options: [.drawsAfterEndLocation])
ctx.restoreGState()

// A dark skyline of rooftops and spires along the bottom.
ctx.setFillColor(rgb(8, 2, 5))
let path = CGMutablePath()
let roof: [(CGFloat, CGFloat)] = [(0, 0.20), (0.08, 0.20), (0.08, 0.26), (0.13, 0.33), (0.18, 0.26), (0.18, 0.22),
                                  (0.27, 0.22), (0.27, 0.30), (0.30, 0.30), (0.32, 0.45), (0.34, 0.30), (0.37, 0.30),
                                  (0.37, 0.24), (0.48, 0.24), (0.48, 0.28), (0.55, 0.34), (0.62, 0.28), (0.62, 0.21),
                                  (0.70, 0.21), (0.70, 0.27), (0.73, 0.27), (0.75, 0.39), (0.77, 0.27), (0.80, 0.27),
                                  (0.80, 0.19), (0.90, 0.19), (0.90, 0.24), (1, 0.24)]
path.move(to: CGPoint(x: 0, y: 0))
for (x, y) in roof { path.addLine(to: CGPoint(x: x * s, y: y * s)) }
path.addLine(to: CGPoint(x: s, y: 0))
path.closeSubpath()
ctx.addPath(path)
ctx.fillPath()

guard let image = ctx.makeImage(),
      let png = NSBitmapImageRep(cgImage: image).representation(using: .png, properties: [:]) else { exit(1) }
do { try png.write(to: URL(fileURLWithPath: args[1])) } catch {
    FileHandle.standardError.write("make_icon: \(error)\n".data(using: .utf8)!)
    exit(1)
}
