import Foundation
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers
// Dainvo Office app icon generator (macOS: swiftc -O -o make-app-icon make-app-icon.swift)
// usage: make-app-icon dainvo/branding/app-icon-source.png <out.png> <canvas> <contentFraction>
//   macOS icon set: canvas 1024, fraction 0.8047 (Apple grid); Linux/Windows: 0.9375
// then resize with sips into sysui/desktop/icons/macos/app-launcher/main.iconset,
// sysui/desktop/icons/hicolor/*/apps/{main,startcenter}.png and soffice.ico
let a = CommandLine.arguments
let src = CGImageSourceCreateWithURL(URL(fileURLWithPath: a[1]) as CFURL, nil)!
let img = CGImageSourceCreateImageAtIndex(src, 0, nil)!
let canvas = Int(a[3])!, frac = Double(a[4])!
// blue square bounds in the source (top-down y), measured: x 111-1141, y 127-1142
let bx = 111.0, by = 127.0, bw = 1031.0, bh = 1016.0
let crop = img.cropping(to: CGRect(x: bx, y: by, width: bw, height: bh))!
let side = Double(canvas) * frac
let off = (Double(canvas) - side) / 2
let cs = CGColorSpace(name: CGColorSpace.sRGB)!
let ctx = CGContext(data: nil, width: canvas, height: canvas, bitsPerComponent: 8, bytesPerRow: 0, space: cs, bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
ctx.interpolationQuality = .high
let rect = CGRect(x: off, y: off, width: side, height: side)
// clean edge: clip to a rounded square matching the artwork's corners
ctx.addPath(CGPath(roundedRect: rect, cornerWidth: side*0.185, cornerHeight: side*0.185, transform: nil))
ctx.clip()
ctx.draw(crop, in: rect)
let out = ctx.makeImage()!
let dest = CGImageDestinationCreateWithURL(URL(fileURLWithPath: a[2]) as CFURL, UTType.png.identifier as CFString, 1, nil)!
CGImageDestinationAddImage(dest, out, nil)
CGImageDestinationFinalize(dest)
