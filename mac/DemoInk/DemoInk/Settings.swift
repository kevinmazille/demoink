import Foundation
import CoreGraphics

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

    private enum Key {
        static let drawColorIndex = "Draw.colorIndex"
        static let drawPenWidth = "Draw.penWidth"
        static let textFontName = "Text.fontName"
        static let textFontSize = "Text.fontSize"
    }
}
