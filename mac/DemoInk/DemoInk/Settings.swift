import AppKit

/// Central typed accessor over `UserDefaults` for the user-configurable defaults,
/// the macOS idiom replacing the Windows `.ini`. Each property falls back to the
/// hard-coded `DrawModel` constant when never set, so a fresh install behaves
/// exactly as before the settings UI existed.
///
/// These are the *launch* defaults a new draw session starts from; live changes
/// during a session (wheel, arrows, digits) are not written back, mirroring the
/// Windows behaviour ("épaisseur non persistée d'une session à l'autre").
enum Settings {
    private static var d: UserDefaults { .standard }

    // MARK: - Draw

    static var defaultColorIndex: Int {
        get {
            guard d.object(forKey: Key.drawColorIndex) != nil else { return DrawModel.defaultColorIndex }
            // Clamp defensively: the palette is 10 colors (0...9).
            return min(max(d.integer(forKey: Key.drawColorIndex), 0), 9)
        }
        set { d.set(newValue, forKey: Key.drawColorIndex) }
    }

    static var defaultPenWidth: CGFloat {
        get {
            guard d.object(forKey: Key.drawPenWidth) != nil else { return DrawModel.defaultPenWidth }
            let w = CGFloat(d.double(forKey: Key.drawPenWidth))
            return min(max(w, DrawModel.minPenWidth), DrawModel.maxPenWidth)
        }
        set { d.set(Double(newValue), forKey: Key.drawPenWidth) }
    }

    // MARK: - Text

    static var defaultFontName: String {
        get { d.string(forKey: Key.textFontName) ?? DrawModel.defaultFontName }
        set { d.set(newValue, forKey: Key.textFontName) }
    }

    static var defaultFontSize: CGFloat {
        get {
            guard d.object(forKey: Key.textFontSize) != nil else { return DrawModel.defaultFontSize }
            let s = CGFloat(d.double(forKey: Key.textFontSize))
            return min(max(s, DrawModel.minFontSize), DrawModel.maxFontSize)
        }
        set { d.set(Double(newValue), forKey: Key.textFontSize) }
    }

    // MARK: - Color palettes

    /// The 10-color palette for the light or dark theme: user-edited colors when
    /// present in UserDefaults, else the built-in `DrawModel` default. Stored as
    /// an array of "#RRGGBB" hex strings; a malformed or wrong-length array is
    /// ignored so a bad write can't corrupt the palette.
    static func palette(dark: Bool) -> [NSColor] {
        let fallback = dark ? DrawModel.defaultDarkPalette : DrawModel.defaultLightPalette
        guard let hexes = d.stringArray(forKey: paletteKey(dark: dark)), hexes.count == 10 else {
            return fallback
        }
        return hexes.enumerated().map { i, hex in NSColor(hex: hex) ?? fallback[i] }
    }

    /// Sets a single swatch (0...9) in the light or dark palette, preserving the
    /// others. Writes the whole array back as hex.
    static func setPaletteColor(_ color: NSColor, atIndex index: Int, dark: Bool) {
        guard (0..<10).contains(index) else { return }
        var current = palette(dark: dark)
        current[index] = color
        d.set(current.map { $0.hexString }, forKey: paletteKey(dark: dark))
    }

    /// Restores one theme's palette to the built-in default.
    static func resetPalette(dark: Bool) {
        d.removeObject(forKey: paletteKey(dark: dark))
    }

    private static func paletteKey(dark: Bool) -> String {
        dark ? "Colors.dark" : "Colors.light"
    }

    // MARK: - Shortcuts

    /// The in-overlay letter shortcuts, rebindable like the Windows Shortcuts tab.
    /// Only these single-letter actions are configurable; the arrows, digits,
    /// Esc, and Delete stay fixed (they mirror the Windows "reste fixe" rule).
    enum ShortcutAction: String, CaseIterable {
        case text        // enter text mode      (default A)
        case eraseAll    // erase all strokes     (default W)
        case cycleTheme  // Transparent/Light/Dark (default Q)
        case cycleBoard  // board frame None/A/B   (default Z)

        var title: String {
            switch self {
            case .text: return "Text mode"
            case .eraseAll: return "Erase all"
            case .cycleTheme: return "Cycle background"
            case .cycleBoard: return "Cycle board frame"
            }
        }

        /// The built-in default key (lowercased single character).
        var defaultKey: String {
            switch self {
            case .text: return "a"
            case .eraseAll: return "w"
            case .cycleTheme: return "q"
            case .cycleBoard: return "z"
            }
        }
    }

    /// The key currently bound to `action` (a lowercased single character).
    static func shortcutKey(_ action: ShortcutAction) -> String {
        d.string(forKey: "Shortcut.\(action.rawValue)") ?? action.defaultKey
    }

    static func setShortcutKey(_ key: String, for action: ShortcutAction) {
        d.set(key.lowercased(), forKey: "Shortcut.\(action.rawValue)")
    }

    /// The action bound to `key`, if any — used by the overlay to dispatch a
    /// key press without hard-coding letters.
    static func action(forKey key: String) -> ShortcutAction? {
        let lower = key.lowercased()
        return ShortcutAction.allCases.first { shortcutKey($0) == lower }
    }

    /// Restores every shortcut to its built-in default.
    static func resetShortcuts() {
        for action in ShortcutAction.allCases {
            d.removeObject(forKey: "Shortcut.\(action.rawValue)")
        }
    }

    private enum Key {
        static let drawColorIndex = "Draw.colorIndex"
        static let drawPenWidth = "Draw.penWidth"
        static let textFontName = "Text.fontName"
        static let textFontSize = "Text.fontSize"
    }
}

extension NSColor {
    /// Parses "#RRGGBB" (or "RRGGBB") into an sRGB color, nil if malformed.
    convenience init?(hex: String) {
        var s = hex.trimmingCharacters(in: .whitespaces)
        if s.hasPrefix("#") { s.removeFirst() }
        guard s.count == 6, let value = UInt32(s, radix: 16) else { return nil }
        self.init(
            srgbRed: CGFloat((value >> 16) & 0xFF) / 255,
            green: CGFloat((value >> 8) & 0xFF) / 255,
            blue: CGFloat(value & 0xFF) / 255,
            alpha: 1
        )
    }

    /// "#RRGGBB" from this color's sRGB components (alpha dropped — palettes are
    /// opaque; the theme applies alpha at draw time).
    var hexString: String {
        let c = usingColorSpace(.sRGB) ?? self
        let r = Int((c.redComponent * 255).rounded())
        let g = Int((c.greenComponent * 255).rounded())
        let b = Int((c.blueComponent * 255).rounded())
        return String(format: "#%02X%02X%02X", r, g, b)
    }
}
