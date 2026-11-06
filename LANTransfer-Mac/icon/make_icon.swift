import AppKit

let S: CGFloat = 512
let img = NSImage(size: NSSize(width: S, height: S))
img.lockFocus()
let ctx = NSGraphicsContext.current!.cgContext

// Arka plan: yuvarlatılmış kare, mavi→turkuaz degrade
let inset: CGFloat = 24
let rect = CGRect(x: inset, y: inset, width: S - 2*inset, height: S - 2*inset)
let bg = CGPath(roundedRect: rect, cornerWidth: 104, cornerHeight: 104, transform: nil)
ctx.saveGState()
ctx.setShadow(offset: CGSize(width: 0, height: -8), blur: 18, color: NSColor.black.withAlphaComponent(0.35).cgColor)
ctx.addPath(bg); ctx.setFillColor(NSColor.black.cgColor); ctx.fillPath()
ctx.restoreGState()
ctx.saveGState()
ctx.addPath(bg); ctx.clip()
let grad = CGGradient(colorsSpace: CGColorSpaceCreateDeviceRGB(), colors: [
    NSColor(red: 0.10, green: 0.36, blue: 0.95, alpha: 1).cgColor,
    NSColor(red: 0.05, green: 0.78, blue: 0.80, alpha: 1).cgColor] as CFArray, locations: [0, 1])!
ctx.drawLinearGradient(grad, start: CGPoint(x: 0, y: S), end: CGPoint(x: S, y: 0), options: [])
// üst parlama
ctx.setFillColor(NSColor.white.withAlphaComponent(0.10).cgColor)
ctx.fillEllipse(in: CGRect(x: -80, y: S*0.55, width: S + 160, height: S*0.8))
ctx.restoreGState()

// Ok çizici: yatay, kalın, yuvarlak uçlu
func arrow(y: CGFloat, fromX: CGFloat, toX: CGFloat, alpha: CGFloat) {
    let dir: CGFloat = toX > fromX ? 1 : -1
    let head: CGFloat = 62
    ctx.setStrokeColor(NSColor.white.withAlphaComponent(alpha).cgColor)
    ctx.setLineWidth(40); ctx.setLineCap(.round); ctx.setLineJoin(.round)
    ctx.move(to: CGPoint(x: fromX, y: y)); ctx.addLine(to: CGPoint(x: toX, y: y)); ctx.strokePath()
    ctx.move(to: CGPoint(x: toX - dir*head, y: y + head))
    ctx.addLine(to: CGPoint(x: toX, y: y))
    ctx.addLine(to: CGPoint(x: toX - dir*head, y: y - head))
    ctx.strokePath()
}
ctx.saveGState()
ctx.setShadow(offset: CGSize(width: 0, height: -4), blur: 10, color: NSColor.black.withAlphaComponent(0.25).cgColor)
arrow(y: 318, fromX: 140, toX: 372, alpha: 1.0)   // sağa
arrow(y: 194, fromX: 372, toX: 140, alpha: 0.85)  // sola
ctx.restoreGState()

img.unlockFocus()
let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: 512, pixelsHigh: 512, bitsPerSample: 8,
    samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
rep.size = img.size
NSGraphicsContext.saveGraphicsState()
NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
img.draw(in: NSRect(x: 0, y: 0, width: 512, height: 512))
NSGraphicsContext.restoreGraphicsState()
try! rep.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: "icon_512.png"))
