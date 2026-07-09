import AppKit

/// Kind of annotation. Only `.hand` (freehand) exists in Stage 2; shapes
/// (straight/arrow/rectangle/ellipse) and text arrive in later stages.
/// Mirrors `LineType` in the Windows `MainWindow.h`.
enum LineType {
    case hand
}

/// One annotation stroke. Deliberately faithful to the Windows `DrawLine`
/// model (MainWindow.h) so later stages can extend it without reshaping the
/// port: points captured live, plus the color/width/alpha frozen at draw time.
struct DrawLine {
    var lineType: LineType = .hand
    var points: [CGPoint] = []
    var colorIndex: Int = DrawModel.defaultColorIndex
    var penWidth: CGFloat = DrawModel.defaultPenWidth
    /// 0...255, matching the Win32 BYTE alpha.
    var alpha: Int = DrawModel.lineAlpha
}

/// Constants and the color palette carried over from the Windows build.
enum DrawModel {
    /// Semi-transparent ink on the Transparent/Light theme (Win32 `LINE_ALPHA`).
    static let lineAlpha = 100

    /// Red at launch — matches the Windows "Rouge au lancement" default
    /// (`m_colorIndex(1)`; index 1 of the light palette is pure red).
    static let defaultColorIndex = 1
    static let defaultPenWidth: CGFloat = 6
    static let minPenWidth: CGFloat = 1
    static let maxPenWidth: CGFloat = 32

    /// Light palette, shared with the Transparent theme on Windows.
    /// `DEFAULT_COLORS_LIGHT` in MainWindow.h.
    static let lightPalette: [NSColor] = [
        NSColor(srgbRed: 255 / 255, green: 255 / 255, blue:   0 / 255, alpha: 1),
        NSColor(srgbRed: 255 / 255, green:   0 / 255, blue:   0 / 255, alpha: 1),
        NSColor(srgbRed:   0 / 255, green:  80 / 255, blue: 220 / 255, alpha: 1),
        NSColor(srgbRed:   0 / 255, green: 170 / 255, blue:   0 / 255, alpha: 1),
        NSColor(srgbRed: 150 / 255, green:   0 / 255, blue:   0 / 255, alpha: 1),
        NSColor(srgbRed:   0 / 255, green:   0 / 255, blue: 150 / 255, alpha: 1),
        NSColor(srgbRed:   0 / 255, green: 100 / 255, blue:   0 / 255, alpha: 1),
        NSColor(srgbRed:   0 / 255, green:   0 / 255, blue:   0 / 255, alpha: 1),
        NSColor(srgbRed: 120 / 255, green: 120 / 255, blue: 120 / 255, alpha: 1),
        NSColor(srgbRed: 200 / 255, green:   0 / 255, blue: 200 / 255, alpha: 1),
    ]

    static func color(atIndex index: Int, alpha: Int) -> NSColor {
        lightPalette[index].withAlphaComponent(CGFloat(alpha) / 255.0)
    }
}
