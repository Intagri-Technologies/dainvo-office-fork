// Render an SVG to a PNG at its own size times <scale>, using macOS's
// built-in SVG support (NSImage, macOS 14+). Used by make_branding.py.
// usage: svg2png <in.svg> <out.png> <width> <height>
import AppKit

let a = CommandLine.arguments
guard a.count == 5, let img = NSImage(contentsOf: URL(fileURLWithPath: a[1])),
      let w = Int(a[3]), let h = Int(a[4]) else { fatalError("usage: svg2png in.svg out.png width height") }
let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: w, pixelsHigh: h, bitsPerSample: 8,
                           samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
                           colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
NSGraphicsContext.saveGraphicsState()
NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
NSGraphicsContext.current?.imageInterpolation = .high
img.draw(in: NSRect(x: 0, y: 0, width: w, height: h))
NSGraphicsContext.restoreGraphicsState()
try! rep.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: a[2]))
