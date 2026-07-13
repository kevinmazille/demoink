import AppKit

/// Full-screen transparent overlay. Borderless and click-through-capable, it
/// hosts the drawing surface. Overrides `canBecomeKey` so the view can receive
/// keyDown for the draw-mode shortcuts (undo, colors, width, erase, Esc).
final class OverlayWindow: NSWindow {
    convenience init(screen: NSScreen) {
        // Create with a zero-based rect, then place the window explicitly with
        // `setFrame` in global coordinates. Passing `screen:` to the initializer
        // reinterprets `contentRect` relative to that screen, which double-counts
        // a secondary display's offset and lands the window off-screen — the
        // reason the overlay only ever appeared on the main display.
        self.init(
            contentRect: NSRect(origin: .zero, size: screen.frame.size),
            styleMask: [.borderless, .fullSizeContentView],
            backing: .buffered,
            defer: false
        )
        setFrame(screen.frame, display: true)

        backgroundColor = .clear
        isOpaque = false
        hasShadow = false
        // We keep a strong Swift reference and set it to nil on close; leaving
        // the AppKit default (release-on-close) would double-free the window
        // and crash in objc_release during the runloop's pool pop.
        isReleasedWhenClosed = false
        level = .screenSaver
        ignoresMouseEvents = false
        collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary, .stationary]
        // The content view frame is expressed in the *window's* coordinate space,
        // which always starts at zero — NOT in global screen coordinates. Using
        // `screen.frame` here happens to work on the main display (origin ≈ zero)
        // but on a secondary display the non-zero origin shoves the drawing
        // surface off the window, so nothing renders and mouse tracking misses.
        // Size to the screen but anchor at the origin.
        contentView = OverlayView(frame: NSRect(origin: .zero, size: screen.frame.size))
    }

    // Borderless windows refuse key status by default; drawing shortcuts need it.
    override var canBecomeKey: Bool { true }
    override var canBecomeMain: Bool { true }
}

/// The drawing surface. Holds the annotation list and renders it immediate-mode
/// in `draw(_:)`, faithful to the Windows `RenderAnnotations`. Freehand strokes
/// are smoothed with a cardinal spline (tension 0.5) to match GDI+ `DrawCurve`.
final class OverlayView: NSView {
    /// Called when the user presses Esc — the delegate closes the overlay.
    var onExit: (() -> Void)?

    private var lines: [DrawLine] = []
    private var isDrawing = false

    // Current tool state (frozen into each DrawLine at stroke start). Seeded from
    // the user's saved launch defaults (Settings), falling back to DrawModel.
    private var colorIndex = Settings.defaultColorIndex
    private var penWidth = Settings.defaultPenWidth
    private var theme: Theme = .transparent
    private var boardStyle: BoardStyle = .none
    private var alpha = DrawModel.lineAlpha

    // Text mode: while active the last line is a .text line that follows the
    // mouse until a click commits it; keystrokes edit it and a caret blinks.
    private var isTextMode = false
    private var caretVisible = true
    private var caretTimer: Timer?

    override var isFlipped: Bool { true } // top-left origin, like Windows

    override var acceptsFirstResponder: Bool { true }

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        if window != nil {
            window?.makeFirstResponder(self)
        } else {
            // Overlay closing: stop the caret timer and, if the cursor was
            // hidden, restore it so the system arrow doesn't stay hidden.
            caretTimer?.invalidate()
            caretTimer = nil
            isTextMode = false
            if cursorPoint != nil {
                NSCursor.unhide()
                cursorPoint = nil
            }
        }
    }

    // MARK: - Cursor indicator

    /// Live position of the pointer inside the overlay; the colour ball is
    /// painted here every frame. nil while the pointer is outside the view.
    private var cursorPoint: CGPoint?

    /// A tracking area spanning the whole view so we hide the system cursor and
    /// follow the mouse (moved *and* dragged) across the entire overlay.
    override func updateTrackingAreas() {
        super.updateTrackingAreas()
        trackingAreas.forEach(removeTrackingArea)
        addTrackingArea(NSTrackingArea(
            rect: bounds,
            options: [.activeAlways, .mouseMoved, .mouseEnteredAndExited, .inVisibleRect],
            owner: self,
            userInfo: nil
        ))
    }

    override func mouseEntered(with event: NSEvent) {
        NSCursor.hide() // only the colour ball should be visible, no system arrow
        cursorPoint = convert(event.locationInWindow, from: nil)
        needsDisplay = true
    }

    override func mouseExited(with event: NSEvent) {
        NSCursor.unhide()
        cursorPoint = nil
        needsDisplay = true
    }

    override func mouseMoved(with event: NSEvent) {
        let p = convert(event.locationInWindow, from: nil)
        cursorPoint = p
        // Before it's committed, the text line tracks the pointer.
        if isTextMode, let idx = lines.indices.last, lines[idx].lineType == .text {
            lines[idx].lineStart = p
        }
        needsDisplay = true
    }

    /// Nudge a redraw of the ball after a colour/width/theme change.
    private func refreshCursor() {
        needsDisplay = true
    }

    /// Paints the pen-tip indicator: a filled disc of the current ink colour,
    /// sized to the pen width — faithful to the Windows draw cursor, but drawn
    /// in-view so no system cursor ever shows.
    private func drawCursorIndicator() {
        // In text mode the caret is the indicator; the colour ball would just
        // overlap it, so suppress it.
        guard !isTextMode, let p = cursorPoint else { return }
        let dia = max(min(penWidth, 24), 6) // clamp so the disc stays usable
        let discRect = NSRect(x: p.x - dia / 2, y: p.y - dia / 2, width: dia, height: dia)

        let color = DrawModel.color(atIndex: colorIndex, alpha: alpha, theme: theme)
        color.setFill()
        NSBezierPath(ovalIn: discRect).fill()
        // A hairline outline keeps a light-on-light disc visible.
        NSColor(white: 0, alpha: 0.35).setStroke()
        let ring = NSBezierPath(ovalIn: discRect)
        ring.lineWidth = 1
        ring.stroke()
    }

    // MARK: - Board frame

    /// Decorative "board" behind the annotations, a faithful port of the Windows
    /// `PaintBoardFrame`. Geometry is authored in a 1920×1080 design space and
    /// scaled to the live view; the flipped (top-left origin) coordinate system
    /// matches the original directly. Frame A = light whiteboard, B = dark slate.
    private func drawBoardFrame() {
        let cx = bounds.width, cy = bounds.height
        guard cx > 0, cy > 0 else { return }

        let sx = cx / 1920.0, sy = cy / 1080.0
        let s = sy // uniform scale for stroke widths / tick sizes
        func X(_ v: CGFloat) -> CGFloat { v * sx }
        func Y(_ v: CGFloat) -> CGFloat { v * sy }
        func W(_ v: CGFloat) -> CGFloat { max(1, v * s) }

        let clay = NSColor(srgbRed: 0xD9 / 255, green: 0x77 / 255, blue: 0x57 / 255, alpha: 1)

        if boardStyle == .frameA {
            // Light whiteboard: paper gradient, grey mat border, thin clay
            // liseré framing the ~95% canvas, plus corner registration ticks.
            let full = NSRect(x: 0, y: 0, width: cx, height: cy)
            let paper = NSGradient(starting: NSColor(srgbRed: 0xFD / 255, green: 0xFD / 255, blue: 0xFC / 255, alpha: 1),
                                   ending: NSColor(srgbRed: 0xF4 / 255, green: 0xF2 / 255, blue: 0xEE / 255, alpha: 1))
            paper?.draw(in: full, angle: -90) // vertical, top→bottom in flipped space

            // Grey mat: a thick border stroke around the outer edge.
            let mat = NSColor(srgbRed: 0xE7 / 255, green: 0xE3 / 255, blue: 0xDC / 255, alpha: 1)
            mat.setStroke()
            let matRect = NSRect(x: X(14), y: Y(14), width: X(1920) - X(28), height: Y(1080) - Y(28))
            let matPath = NSBezierPath(rect: matRect)
            matPath.lineWidth = W(28)
            matPath.stroke()

            // Thin clay liseré, rounded, framing the drawing zone.
            clay.setStroke()
            let liseré = NSBezierPath(roundedRect: NSRect(x: X(34), y: Y(34), width: X(1852), height: Y(1012)),
                                      xRadius: W(8), yRadius: W(8))
            liseré.lineWidth = W(3)
            liseré.stroke()

            // Corner registration ticks.
            clay.setStroke()
            func tick(_ x1: CGFloat, _ y1: CGFloat, _ x2: CGFloat, _ y2: CGFloat) {
                let p = NSBezierPath()
                p.move(to: CGPoint(x: X(x1), y: Y(y1)))
                p.line(to: CGPoint(x: X(x2), y: Y(y2)))
                p.lineWidth = W(4)
                p.lineCapStyle = .round
                p.stroke()
            }
            tick(60, 34, 60, 70);      tick(34, 60, 70, 60)       // top-left
            tick(1860, 34, 1860, 70);  tick(1886, 60, 1850, 60)   // top-right
            tick(60, 1046, 60, 1010);  tick(34, 1020, 70, 1020)   // bottom-left
            tick(1860, 1046, 1860, 1010); tick(1886, 1020, 1850, 1020) // bottom-right
        } else if boardStyle == .frameB {
            // Dark slate board: bevelled frame, radial slate canvas, clay tray.
            let full = NSRect(x: 0, y: 0, width: cx, height: cy)
            let frame = NSGradient(starting: NSColor(srgbRed: 0x3A / 255, green: 0x3D / 255, blue: 0x42 / 255, alpha: 1),
                                   ending: NSColor(srgbRed: 0x2C / 255, green: 0x2E / 255, blue: 0x33 / 255, alpha: 1))
            frame?.draw(in: full, angle: -90)

            // Inner bevel highlight.
            NSColor(srgbRed: 0x4A / 255, green: 0x4D / 255, blue: 0x53 / 255, alpha: 1).setStroke()
            let bevel = NSBezierPath(rect: NSRect(x: X(22), y: Y(22), width: X(1920) - X(44), height: Y(1080) - Y(44)))
            bevel.lineWidth = W(2)
            bevel.stroke()

            // Slate canvas (~95%), radial gradient centre-biased upward.
            let canvas = NSRect(x: X(40), y: Y(40), width: X(1840), height: Y(1000))
            let slate = NSGradient(starting: NSColor(srgbRed: 0x2A / 255, green: 0x2D / 255, blue: 0x31 / 255, alpha: 1),
                                   ending: NSColor(srgbRed: 0x20 / 255, green: 0x22 / 255, blue: 0x25 / 255, alpha: 1))
            let canvasPath = NSBezierPath(rect: canvas)
            slate?.draw(in: canvasPath, relativeCenterPosition: NSPoint(x: 0, y: -0.16))

            // Clay tray baseline at the bottom of the board.
            NSColor(srgbRed: 0xD9 / 255, green: 0x77 / 255, blue: 0x57 / 255, alpha: 0xD9 / 255).setFill()
            NSRect(x: X(40), y: Y(1028), width: X(1840), height: Y(12)).fill()
        }
    }

    // MARK: - Mouse

    // A stroke begins on either button; the button (left/right) plus the held
    // modifiers decide the LineType live during the drag, per the Windows model.
    private func beginStroke(at p: CGPoint) {
        cursorPoint = p
        var line = DrawLine()
        line.colorIndex = colorIndex
        line.penWidth = penWidth
        line.alpha = alpha
        line.points = [p]
        line.lineStart = p
        lines.append(line)
        isDrawing = true
        needsDisplay = true
    }

    override func mouseDown(with event: NSEvent) {
        // A left click commits the text and leaves text mode (Windows behaviour:
        // the text stops following the mouse and is dropped in place).
        if isTextMode {
            exitTextMode(commit: true)
            return
        }
        beginStroke(at: convert(event.locationInWindow, from: nil))
    }

    override func rightMouseDown(with event: NSEvent) {
        guard !isTextMode else { return }
        beginStroke(at: convert(event.locationInWindow, from: nil))
    }

    // Left-button drag: Shift+Ctrl→ellipse, Shift→rectangle, Ctrl→straight,
    // else freehand. Mirrors WM_MOUSEMOVE / MK_LBUTTON in MainWindow.cpp.
    override func mouseDragged(with event: NSEvent) {
        guard isDrawing, !lines.isEmpty else { return }
        let p = convert(event.locationInWindow, from: nil)
        cursorPoint = p
        let flags = event.modifierFlags
        let idx = lines.count - 1

        if flags.contains(.shift) || flags.contains(.control) {
            if flags.contains(.shift) {
                lines[idx].lineType = flags.contains(.control) ? .ellipse : .rectangle
            } else {
                lines[idx].lineType = .straight
            }
            lines[idx].lineEnd = p
            needsDisplay = true
            return
        }

        // Freehand: skip sub-pixel jitter, matching the Windows >1px gate.
        lines[idx].lineType = .hand
        if let last = lines[idx].points.last,
           hypot(p.x - last.x, p.y - last.y) <= 1 {
            return
        }
        lines[idx].points.append(p)
        needsDisplay = true
    }

    // Right-button drag: arrow by default; Ctrl→straight; Shift constrains to
    // the dominant axis. Mirrors WM_MOUSEMOVE / MK_RBUTTON in MainWindow.cpp.
    override func rightMouseDragged(with event: NSEvent) {
        guard isDrawing, !lines.isEmpty else { return }
        var p = convert(event.locationInWindow, from: nil)
        cursorPoint = p
        let flags = event.modifierFlags
        let idx = lines.count - 1
        let start = lines[idx].lineStart

        if flags.contains(.shift) {
            if abs(p.x - start.x) > abs(p.y - start.y) {
                p.y = start.y // horizontal
            } else {
                p.x = start.x // vertical
            }
        }
        lines[idx].lineType = flags.contains(.control) ? .straight : .arrow
        lines[idx].lineEnd = p
        needsDisplay = true
    }

    override func mouseUp(with event: NSEvent) {
        isDrawing = false
    }

    override func rightMouseUp(with event: NSEvent) {
        isDrawing = false
    }

    // MARK: - Scroll wheel = pen width (matches Windows wheel-resizes)

    override func scrollWheel(with event: NSEvent) {
        // In text mode the wheel resizes the font (±4, clamped); otherwise it
        // resizes the pen — matching the Windows wheel-resizes behaviour.
        if isTextMode, let idx = lines.indices.last, lines[idx].lineType == .text {
            if event.deltaY > 0 {
                lines[idx].fontSize = min(lines[idx].fontSize + DrawModel.fontStep, DrawModel.maxFontSize)
            } else if event.deltaY < 0 {
                lines[idx].fontSize = max(lines[idx].fontSize - DrawModel.fontStep, DrawModel.minFontSize)
            }
            needsDisplay = true
            return
        }
        if event.deltaY > 0 {
            penWidth = min(penWidth + 1, DrawModel.maxPenWidth)
        } else if event.deltaY < 0 {
            penWidth = max(penWidth - 1, DrawModel.minPenWidth)
        }
        refreshCursor()
    }

    // MARK: - Keyboard

    override func keyDown(with event: NSEvent) {
        // In text mode every key edits the text: shortcuts are suppressed, just
        // like Windows skips TranslateAccelerator while typing (DemoHelper.cpp).
        if isTextMode {
            handleTextKey(event)
            return
        }

        switch event.keyCode {
        case 53: // Esc — exit draw mode
            onExit?()
            return
        case 51, 117: // Delete / Forward-delete — undo last stroke
            if !lines.isEmpty { lines.removeLast() }
            needsDisplay = true
            return
        case 126: // Up arrow — thicker
            penWidth = min(penWidth + 1, DrawModel.maxPenWidth)
            refreshCursor()
            return
        case 125: // Down arrow — thinner
            penWidth = max(penWidth - 1, DrawModel.minPenWidth)
            refreshCursor()
            return
        case 123: // Left arrow — previous color
            colorIndex = (colorIndex + 9) % 10
            refreshCursor()
            return
        case 124: // Right arrow — next color
            colorIndex = (colorIndex + 1) % 10
            refreshCursor()
            return
        default:
            break
        }

        guard let chars = event.charactersIgnoringModifiers else { return }
        if let digit = Int(chars), digit >= 0, digit <= 9 {
            colorIndex = digit
            refreshCursor()
            return
        }
        // Letter shortcuts are user-rebindable (Settings › Shortcuts); dispatch
        // by looking up the action bound to the pressed key rather than hard-
        // coding letters. The defaults are A/W/Q/Z.
        switch Settings.action(forKey: chars) {
        case .eraseAll:
            lines.removeAll()
            needsDisplay = true
        case .cycleTheme:
            toggleTheme()
        case .cycleBoard:
            cycleBoard()
        case .text:
            enterTextMode()
        case nil:
            super.keyDown(with: event)
        }
    }

    // MARK: - Text mode

    /// A key. Starts a new .text line at the pointer, following the mouse until
    /// a click commits it. Alpha follows the theme like strokes; a caret blinks
    /// at 500 ms. Mirrors ID_CMD_TEXTMODE in Commands.cpp.
    private func enterTextMode() {
        guard !isTextMode else { return }
        var line = DrawLine()
        line.lineType = .text
        line.colorIndex = colorIndex
        line.penWidth = penWidth
        line.alpha = alpha
        line.fontSize = Settings.defaultFontSize
        line.fontName = Settings.defaultFontName
        line.lineStart = cursorPoint ?? CGPoint(x: bounds.midX, y: bounds.midY)
        lines.append(line)

        isTextMode = true
        isDrawing = false
        caretVisible = true
        caretTimer = Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { [weak self] _ in
            guard let self, self.isTextMode else { return }
            self.caretVisible.toggle()
            self.needsDisplay = true
        }
        needsDisplay = true
    }

    /// Edits the active text line. Backspace deletes; Esc cancels (drops the
    /// line, stays in the overlay); Return is ignored; printable characters
    /// append. Mirrors the WM_CHAR handler in MainWindow.cpp.
    private func handleTextKey(_ event: NSEvent) {
        guard isTextMode, let idx = lines.indices.last, lines[idx].lineType == .text else {
            exitTextMode(commit: false)
            return
        }
        switch event.keyCode {
        case 53: // Esc — cancel this text line
            exitTextMode(commit: false)
            return
        case 51, 117: // Backspace / Forward-delete
            if !lines[idx].text.isEmpty { lines[idx].text.removeLast() }
        case 36, 76: // Return / Enter — commit the text
            exitTextMode(commit: true)
            return
        default:
            if let s = event.characters, !s.isEmpty,
               s.unicodeScalars.allSatisfy({ $0.value >= 0x20 }) {
                lines[idx].text.append(s)
            }
        }
        needsDisplay = true
    }

    /// Leaves text mode. On commit an empty text line is discarded; on cancel
    /// the line is always dropped. Kills the caret timer either way.
    private func exitTextMode(commit: Bool) {
        caretTimer?.invalidate()
        caretTimer = nil
        isTextMode = false
        if let idx = lines.indices.last, lines[idx].lineType == .text {
            if !commit || lines[idx].text.isEmpty {
                lines.remove(at: idx)
            }
        }
        needsDisplay = true
    }

    // MARK: - Theme & board

    /// Q key. First press from the pristine Transparent state wipes annotations
    /// and starts fresh on the Light canvas; afterwards Q toggles Light ↔ Dark
    /// and preserves the drawings, re-alpha'ing them to the new theme. Q also
    /// clears any board frame. Mirrors ID_CMD_TOGGLETHEME in Commands.cpp.
    private func toggleTheme() {
        if theme == .transparent {
            theme = .light
            isDrawing = false
            lines.removeAll()
        } else {
            theme = (theme == .light) ? .dark : .light
        }
        boardStyle = .none
        reapplyThemeAlpha()
    }

    /// Z key. Cycles the board frame None→A→B→A. FrameA couples to Light,
    /// FrameB to Dark. Wipe rule shared with Q: leaving pristine Transparent
    /// clears the drawings; any later switch preserves them and re-alphas to the
    /// new theme. Mirrors ID_CMD_CYCLEBOARD in Commands.cpp.
    private func cycleBoard() {
        let leavingTransparent = (theme == .transparent)
        if boardStyle == .none {
            boardStyle = .frameA
        } else {
            boardStyle = (boardStyle == .frameA) ? .frameB : .frameA
        }
        theme = (boardStyle == .frameA) ? .light : .dark
        if leavingTransparent {
            isDrawing = false
            lines.removeAll()
        }
        reapplyThemeAlpha()
    }

    /// Match the ink alpha to the current theme (opaque on Dark) and refresh.
    private func reapplyThemeAlpha() {
        alpha = DrawModel.alpha(for: theme)
        for i in lines.indices {
            lines[i].alpha = alpha
        }
        refreshCursor()
        needsDisplay = true
    }

    // MARK: - Session defaults

    /// Writes the tool state from this session back as the launch defaults, so
    /// the next session (and the next launch) resumes where the user left off:
    /// last color, last pen width, and — if any text was placed — the last font
    /// size/name. Called on exit from draw mode.
    ///
    /// This intentionally diverges from the Windows build, where live changes
    /// weren't persisted ("épaisseur non persistée d'une session à l'autre"); the
    /// Mac version remembers them, which is what feels natural here.
    func persistSessionDefaults() {
        Settings.defaultColorIndex = colorIndex
        Settings.defaultPenWidth = penWidth
        if let lastText = lines.last(where: { $0.lineType == .text }) {
            Settings.defaultFontSize = lastText.fontSize
            Settings.defaultFontName = lastText.fontName
        }
    }

    // MARK: - Auto-screenshot

    /// Saves the annotated screen on exit, faithful to the Windows
    /// `SaveScreenshot`: no-op when nothing was drawn or auto-capture is off.
    /// Runs the (async) desktop capture while the window is still on-screen, then
    /// calls `completion` so the caller can close the overlay. `completion` always
    /// fires — on the main actor — even on the no-op / failure paths.
    func saveScreenshotIfNeeded(completion: @escaping () -> Void) {
        guard !lines.isEmpty, Screenshot.isEnabled else {
            completion()
            return
        }

        // Snapshot the client name and screen/window now, on the main thread,
        // before anything can tear down.
        let client = Screenshot.meetClientName()
        let screen = window?.screen
        let windowNumber = window?.windowNumber
        let needsDesktop = (theme == .transparent)

        Task { @MainActor in
            // Transparent theme shows the real desktop through the clear overlay,
            // so capture what's below our window. Light/Dark paint their own
            // opaque background, so no desktop capture is needed.
            var desktop: CGImage?
            if needsDesktop, let screen, let windowNumber {
                desktop = await Screenshot.captureDesktop(screen: screen, excludingWindow: windowNumber)
            }
            if let image = composedScreenshot(desktopBelow: desktop) {
                Screenshot.save(image, client: client)
            }
            completion()
        }
    }

    /// Flattens the overlay to a native-resolution image: the desktop (or the
    /// theme's solid fill + board) underneath, every annotation on top, and never
    /// the cursor ball or caret.
    private func composedScreenshot(desktopBelow: CGImage?) -> NSImage? {
        let ptSize = bounds.size
        guard ptSize.width > 0, ptSize.height > 0 else { return nil }

        // Render background/board/annotations at the backing scale. Nil out the
        // pointer so the colour ball never lands in the capture; text mode is
        // already off by the time we exit, so the caret won't draw either.
        cursorPoint = nil
        guard let viewRep = bitmapImageRepForCachingDisplay(in: bounds) else { return nil }
        cacheDisplay(in: bounds, to: viewRep)

        let pxW = viewRep.pixelsWide, pxH = viewRep.pixelsHigh
        guard pxW > 0, pxH > 0,
              let space = CGColorSpace(name: CGColorSpace.sRGB),
              let cg = CGContext(data: nil, width: pxW, height: pxH,
                                 bitsPerComponent: 8, bytesPerRow: 0, space: space,
                                 bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else {
            return nil
        }
        let full = CGRect(x: 0, y: 0, width: pxW, height: pxH)
        if let desktopBelow { cg.draw(desktopBelow, in: full) }
        if let viewCG = viewRep.cgImage { cg.draw(viewCG, in: full) }
        guard let out = cg.makeImage() else { return nil }
        return NSImage(cgImage: out, size: ptSize)
    }

    // MARK: - Rendering

    override func draw(_ dirtyRect: NSRect) {
        super.draw(dirtyRect)
        guard let ctx = NSGraphicsContext.current else { return }
        ctx.imageInterpolation = .high

        // Theme background: Transparent shows the desktop (clear window);
        // Light/Dark paint a solid fill behind the annotations.
        switch theme {
        case .transparent:
            break
        case .light:
            DrawModel.backgroundLight.setFill()
            bounds.fill()
        case .dark:
            DrawModel.backgroundDark.setFill()
            bounds.fill()
        }

        // Board frame sits above the solid fill, below the annotations.
        if boardStyle != .none {
            drawBoardFrame()
        }

        for line in lines {
            let color = DrawModel.color(atIndex: line.colorIndex, alpha: line.alpha, theme: theme)
            color.setStroke()
            color.setFill()

            switch line.lineType {
            case .hand:
                if line.points.count == 1 {
                    // Single click: a filled dot of pen-width diameter, matching
                    // the Windows FillEllipse case.
                    let r = line.penWidth / 2
                    let p = line.points[0]
                    let dot = NSBezierPath(ovalIn: NSRect(x: p.x - r, y: p.y - r,
                                                          width: line.penWidth, height: line.penWidth))
                    dot.fill()
                } else {
                    let path = OverlayView.cardinalSpline(through: line.points, tension: 0.5)
                    strokeShape(path, width: line.penWidth)
                }
            case .straight:
                guard let end = line.lineEnd else { break }
                let path = NSBezierPath()
                path.move(to: line.lineStart)
                path.line(to: end)
                strokeShape(path, width: line.penWidth)
            case .arrow:
                guard let end = line.lineEnd else { break }
                drawArrow(from: line.lineStart, to: end, penWidth: line.penWidth)
            case .rectangle:
                guard let end = line.lineEnd else { break }
                strokeShape(NSBezierPath(rect: rect(from: line.lineStart, to: end)), width: line.penWidth)
            case .ellipse:
                guard let end = line.lineEnd else { break }
                strokeShape(NSBezierPath(ovalIn: rect(from: line.lineStart, to: end)), width: line.penWidth)
            case .text:
                drawText(line, color: color)
            }
        }

        if isTextMode, caretVisible {
            drawTextCaret()
        }

        drawCursorIndicator()
    }

    /// Draws a committed or in-progress text line. Origin = top-left of the em
    /// box at `lineStart`, faithful to the Windows `DrawString` placement.
    private func drawText(_ line: DrawLine, color: NSColor) {
        guard !line.text.isEmpty, line.lineStart.x >= 0, line.lineStart.y >= 0 else { return }
        let font = DrawModel.resolveTextFont(line.fontName, size: line.fontSize)
        let attrs: [NSAttributedString.Key: Any] = [.font: font, .foregroundColor: color]
        (line.text as NSString).draw(at: line.lineStart, withAttributes: attrs)
    }

    /// Blinking caret shown only while typing, anchored on the text baseline so
    /// the text sits at the bottom of the caret (matches RenderTextCaret). Not
    /// part of the annotation list, so it never lands in a screenshot.
    private func drawTextCaret() {
        guard let idx = lines.indices.last, lines[idx].lineType == .text else { return }
        let line = lines[idx]
        let font = DrawModel.resolveTextFont(line.fontName, size: line.fontSize)

        // Caret X = origin + width of the text typed so far.
        var caretX = line.lineStart.x
        if !line.text.isEmpty {
            let w = (line.text as NSString).size(withAttributes: [.font: font]).width
            caretX += w
        }
        // Height = ascent; bottom anchored on the baseline (origin.y + ascent).
        let caretHeight = font.ascender
        let caretY = line.lineStart.y // top of em box; caret spans down to baseline
        let color = DrawModel.color(atIndex: line.colorIndex, alpha: DrawModel.opaqueAlpha, theme: theme)
        color.setStroke()
        let caret = NSBezierPath()
        caret.move(to: CGPoint(x: caretX, y: caretY))
        caret.line(to: CGPoint(x: caretX, y: caretY + caretHeight))
        caret.lineWidth = max(2, line.fontSize / 16)
        caret.stroke()
    }

    private func strokeShape(_ path: NSBezierPath, width: CGFloat) {
        path.lineWidth = width
        path.lineCapStyle = .round
        path.lineJoinStyle = .round
        path.stroke()
    }

    private func rect(from a: CGPoint, to b: CGPoint) -> NSRect {
        NSRect(x: min(a.x, b.x), y: min(a.y, b.y),
               width: abs(b.x - a.x), height: abs(b.y - a.y))
    }

    /// Draws an arrow as a *single* filled polygon that outlines the whole
    /// shape — shaft rectangle + arrowhead in one closed path, filled once.
    /// GDI+ had LineCapArrowAnchor; NSBezierPath has no arrow cap, and stroking
    /// a shaft then filling a separate head made the two overlap. Under
    /// semi-transparent ink that overlap composited darker and looked ragged.
    /// One fill = uniform alpha everywhere, no seam.
    private func drawArrow(from start: CGPoint, to end: CGPoint, penWidth: CGFloat) {
        let dx = end.x - start.x, dy = end.y - start.y
        let len = hypot(dx, dy)
        guard len > 0.5 else { return }

        let dir = CGPoint(x: dx / len, y: dy / len)
        let perp = CGPoint(x: -dir.y, y: dir.x)

        let headLen = min(max(penWidth * 3, 10), len)   // never longer than the arrow
        let shaftHalf = max(penWidth, 1.5) / 2           // half the shaft thickness
        let barbHalf = headLen * 0.35                    // arrowhead half-spread

        // Base of the head along the centreline; shaft runs from start to here.
        let base = CGPoint(x: end.x - dir.x * headLen, y: end.y - dir.y * headLen)

        func offset(_ p: CGPoint, _ v: CGPoint, _ d: CGFloat) -> CGPoint {
            CGPoint(x: p.x + v.x * d, y: p.y + v.y * d)
        }

        let path = NSBezierPath()
        path.move(to: offset(start, perp, shaftHalf))      // shaft, one side
        path.line(to: offset(base, perp, shaftHalf))
        path.line(to: offset(base, perp, barbHalf))        // barb 1
        path.line(to: end)                                 // tip
        path.line(to: offset(base, perp, -barbHalf))       // barb 2
        path.line(to: offset(base, perp, -shaftHalf))
        path.line(to: offset(start, perp, -shaftHalf))     // shaft, other side
        path.close()
        path.fill()
    }

    /// Cardinal spline through the given points, converted to cubic Béziers.
    /// Mirrors GDI+ `DrawCurve(points, tension: 0.5)`: control points are
    /// derived from neighbouring points scaled by tension/3.
    static func cardinalSpline(through pts: [CGPoint], tension: CGFloat) -> NSBezierPath {
        let path = NSBezierPath()
        guard pts.count >= 2 else { return path }
        path.move(to: pts[0])

        let t = tension / 3.0
        for i in 0..<(pts.count - 1) {
            let p0 = pts[max(i - 1, 0)]
            let p1 = pts[i]
            let p2 = pts[i + 1]
            let p3 = pts[min(i + 2, pts.count - 1)]

            let cp1 = CGPoint(x: p1.x + (p2.x - p0.x) * t,
                              y: p1.y + (p2.y - p0.y) * t)
            let cp2 = CGPoint(x: p2.x - (p3.x - p1.x) * t,
                              y: p2.y - (p3.y - p1.y) * t)
            path.curve(to: p2, controlPoint1: cp1, controlPoint2: cp2)
        }
        return path
    }
}
