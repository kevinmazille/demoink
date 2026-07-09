import AppKit

/// Kind of annotation. Text arrives in a later stage. The `LineType` can flip
/// live during a drag according to the held modifiers, exactly like the Windows
/// `LineType` in `MainWindow.h`.
enum LineType {
    case hand
    case straight
    case arrow
    case rectangle
    case ellipse
}

/// One annotation stroke. Deliberately faithful to the Windows `DrawLine`
/// model (MainWindow.h): `points` holds the live freehand polyline, while
/// `lineStart`/`lineEnd` drive the two-point shapes (straight/arrow/rect/
/// ellipse). Color/width/alpha are frozen at draw time.
struct DrawLine {
    var lineType: LineType = .hand
    var points: [CGPoint] = []
    /// nil until the drag sets the far corner/endpoint (shapes only).
    var lineStart: CGPoint = .zero
    var lineEnd: CGPoint?
    var colorIndex: Int = DrawModel.defaultColorIndex
    var penWidth: CGFloat = DrawModel.defaultPenWidth
    /// 0...255, matching the Win32 BYTE alpha.
    var alpha: Int = DrawModel.lineAlpha
}

/// Overlay theme. Mirrors `Theme` in MainWindow.h.
/// - `.transparent`: clear overlay, the desktop shows through (pristine start).
/// - `.light`: solid light fill, light palette, semi-transparent ink.
/// - `.dark`: solid dark fill, dark palette, opaque ink.
enum Theme {
    case transparent
    case light
    case dark

    var isDark: Bool { self == .dark }
}

/// Constants and the color palettes carried over from the Windows build.
enum DrawModel {
    /// Semi-transparent ink on the Transparent/Light theme (Win32 `LINE_ALPHA`).
    static let lineAlpha = 100
    static let opaqueAlpha = 255

    /// Red at launch — matches the Windows "Rouge au lancement" default
    /// (`m_colorIndex(1)`; index 1 of the light palette is pure red).
    static let defaultColorIndex = 1
    static let defaultPenWidth: CGFloat = 6
    static let minPenWidth: CGFloat = 1
    static let maxPenWidth: CGFloat = 32

    /// Solid background fills for the Light / Dark themes (Win32 DEFAULT_BG_*).
    static let backgroundLight = NSColor(srgbRed: 1, green: 1, blue: 1, alpha: 1)
    static let backgroundDark = NSColor(srgbRed: 0, green: 0, blue: 0, alpha: 1)

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

    /// Dark palette, tuned for a black background. `DEFAULT_COLORS_DARK`.
    static let darkPalette: [NSColor] = [
        NSColor(srgbRed: 255 / 255, green: 255 / 255, blue:   0 / 255, alpha: 1),
        NSColor(srgbRed: 255 / 255, green: 140 / 255, blue:   0 / 255, alpha: 1),
        NSColor(srgbRed: 255 / 255, green:  90 / 255, blue:  90 / 255, alpha: 1),
        NSColor(srgbRed:   0 / 255, green: 220 / 255, blue: 255 / 255, alpha: 1),
        NSColor(srgbRed: 220 / 255, green: 150 / 255, blue:  80 / 255, alpha: 1),
        NSColor(srgbRed: 255 / 255, green: 120 / 255, blue: 150 / 255, alpha: 1),
        NSColor(srgbRed: 140 / 255, green: 180 / 255, blue: 255 / 255, alpha: 1),
        NSColor(srgbRed: 255 / 255, green: 255 / 255, blue: 255 / 255, alpha: 1),
        NSColor(srgbRed: 180 / 255, green: 180 / 255, blue: 180 / 255, alpha: 1),
        NSColor(srgbRed: 120 / 255, green: 255 / 255, blue: 120 / 255, alpha: 1),
    ]

    static func palette(for theme: Theme) -> [NSColor] {
        theme.isDark ? darkPalette : lightPalette
    }

    /// Alpha the theme draws ink at: opaque on Dark, semi-transparent otherwise.
    static func alpha(for theme: Theme) -> Int {
        theme.isDark ? opaqueAlpha : lineAlpha
    }

    static func color(atIndex index: Int, alpha: Int, theme: Theme) -> NSColor {
        palette(for: theme)[index].withAlphaComponent(CGFloat(alpha) / 255.0)
    }
}
