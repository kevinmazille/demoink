import AppKit

/// Kind of annotation. The `LineType` can flip live during a drag according to
/// the held modifiers, exactly like the Windows `LineType` in `MainWindow.h`.
enum LineType {
    case hand
    case straight
    case arrow
    case rectangle
    case ellipse
    case text
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
    /// Text-mode fields. `lineStart` doubles as the text origin (top-left of the
    /// em box, like the Windows `lineStartPoint`).
    var text: String = ""
    var fontSize: CGFloat = DrawModel.defaultFontSize
    var fontName: String = DrawModel.defaultFontName
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

/// Decorative "board" behind the annotations. Mirrors `BoardStyle` in
/// MainWindow.h. Coupled to the theme like Windows: FrameA↔Light (light
/// whiteboard), FrameB↔Dark (dark slate). `Z` cycles None→A→B→A; `Q` resets
/// it to None. Each frame is vector-drawn (a user image replacement comes with
/// the settings stage).
enum BoardStyle {
    case none
    case frameA
    case frameB
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

    /// Text mode. Default size 30 like the Windows `Text/defaultsize`; wheel
    /// adjusts by 4 within [8, 256]. The font list favours a handwritten look
    /// (the Windows default was Segoe Print) but every entry is a macOS system
    /// font, with `resolveTextFont` falling back to the first available.
    static let defaultFontSize: CGFloat = 30
    static let minFontSize: CGFloat = 8
    static let maxFontSize: CGFloat = 256
    static let fontStep: CGFloat = 4
    static let textFonts = ["Avenir Next", "Helvetica Neue", "Bradley Hand", "Marker Felt"]
    static var defaultFontName: String { textFonts[0] }

    /// Mirrors the Windows `ResolveTextFont`: the configured (here: default)
    /// font if installed, else the first available in the list, else the system
    /// font. Guarantees `NSFont(name:size:)` never comes back nil at draw time.
    static func resolveTextFont(_ name: String, size: CGFloat) -> NSFont {
        if let f = NSFont(name: name, size: size) { return f }
        for candidate in textFonts {
            if let f = NSFont(name: candidate, size: size) { return f }
        }
        return NSFont.systemFont(ofSize: size)
    }

    /// Built-in solid background fills for the Light / Dark themes (Win32
    /// DEFAULT_BG_*). The live fills are read from `Settings` (editable via the
    /// Background tab), falling back to these.
    static let defaultBackgroundLight = NSColor(srgbRed: 1, green: 1, blue: 1, alpha: 1)
    static let defaultBackgroundDark = NSColor(srgbRed: 0, green: 0, blue: 0, alpha: 1)

    /// The live solid fill for a theme: the user-edited color when set, else the
    /// built-in default.
    static func background(for theme: Theme) -> NSColor {
        Settings.backgroundColor(dark: theme.isDark)
    }

    /// Built-in light palette, shared with the Transparent theme on Windows.
    /// `DEFAULT_COLORS_LIGHT` in MainWindow.h. The live palette is read from
    /// `Settings` (editable via the Colors tab), falling back to this.
    static let defaultLightPalette: [NSColor] = [
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

    /// Built-in dark palette, tuned for a black background. `DEFAULT_COLORS_DARK`.
    static let defaultDarkPalette: [NSColor] = [
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

    /// The live palette: user-edited colors from `Settings` when present, else
    /// the built-in default for the theme.
    static func palette(for theme: Theme) -> [NSColor] {
        Settings.palette(dark: theme.isDark)
    }

    static var defaultPalette: (light: [NSColor], dark: [NSColor]) {
        (defaultLightPalette, defaultDarkPalette)
    }

    /// Alpha the theme draws ink at: opaque on Dark, semi-transparent otherwise.
    static func alpha(for theme: Theme) -> Int {
        theme.isDark ? opaqueAlpha : lineAlpha
    }

    static func color(atIndex index: Int, alpha: Int, theme: Theme) -> NSColor {
        palette(for: theme)[index].withAlphaComponent(CGFloat(alpha) / 255.0)
    }
}
