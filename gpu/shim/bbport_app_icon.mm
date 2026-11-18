// SPDX-License-Identifier: GPL-2.0-or-later
#include "bbport_app_icon.h"

#import <AppKit/AppKit.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace BbAppIcon {
namespace {

// Apple's icon grid (the 824 tile centred in a 1024 canvas),
// a superellipse |x|^5 + |y|^5 = 1 for the continuous corners, and the template's shadow.
constexpr int kCanvas = 1024;
constexpr CGFloat kTile = 824;

CGPathRef TilePath(CGRect rect) {
    CGMutablePathRef path = CGPathCreateMutable();
    const double n = 5.0;
    const int steps = 720;
    for (int i = 0; i <= steps; ++i) {
        const double t = double(i) / steps * 2 * M_PI;
        const double c = std::cos(t), s = std::sin(t);
        const double x = std::copysign(std::pow(std::fabs(c), 2 / n), c);
        const double y = std::copysign(std::pow(std::fabs(s), 2 / n), s);
        const CGFloat px = CGRectGetMidX(rect) + CGFloat(x) * rect.size.width / 2;
        const CGFloat py = CGRectGetMidY(rect) + CGFloat(y) * rect.size.height / 2;
        if (i == 0) {
            CGPathMoveToPoint(path, nullptr, px, py);
        } else {
            CGPathAddLineToPoint(path, nullptr, px, py);
        }
    }
    CGPathCloseSubpath(path);
    return path;
}

CGImageRef Rounded(CGImageRef image) {
    CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef ctx = CGBitmapContextCreate(nullptr, kCanvas, kCanvas, 8, 0, space,
                                             kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(space);
    if (!ctx) {
        return nullptr;
    }
    CGContextSetInterpolationQuality(ctx, kCGInterpolationHigh);
    const CGFloat inset = (kCanvas - kTile) / 2;
    const CGRect rect = CGRectMake(inset, inset, kTile, kTile);
    CGPathRef path = TilePath(rect);
    CGContextSaveGState(ctx);
    CGColorRef shadow = CGColorCreateGenericGray(0, 0.3);
    CGContextSetShadowWithColor(ctx, CGSizeMake(0, -12), 28, shadow);
    CGColorRelease(shadow);
    CGContextAddPath(ctx, path);
    CGContextSetGrayFillColor(ctx, 0, 1);
    CGContextFillPath(ctx);
    CGContextRestoreGState(ctx);
    CGContextAddPath(ctx, path);
    CGContextClip(ctx);
    CGContextDrawImage(ctx, rect, image);
    CGPathRelease(path);
    CGImageRef out = CGBitmapContextCreateImage(ctx);
    CGContextRelease(ctx);
    return out;
}

} // namespace

void ApplyGameIcon() {
    const char* path = std::getenv("BB_GAME_ICON");
    if (!path || !path[0]) {
        return;
    }
    // The game window always shows the game's artwork, and only it does: the launcher keeps the
    // app's own icon (LauncherModel.removeGameIconFromBundle). Set before SDL makes this a
    // regular app, so its Dock tile starts with the game's icon instead of switching from the
    // bundle's (the host runs inside the same bundle).
    NSString* file = [NSString stringWithUTF8String:path];
    void (^apply)(void) = ^{
        [NSApplication sharedApplication];
        NSImage* art = [[NSImage alloc] initWithContentsOfFile:file];
        if (!art) {
            std::printf("Window: no game icon at %s: the app's icon stays\n", file.UTF8String);
            return;
        }
        CGImageRef square = [art CGImageForProposedRect:nullptr context:nil hints:nil];
        CGImageRef rounded = square ? Rounded(square) : nullptr;
        if (rounded) {
            NSApp.applicationIconImage = [[NSImage alloc] initWithCGImage:rounded size:NSMakeSize(512, 512)];
            CGImageRelease(rounded);
        } else {
            NSApp.applicationIconImage = art;
        }
        std::printf("Window: Dock icon from %s\n", file.UTF8String);
    };
    if ([NSThread isMainThread]) {
        apply();
    } else {
        dispatch_async(dispatch_get_main_queue(), apply);
    }
}

} // namespace BbAppIcon
