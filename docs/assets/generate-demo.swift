import AppKit
import ImageIO
import UniformTypeIdentifiers

let width: CGFloat = 900
let height: CGFloat = 520
let output = URL(fileURLWithPath: "docs/assets/ctree-demo.gif")

let background = NSColor(calibratedRed: 0.055, green: 0.067, blue: 0.09, alpha: 1)
let panel = NSColor(calibratedRed: 0.09, green: 0.11, blue: 0.145, alpha: 1)
let foreground = NSColor(calibratedRed: 0.86, green: 0.89, blue: 0.92, alpha: 1)
let muted = NSColor(calibratedRed: 0.48, green: 0.55, blue: 0.62, alpha: 1)
let cyan = NSColor(calibratedRed: 0.31, green: 0.82, blue: 0.86, alpha: 1)
let yellow = NSColor(calibratedRed: 0.96, green: 0.76, blue: 0.32, alpha: 1)
let green = NSColor(calibratedRed: 0.45, green: 0.82, blue: 0.55, alpha: 1)

let regular = NSFont.monospacedSystemFont(ofSize: 20, weight: .regular)
let bold = NSFont.monospacedSystemFont(ofSize: 20, weight: .semibold)
let small = NSFont.monospacedSystemFont(ofSize: 15, weight: .medium)

func text(_ value: String, _ x: CGFloat, _ y: CGFloat, color: NSColor = foreground,
          font: NSFont = regular) {
    value.draw(at: NSPoint(x: x, y: y), withAttributes: [
        .font: font,
        .foregroundColor: color
    ])
}

func roundedRect(_ rect: NSRect, radius: CGFloat, color: NSColor) {
    color.setFill()
    NSBezierPath(roundedRect: rect, xRadius: radius, yRadius: radius).fill()
}

struct Scene {
    let selected: Int
    let popup: Bool
    let shell: Bool
    let hint: String
    let duration: Double
}

let scenes = [
    Scene(selected: 0, popup: false, shell: false, hint: "ctree ~/Projects", duration: 1.3),
    Scene(selected: 1, popup: false, shell: false, hint: "↓  navigate", duration: 0.8),
    Scene(selected: 2, popup: false, shell: false, hint: "↓  navigate", duration: 0.8),
    Scene(selected: 2, popup: true, shell: false, hint: "Space  preview children", duration: 2.0),
    Scene(selected: 2, popup: false, shell: false, hint: "Esc  close preview", duration: 0.8),
    Scene(selected: 2, popup: true, shell: false, hint: "Click  preview • click again to enter", duration: 2.0),
    Scene(selected: 2, popup: false, shell: true, hint: "Enter  change the current shell directory", duration: 2.2),
]

let tree = [
    "~/Projects",
    "├── api-server",
    "├── clickable-tree",
    "├── design-system",
    "└── notes",
]

func makeFrame(_ scene: Scene) -> CGImage {
    let image = NSImage(size: NSSize(width: width, height: height))
    image.lockFocus()
    background.setFill()
    NSRect(x: 0, y: 0, width: width, height: height).fill()

    roundedRect(NSRect(x: 26, y: 24, width: width - 52, height: height - 48), radius: 14, color: panel)
    NSColor(calibratedWhite: 0.15, alpha: 1).setFill()
    NSRect(x: 26, y: height - 82, width: width - 52, height: 58).fill()
    for (i, color) in [NSColor.systemRed, .systemYellow, .systemGreen].enumerated() {
        color.setFill()
        NSBezierPath(ovalIn: NSRect(x: 48.0 + CGFloat(i * 25), y: height - 60, width: 12, height: 12)).fill()
    }
    text("ctree — interactive directory tree", 145, height - 67, color: muted, font: small)

    if scene.shell {
        text("~/Projects/clickable-tree", 58, height - 132, color: cyan, font: bold)
        text("% pwd", 58, height - 174, color: foreground)
        text("/Users/demo/Projects/clickable-tree", 58, height - 216, color: green)
        text("%", 58, height - 272, color: cyan, font: bold)
        text("✓ current shell directory changed", 58, 92, color: green, font: bold)
    } else {
        for (index, line) in tree.enumerated() {
            let y = height - 132 - CGFloat(index * 42)
            if index == scene.selected {
                roundedRect(NSRect(x: 48, y: y - 6, width: 390, height: 34), radius: 5,
                            color: NSColor(calibratedRed: 0.12, green: 0.35, blue: 0.40, alpha: 1))
            }
            text(line, 58, y, color: index == 0 ? cyan : foreground,
                 font: index == scene.selected ? bold : regular)
        }

        if scene.popup {
            roundedRect(NSRect(x: 458, y: 180, width: 370, height: 194), radius: 9,
                        color: NSColor(calibratedRed: 0.075, green: 0.09, blue: 0.12, alpha: 1))
            yellow.setStroke()
            let border = NSBezierPath(roundedRect: NSRect(x: 458, y: 180, width: 370, height: 194),
                                      xRadius: 9, yRadius: 9)
            border.lineWidth = 2
            border.stroke()
            text("clickable-tree/", 482, 332, color: yellow, font: bold)
            text("├── docs/", 482, 290)
            text("├── shell/", 482, 252)
            text("├── ctree.c", 482, 214)
            text("└── README.md", 482, 176)
        }
        text("j/k move   Space/Right preview   Enter cd   q quit", 58, 78, color: muted, font: small)
    }

    roundedRect(NSRect(x: 248, y: 28, width: 404, height: 39), radius: 18,
                color: NSColor(calibratedWhite: 0.02, alpha: 0.94))
    let hintWidth = (scene.hint as NSString).size(withAttributes: [.font: small]).width
    text(scene.hint, CGFloat(width) / 2 - hintWidth / 2, 39, color: yellow, font: small)

    image.unlockFocus()
    var rect = NSRect(origin: .zero, size: image.size)
    return image.cgImage(forProposedRect: &rect, context: nil, hints: nil)!
}

guard let destination = CGImageDestinationCreateWithURL(
    output as CFURL, UTType.gif.identifier as CFString, scenes.count, nil
) else { fatalError("Could not create GIF destination") }

CGImageDestinationSetProperties(destination, [
    kCGImagePropertyGIFDictionary: [kCGImagePropertyGIFLoopCount: 0]
] as CFDictionary)

for scene in scenes {
    CGImageDestinationAddImage(destination, makeFrame(scene), [
        kCGImagePropertyGIFDictionary: [kCGImagePropertyGIFDelayTime: scene.duration]
    ] as CFDictionary)
}

guard CGImageDestinationFinalize(destination) else { fatalError("Could not write GIF") }
print("Wrote \(output.path)")
