import AppKit
import ScreenCaptureKit

/// Auto-screenshot support, a faithful port of the Windows `SaveScreenshot` /
/// `GetMeetName` (MainWindow.cpp). On leaving draw mode, if anything was drawn,
/// the annotated screen is saved as a PNG under a two-tree layout:
///
///   By date/YYYY-MM-DD/[<client>/]HH-MM-SS.png   — always written
///   By client/<client>/YYYY-MM-DD/HH-MM-SS.png    — only when a Meet name found
///
/// The client name is parsed from the active Google Chrome window title, exactly
/// like the Windows build (`Meet - <name>` — on Windows suffixed ` - Google
/// Chrome`, on macOS usually just the tab title).
enum Screenshot {
    // MARK: - Settings (UserDefaults; the tabbed UI comes at Stage 8)

    /// Auto-capture is opt-out, like the Windows `Screenshot/enabled` default 1.
    static var isEnabled: Bool {
        get {
            let d = UserDefaults.standard
            return d.object(forKey: "Screenshot.enabled") == nil ? true : d.bool(forKey: "Screenshot.enabled")
        }
        set { UserDefaults.standard.set(newValue, forKey: "Screenshot.enabled") }
    }

    /// Meet detection is opt-out, like `Screenshot/meetdetect` default 1.
    static var isMeetDetectEnabled: Bool {
        get {
            let d = UserDefaults.standard
            return d.object(forKey: "Screenshot.meetdetect") == nil ? true : d.bool(forKey: "Screenshot.meetdetect")
        }
        set { UserDefaults.standard.set(newValue, forKey: "Screenshot.meetdetect") }
    }

    /// The user-configured root path, or nil when left at the default. Empty
    /// strings are treated as "unset" so clearing the field falls back cleanly.
    static var configuredFolderPath: String? {
        get {
            let s = UserDefaults.standard.string(forKey: "Screenshot.folder")
            return (s?.isEmpty ?? true) ? nil : s
        }
        set {
            let d = UserDefaults.standard
            if let newValue, !newValue.isEmpty {
                d.set(newValue, forKey: "Screenshot.folder")
            } else {
                d.removeObject(forKey: "Screenshot.folder")
            }
        }
    }

    /// Configured root, else `~/Pictures/DemoInk` (mirrors the Windows default of
    /// `%USERPROFILE%\Pictures\DemoInk`).
    static var rootFolder: URL? {
        if let configured = configuredFolderPath {
            return URL(fileURLWithPath: configured, isDirectory: true)
        }
        guard let pics = FileManager.default.urls(for: .picturesDirectory, in: .userDomainMask).first else {
            return nil
        }
        return pics.appendingPathComponent("DemoInk", isDirectory: true)
    }

    // MARK: - Save

    /// Writes `image` into the date tree (always) and the client tree (when a
    /// client name is present). Directories are created as needed. Returns the
    /// URLs actually written, for logging.
    @discardableResult
    static func save(_ image: NSImage, client: String?) -> [URL] {
        guard let root = rootFolder else { return [] }
        guard let png = pngData(from: image) else { return [] }

        let (ymd, hms) = timestamp()
        let fm = FileManager.default
        var written: [URL] = []

        // Tree 2 — by date (always). Client is an optional leaf folder; this is
        // the fallback when no Meet name was detected or detection is off.
        var byDate = root.appendingPathComponent("By date", isDirectory: true)
            .appendingPathComponent(ymd, isDirectory: true)
        if let client, !client.isEmpty {
            byDate.appendPathComponent(client, isDirectory: true)
        }
        if writePNG(png, dir: byDate, file: "\(hms).png", fm: fm) {
            written.append(byDate.appendingPathComponent("\(hms).png"))
        }

        // Tree 1 — by client (only when a Meet name was detected).
        if let client, !client.isEmpty {
            let byClient = root.appendingPathComponent("By client", isDirectory: true)
                .appendingPathComponent(client, isDirectory: true)
                .appendingPathComponent(ymd, isDirectory: true)
            if writePNG(png, dir: byClient, file: "\(hms).png", fm: fm) {
                written.append(byClient.appendingPathComponent("\(hms).png"))
            }
        }
        return written
    }

    private static func writePNG(_ data: Data, dir: URL, file: String, fm: FileManager) -> Bool {
        do {
            try fm.createDirectory(at: dir, withIntermediateDirectories: true)
            try data.write(to: dir.appendingPathComponent(file))
            return true
        } catch {
            NSLog("DemoInk: screenshot write failed at \(dir.path): \(error)")
            return false
        }
    }

    private static func pngData(from image: NSImage) -> Data? {
        guard let tiff = image.tiffRepresentation,
              let rep = NSBitmapImageRep(data: tiff) else { return nil }
        return rep.representation(using: .png, properties: [:])
    }

    private static func timestamp() -> (ymd: String, hms: String) {
        // POSIX locale so the numeric format is stable regardless of the user's
        // locale (matches the fixed `%04d-%02d-%02d` on Windows).
        let f = DateFormatter()
        f.locale = Locale(identifier: "en_US_POSIX")
        let now = Date()
        f.dateFormat = "yyyy-MM-dd"
        let ymd = f.string(from: now)
        f.dateFormat = "HH-mm-ss"
        let hms = f.string(from: now)
        return (ymd, hms)
    }

    // MARK: - Fast freeze grab (synchronous, for draw-mode entry)

    /// A fast, synchronous grab of the whole display, used only for the freeze at
    /// draw-mode entry. Unlike `captureDesktop` (async ScreenCaptureKit, which
    /// enumerates every on-screen window before it can grab — tens of ms), this
    /// captures the display in one call on the calling thread, so it runs at the
    /// very first instant of the hotkey handler.
    ///
    /// That latency is the whole game for keeping a pointer-attached popup or
    /// tooltip on screen: the longer we wait between the hotkey and the grab, the
    /// more likely the front app dismisses it (some close it on the very first key
    /// or focus twitch). Grabbing synchronously and immediately catches it in far
    /// more apps than the async path did.
    ///
    /// `CGDisplayCreateImage` is deprecated on macOS 15 but still functions on our
    /// 13+ target; there is no synchronous ScreenCaptureKit equivalent. Needs
    /// Screen Recording (silent preflight, never prompts); returns nil if not
    /// granted, so the caller falls back to the live desktop.
    static func freezeDisplay(screen: NSScreen) -> CGImage? {
        guard let displayID = screen.displayID else { return nil }
        guard CGPreflightScreenCaptureAccess() else { return nil }
        return CGDisplayCreateImage(displayID)
    }

    // MARK: - Desktop capture (Screen Recording, ScreenCaptureKit)

    /// Captures the desktop for `screen`, excluding our own overlay window so its
    /// clear surface and annotations aren't double-counted. Uses ScreenCaptureKit
    /// (`CGWindowListCreateImage` is removed from recent SDKs). Returns nil if
    /// Screen Recording isn't granted yet or capture fails; the caller then falls
    /// back to saving the annotations over an empty background.
    ///
    /// This never prompts for permission. Requesting the grant is the sole job of
    /// the Permissions panel (⌘⇧P). We gate on `CGPreflightScreenCaptureAccess()`,
    /// which is silent (unlike `SCShareableContent`, which itself pops the TCC
    /// prompt when unauthorized); if it reports no access we bail out and let the
    /// caller save the annotations over an empty background rather than show a
    /// prompt on every draw exit. This does mean a freshly rebuilt ad-hoc binary
    /// (new hash → TCC sees a different app) captures nothing until re-granted +
    /// relaunched via ⌘⇧P — acceptable, and far better than a prompt every time.
    ///
    /// `screen` identifies which physical display to grab, so a capture triggered
    /// on a secondary monitor doesn't silently record the main one.
    ///
    /// `excludingWindow` drops our own overlay from the captured stack; pass nil
    /// when there is nothing to exclude — e.g. the freeze taken at draw-mode entry
    /// happens *before* the overlay exists, so the whole desktop (including any
    /// transient popup still attached to the pointer) is captured verbatim.
    static func captureDesktop(screen: NSScreen, excludingWindow windowNumber: Int? = nil) async -> CGImage? {
        guard let displayID = screen.displayID else { return nil }

        // Silent gate: preflight never prompts. Bail before touching
        // SCShareableContent, which would pop the TCC dialog when unauthorized.
        guard CGPreflightScreenCaptureAccess() else {
            NSLog("DemoInk: Screen Recording not granted — skipping desktop capture. Grant via ⌘⇧P and relaunch.")
            return nil
        }

        do {
            // excludingDesktopWindows:false keeps the wallpaper; onScreenWindowsOnly
            // gives us the visible stack. We then drop our overlay from it.
            let content = try await SCShareableContent.excludingDesktopWindows(false, onScreenWindowsOnly: true)
            guard let display = content.displays.first(where: { $0.displayID == displayID })
                ?? content.displays.first else { return nil }

            let excluded = windowNumber
                .map { n in content.windows.filter { $0.windowID == CGWindowID(n) } } ?? []
            let filter = SCContentFilter(display: display, excludingWindows: excluded)

            // Capture at native resolution (points × backing scale).
            let scale = screen.backingScaleFactor
            let config = SCStreamConfiguration()
            config.width = Int(CGFloat(display.width) * scale)
            config.height = Int(CGFloat(display.height) * scale)
            config.showsCursor = false
            config.capturesAudio = false

            return try await SCScreenshotManager.captureImage(contentFilter: filter, configuration: config)
        } catch {
            NSLog("DemoInk: desktop capture failed: \(error)")
            return nil
        }
    }

    // MARK: - Meet client name

    /// The meeting name from an on-screen Google Chrome window titled
    /// `Meet - <name>` (optionally suffixed ` - Google Chrome`). Reading window
    /// titles needs Screen Recording too; without it this returns nil and the
    /// capture is filed by date only. Mirrors `FindMeetProc` in MainWindow.cpp.
    static func meetClientName() -> String? {
        guard isMeetDetectEnabled else { return nil }
        let opts: CGWindowListOption = [.optionOnScreenOnly, .excludeDesktopElements]
        guard let list = CGWindowListCopyWindowInfo(opts, kCGNullWindowID) as? [[String: Any]] else {
            return nil
        }
        for w in list {
            guard let owner = w[kCGWindowOwnerName as String] as? String,
                  owner == "Google Chrome",
                  let title = w[kCGWindowName as String] as? String else { continue }
            if let name = parseMeetTitle(title) { return name }
        }
        return nil
    }

    private static func parseMeetTitle(_ title: String) -> String? {
        let prefix = "Meet - "
        guard title.hasPrefix(prefix) else { return nil }
        var name = String(title.dropFirst(prefix.count))
        let suffix = " - Google Chrome"
        if name.hasSuffix(suffix) { name = String(name.dropLast(suffix.count)) }
        let clean = sanitize(name)
        return clean.isEmpty ? nil : clean
    }

    /// Strips characters illegal in a path component (`/` and `:` on macOS, plus
    /// control chars) and trims leading/trailing spaces and dots. Mirrors the
    /// Windows `SanitizeForPath`.
    static func sanitize(_ s: String) -> String {
        let forbidden = Set("/:\\?%*|\"<>")
        let mapped = String(s.map { ch -> Character in
            (forbidden.contains(ch) || ch.asciiValue.map { $0 < 0x20 } == true) ? "_" : ch
        })
        return mapped.trimmingCharacters(in: CharacterSet(charactersIn: " ."))
    }
}

extension NSScreen {
    /// The CoreGraphics display ID for this screen, read from its device
    /// description — needed to target the right monitor in ScreenCaptureKit.
    var displayID: CGDirectDisplayID? {
        (deviceDescription[NSDeviceDescriptionKey("NSScreenNumber")] as? NSNumber)?.uint32Value
    }
}
