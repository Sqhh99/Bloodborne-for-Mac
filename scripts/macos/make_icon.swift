// SPDX-License-Identifier: GPL-2.0-or-later
// The launcher's own artwork: a bone-white moon above a misty, crimson gothic city.
// Every shape and texture is drawn here; no game artwork, external images or fonts are used.
// Full-bleed square: the bundle icon is masked by macOS (scripts/macos/make_icon.sh).
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
ctx.setAllowsAntialiasing(true)
ctx.setShouldAntialias(true)
typealias Pair = (CGFloat, CGFloat)
typealias Triple = (CGFloat, CGFloat, CGFloat)
typealias Quad = (CGFloat, CGFloat, CGFloat, CGFloat)
typealias Five = (CGFloat, CGFloat, CGFloat, CGFloat, CGFloat)

func rgb(_ r: CGFloat, _ g: CGFloat, _ b: CGFloat, _ a: CGFloat = 1) -> CGColor {
    CGColor(colorSpace: space, components: [r / 255, g / 255, b / 255, a])!
}
func gradient(_ stops: [(CGFloat, CGColor)]) -> CGGradient {
    CGGradient(colorsSpace: space, colors: stops.map(\.1) as CFArray, locations: stops.map(\.0))!
}
/// All artwork coordinates are normalized, with y pointing up.
func p(_ x: CGFloat, _ y: CGFloat) -> CGPoint { CGPoint(x: x * s, y: y * s) }
func rect(_ x: CGFloat, _ y: CGFloat, _ w: CGFloat, _ h: CGFloat) -> CGRect {
    CGRect(x: x * s, y: y * s, width: w * s, height: h * s)
}
func polygon(_ points: [Pair], _ colour: CGColor) {
    guard let first = points.first else { return }
    let path = CGMutablePath()
    path.move(to: p(first.0, first.1))
    for point in points.dropFirst() { path.addLine(to: p(point.0, point.1)) }
    path.closeSubpath()
    ctx.addPath(path)
    ctx.setFillColor(colour)
    ctx.fillPath()
}
func line(_ points: [Pair], _ colour: CGColor, width: CGFloat) {
    guard let first = points.first else { return }
    ctx.beginPath()
    ctx.move(to: p(first.0, first.1))
    for point in points.dropFirst() { ctx.addLine(to: p(point.0, point.1)) }
    ctx.setStrokeColor(colour)
    ctx.setLineWidth(width * s)
    ctx.strokePath()
}
/// A soft ellipse, rather than a hard-edged cloud crossing the moon.
func haze(_ x: CGFloat, _ y: CGFloat, _ rx: CGFloat, _ ry: CGFloat, _ colour: CGColor) {
    ctx.saveGState()
    ctx.translateBy(x: x * s, y: y * s)
    ctx.scaleBy(x: rx * s, y: ry * s)
    ctx.drawRadialGradient(gradient([(0, colour), (0.35, colour.copy(alpha: colour.alpha * 0.6)!), (1, colour.copy(alpha: 0)!)]),
                           startCenter: .zero, startRadius: 0, endCenter: .zero, endRadius: 1, options: [])
    ctx.restoreGState()
}

// Fixed seed and fixed texture counts keep every rendered size and build consistent.
var seed: UInt64 = 0xBB20261118
func random() -> CGFloat {
    seed = seed &* 6364136223846793005 &+ 1442695040888963407
    return CGFloat(seed >> 33) / CGFloat(UInt64(1) << 31)
}

// A restrained blue-black sky; blood-red light lives mostly at the horizon.
ctx.drawLinearGradient(gradient([(0, rgb(25, 10, 18)), (0.28, rgb(53, 18, 30)),
                                 (0.60, rgb(24, 17, 28)), (1, rgb(9, 13, 23))]),
                       start: p(0, 0), end: p(0, 1), options: [])
haze(0.57, 0.36, 0.72, 0.40, rgb(140, 40, 58, 0.34))
haze(0.16, 0.80, 0.48, 0.42, rgb(56, 70, 91, 0.13))

// A few quiet stars: none compete with the moon at Dock sizes.
for _ in 0..<48 {
    let x = 0.06 + random() * 0.88, y = 0.52 + random() * 0.43
    let radius = 0.00045 + random() * 0.0011, alpha = 0.12 + random() * 0.35
    ctx.setFillColor(rgb(218, 224, 235, alpha))
    ctx.fillEllipse(in: rect(x - radius, y - radius, radius * 2, radius * 2))
}

// Moon: a strong, simple disc with quiet mineral detail, offset behind the main spire.
let moon = (x: CGFloat(0.575), y: CGFloat(0.665), r: CGFloat(0.232))
haze(moon.x, moon.y, 0.47, 0.46, rgb(187, 121, 126, 0.17))
haze(moon.x, moon.y, 0.30, 0.30, rgb(224, 204, 187, 0.23))
let disc = rect(moon.x - moon.r, moon.y - moon.r, moon.r * 2, moon.r * 2)
ctx.saveGState()
ctx.addEllipse(in: disc)
ctx.clip()
ctx.drawLinearGradient(gradient([(0, rgb(179, 160, 153)), (0.35, rgb(219, 210, 192)),
                                 (0.75, rgb(242, 237, 218)), (1, rgb(226, 224, 209))]),
                       start: p(moon.x + 0.12, moon.y - moon.r),
                       end: p(moon.x - 0.12, moon.y + moon.r), options: [.drawsBeforeStartLocation, .drawsAfterEndLocation])
// Overlapping, soft lunar plains rather than a photographic texture.
for _ in 0..<66 {
    let angle = random() * .pi * 2, distance = sqrt(random()) * moon.r * 0.96
    let x = moon.x + cos(angle) * distance, y = moon.y + sin(angle) * distance
    let radius = 0.008 + random() * 0.046
    haze(x, y, radius * (0.8 + random() * 0.5), radius, rgb(106, 115, 123, 0.035 + random() * 0.08))
}
for (dx, dy, rx, ry) in [(-0.08, 0.08, 0.065, 0.090), (0.04, 0.015, 0.075, 0.052),
                          (0.11, -0.07, 0.050, 0.070), (-0.03, -0.12, 0.060, 0.040)] as [Quad] {
    haze(moon.x + dx, moon.y + dy, rx, ry, rgb(102, 111, 119, 0.15))
}
for _ in 0..<105 {
    let angle = random() * .pi * 2, distance = sqrt(random()) * moon.r
    let x = moon.x + cos(angle) * distance, y = moon.y + sin(angle) * distance
    let radius = 0.001 + random() * 0.005
    ctx.setStrokeColor(rgb(105, 109, 112, 0.045 + random() * 0.06))
    ctx.setLineWidth(0.00065 * s)
    ctx.strokeEllipse(in: rect(x - radius, y - radius, radius * 2, radius * 2))
    haze(x - radius * 0.4, y + radius * 0.4, radius, radius, rgb(255, 252, 229, 0.12))
}
ctx.restoreGState()
ctx.setStrokeColor(rgb(252, 241, 214, 0.22))
ctx.setLineWidth(s * 0.001)
ctx.strokeEllipse(in: disc.insetBy(dx: s * 0.0005, dy: s * 0.0005))

// Cloud banks have soft, uneven edges and leave the moon's outline legible.
for (x, y, rx, ry, alpha) in [(0.73, 0.505, 0.45, 0.025, 0.32), (0.37, 0.473, 0.37, 0.020, 0.35),
                             (0.82, 0.592, 0.35, 0.018, 0.21), (0.22, 0.744, 0.25, 0.021, 0.13)] as [Five] {
    haze(x, y, rx, ry, rgb(21, 17, 27, alpha))
}

// Small city buildings. Their asymmetry gives the skyline a drawn, inhabited character.
func house(_ x: CGFloat, _ base: CGFloat, _ w: CGFloat, _ h: CGFloat, _ colour: CGColor) {
    polygon([(x - w / 2, base), (x - w / 2, base + h * 0.70), (x - w * 0.14, base + h),
             (x + w / 2, base + h * 0.70), (x + w / 2, base)], colour)
    ctx.setFillColor(colour)
    ctx.fill(rect(x + w * 0.20, base + h * 0.72, w * 0.10, h * 0.28))
}
func spire(_ x: CGFloat, _ base: CGFloat, _ w: CGFloat, _ h: CGFloat, _ colour: CGColor) {
    polygon([(x - w * 0.40, base), (x - w * 0.40, base + h * 0.53),
             (x - w * 0.57, base + h * 0.53), (x - w * 0.46, base + h * 0.57),
             (x - w * 0.35, base + h * 0.58), (x, base + h),
             (x + w * 0.35, base + h * 0.58), (x + w * 0.46, base + h * 0.57),
             (x + w * 0.57, base + h * 0.53), (x + w * 0.40, base + h * 0.53),
             (x + w * 0.40, base)], colour)
}
ctx.setFillColor(rgb(56, 29, 43))
ctx.fill(rect(0, 0, 1, 0.25))
for (x, w, h) in [(0.02, 0.09, 0.10), (0.12, 0.08, 0.08), (0.23, 0.10, 0.12),
                    (0.34, 0.09, 0.06), (0.66, 0.08, 0.09), (0.80, 0.12, 0.12), (0.94, 0.13, 0.08)] as [Triple] {
    house(x, 0.22, w, h, rgb(53, 29, 43))
}
for (x, w, h) in [(0.13, 0.031, 0.26), (0.29, 0.036, 0.23), (0.63, 0.034, 0.26),
                    (0.76, 0.043, 0.32), (0.91, 0.026, 0.22)] as [Triple] {
    spire(x, 0.21, w, h, rgb(53, 30, 44))
}
haze(0.58, 0.30, 0.68, 0.105, rgb(182, 111, 119, 0.28))
haze(0.20, 0.285, 0.34, 0.07, rgb(134, 109, 124, 0.12))

// Mid-distance roofs and towers, with a little reflected moonlight on their right edges.
let middle = rgb(25, 19, 30)
ctx.setFillColor(middle)
ctx.fill(rect(0, 0, 1, 0.18))
for (x, w, h) in [(0.04, 0.12, 0.105), (0.17, 0.09, 0.08), (0.30, 0.12, 0.09),
                    (0.59, 0.10, 0.11), (0.80, 0.12, 0.11), (0.96, 0.13, 0.14)] as [Triple] {
    house(x, 0.15, w, h, middle)
}
spire(0.23, 0.14, 0.053, 0.32, middle)
spire(0.70, 0.14, 0.062, 0.40, middle)
spire(0.89, 0.14, 0.037, 0.25, middle)
line([(0.70, 0.54), (0.722, 0.372), (0.734, 0.368)], rgb(173, 133, 133, 0.30), width: 0.0013)
line([(0.23, 0.46), (0.249, 0.326)], rgb(173, 133, 133, 0.22), width: 0.001)
haze(0.67, 0.225, 0.48, 0.065, rgb(148, 69, 91, 0.15))

// The foreground cathedral: one readable silhouette, stepped cornices and narrow pinnacles.
let stone = rgb(12, 13, 21), face = rgb(19, 18, 27), edge = rgb(126, 111, 116, 0.34)
polygon([(0.325, 0.10), (0.325, 0.246), (0.362, 0.246), (0.362, 0.287),
         (0.45, 0.364), (0.538, 0.287), (0.538, 0.246), (0.575, 0.246), (0.575, 0.10)], stone)
spire(0.45, 0.285, 0.075, 0.425, stone)
// An angled roof plane, nearly black, catches a thin strip of moonlight.
polygon([(0.45, 0.710), (0.477, 0.5315), (0.45, 0.518)], rgb(29, 25, 34))
line([(0.45, 0.710), (0.477, 0.5315), (0.4928, 0.527)], rgb(182, 161, 153, 0.50), width: 0.0011)
line([(0.418, 0.510), (0.482, 0.510)], edge, width: 0.0016)
line([(0.42, 0.501), (0.480, 0.501)], rgb(66, 56, 67, 0.55), width: 0.0015)
polygon([(0.451, 0.308), (0.478, 0.326), (0.478, 0.499), (0.451, 0.499)], face)
// Side pinnacles and the steep main gable.
spire(0.362, 0.115, 0.028, 0.332, stone)
spire(0.538, 0.115, 0.028, 0.332, stone)
line([(0.45, 0.364), (0.524, 0.299)], edge, width: 0.0015)
line([(0.538, 0.447), (0.548, 0.308)], rgb(152, 132, 134, 0.29), width: 0.001)
// Buttresses step down into the neighbouring rooftops.
for x in [CGFloat(0.385), 0.515] {
    polygon([(x - 0.009, 0.10), (x - 0.009, 0.255), (x - 0.004, 0.269),
             (x + 0.004, 0.269), (x + 0.009, 0.255), (x + 0.009, 0.10)], rgb(17, 17, 25))
    line([(x + 0.009, 0.245), (x + 0.009, 0.138)], rgb(92, 73, 86, 0.29), width: 0.0015)
}

// Warm, pointed lancet windows. Their geometry stays quiet at small sizes.
func lancet(_ x: CGFloat, _ y: CGFloat, _ w: CGFloat, _ h: CGFloat, brightness: CGFloat = 1) {
    haze(x, y + h * 0.45, w * 2.4, h * 0.9, rgb(224, 136, 90, 0.14 * brightness))
    let path = CGMutablePath()
    path.move(to: p(x - w / 2, y))
    path.addLine(to: p(x - w / 2, y + h * 0.65))
    path.addCurve(to: p(x, y + h), control1: p(x - w / 2, y + h * 0.82), control2: p(x - w * 0.18, y + h * 0.96))
    path.addCurve(to: p(x + w / 2, y + h * 0.65), control1: p(x + w * 0.18, y + h * 0.96), control2: p(x + w / 2, y + h * 0.82))
    path.addLine(to: p(x + w / 2, y))
    path.closeSubpath()
    ctx.saveGState()
    ctx.addPath(path)
    ctx.clip()
    ctx.drawLinearGradient(gradient([(0, rgb(153, 76, 50, brightness)), (1, rgb(233, 187, 123, brightness))]),
                           start: p(x, y), end: p(x, y + h), options: [])
    ctx.restoreGState()
    line([(x, y), (x, y + h * 0.84)], rgb(18, 14, 21, 0.80), width: 0.0014)
}
lancet(0.438, 0.426, 0.009, 0.033, brightness: 0.74)
lancet(0.462, 0.426, 0.009, 0.033, brightness: 0.92)
lancet(0.45, 0.348, 0.012, 0.037, brightness: 0.64)
lancet(0.412, 0.168, 0.012, 0.045, brightness: 0.66)
lancet(0.488, 0.168, 0.012, 0.045, brightness: 0.82)
// A small rose window is the only ornamental circle below the moon.
let rose = p(0.45, 0.270), rr = 0.020 * s
haze(0.45, 0.270, 0.047, 0.047, rgb(227, 143, 95, 0.15))
ctx.setFillColor(rgb(179, 117, 83, 0.80))
ctx.fillEllipse(in: CGRect(x: rose.x - rr, y: rose.y - rr, width: rr * 2, height: rr * 2))
ctx.setStrokeColor(rgb(18, 15, 23))
ctx.setLineWidth(s * 0.003)
ctx.strokeEllipse(in: CGRect(x: rose.x - rr * 0.77, y: rose.y - rr * 0.77, width: rr * 1.54, height: rr * 1.54))
for i in 0..<8 {
    let angle = CGFloat(i) * .pi / 4
    line([(0.45, 0.270), (0.45 + cos(angle) * 0.020, 0.270 + sin(angle) * 0.020)], rgb(18, 15, 23), width: 0.002)
}
ctx.setFillColor(rgb(18, 15, 23))
ctx.fillEllipse(in: rect(0.446, 0.266, 0.008, 0.008))

// Low foreground roofs frame the cathedral and hide its base in shadow.
let foreground = rgb(9, 11, 18)
ctx.setFillColor(foreground)
ctx.fill(rect(0, 0, 1, 0.095))
for (x, base, w, h) in [(0.025, 0.04, 0.15, 0.17), (0.14, 0.035, 0.12, 0.17),
                         (0.275, 0.02, 0.13, 0.16), (0.625, 0.035, 0.13, 0.17),
                         (0.805, 0.025, 0.16, 0.18), (0.965, 0.025, 0.16, 0.20)] as [Quad] {
    house(x, base, w, h, foreground)
}
for (x, y) in [(0.135, 0.115), (0.267, 0.087), (0.622, 0.115), (0.80, 0.105), (0.965, 0.135)] as [Pair] {
    lancet(x, y, 0.007, 0.017, brightness: 0.56)
}

// A gentle edge vignette keeps the composition intact under the system's rounded mask.
ctx.drawRadialGradient(gradient([(0, rgb(0, 0, 0, 0)), (0.62, rgb(0, 0, 0, 0)), (1, rgb(2, 3, 9, 0.46))]),
                       startCenter: p(0.52, 0.56), startRadius: 0,
                       endCenter: p(0.52, 0.56), endRadius: s * 0.73, options: [.drawsAfterEndLocation])

guard let image = ctx.makeImage(),
      let png = NSBitmapImageRep(cgImage: image).representation(using: .png, properties: [:]) else { exit(1) }
do { try png.write(to: URL(fileURLWithPath: args[1])) } catch {
    FileHandle.standardError.write("make_icon: \(error)\n".data(using: .utf8)!)
    exit(1)
}
