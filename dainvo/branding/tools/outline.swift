import CoreText
import CoreGraphics
import Foundation
// outline <font.ttf> <weight> <size> <text>  -> prints SVG path data (baseline y=0) and advance width
let a = CommandLine.arguments
let url = URL(fileURLWithPath: a[1]) as CFURL
let descs = CTFontManagerCreateFontDescriptorsFromURL(url) as! [CTFontDescriptor]
let wghtTag = 0x77676874 // 'wght'
let desc = CTFontDescriptorCreateCopyWithAttributes(descs[0], [kCTFontVariationAttribute: [wghtTag: Double(a[2])!]] as CFDictionary)
let font = CTFontCreateWithFontDescriptor(desc, CGFloat(Double(a[3])!), nil)
let line = CTLineCreateWithAttributedString(NSAttributedString(string: a[4], attributes: [NSAttributedString.Key(kCTFontAttributeName as String): font]))
var d = ""
func f(_ v: CGFloat) -> String { String(format: "%.2f", v) }
for run in CTLineGetGlyphRuns(line) as! [CTRun] {
  let n = CTRunGetGlyphCount(run)
  var glyphs = [CGGlyph](repeating: 0, count: n); var pos = [CGPoint](repeating: .zero, count: n)
  CTRunGetGlyphs(run, CFRange(location: 0, length: n), &glyphs); CTRunGetPositions(run, CFRange(location: 0, length: n), &pos)
  for i in 0..<n {
    guard let p = CTFontCreatePathForGlyph(font, glyphs[i], nil) else { continue }
    let ox = pos[i].x
    p.applyWithBlock { el in
      let e = el.pointee; let pts = e.points
      switch e.type {
      case .moveToPoint: d += "M\(f(pts[0].x+ox)) \(f(-pts[0].y))"
      case .addLineToPoint: d += "L\(f(pts[0].x+ox)) \(f(-pts[0].y))"
      case .addQuadCurveToPoint: d += "Q\(f(pts[0].x+ox)) \(f(-pts[0].y)) \(f(pts[1].x+ox)) \(f(-pts[1].y))"
      case .addCurveToPoint: d += "C\(f(pts[0].x+ox)) \(f(-pts[0].y)) \(f(pts[1].x+ox)) \(f(-pts[1].y)) \(f(pts[2].x+ox)) \(f(-pts[2].y))"
      case .closeSubpath: d += "Z"
      @unknown default: break
      }
    }
  }
}
print(f(CTLineGetTypographicBounds(line, nil, nil, nil)))
print(d)
