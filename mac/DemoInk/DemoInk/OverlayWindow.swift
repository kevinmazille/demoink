import AppKit

/// Full-screen transparent overlay. Borderless and click-through-capable, it
/// hosts the drawing surface. Overrides `canBecomeKey` so the view can receive
/// keyDown for the draw-mode shortcuts (undo, colors, width, erase, Esc).
final class OverlayWindow: NSWindow {
    convenience init(screen: NSScreen) {
        self.init(
            contentRect: screen.frame,
            styleMask: [.borderless, .fullSizeContentView],
            backing: .buffered,
            defer: false,
            screen: screen
        )

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
        contentView = OverlayView(frame: screen.frame)
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

    // Current tool state (frozen into each DrawLine at stroke start).
    private var colorIndex = DrawModel.defaultColorIndex
    private var penWidth = DrawModel.defaultPenWidth
    private var theme: Theme = .transparent
    private var alpha = DrawModel.lineAlpha

    override var isFlipped: Bool { true } // top-left origin, like Windows

    override var acceptsFirstResponder: Bool { true }

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        if window != nil {
            window?.makeFirstResponder(self)
        } else if cursorPoint != nil {
            // Overlay closed while the cursor was hidden — restore it so the
            // system arrow doesn't stay hidden everywhere.
            NSCursor.unhide()
            cursorPoint = nil
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
        cursorPoint = convert(event.locationInWindow, from: nil)
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
        guard let p = cursorPoint else { return }
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
        beginStroke(at: convert(event.locationInWindow, from: nil))
    }

    override func rightMouseDown(with event: NSEvent) {
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
        if event.deltaY > 0 {
            penWidth = min(penWidth + 1, DrawModel.maxPenWidth)
        } else if event.deltaY < 0 {
            penWidth = max(penWidth - 1, DrawModel.minPenWidth)
        }
        refreshCursor()
    }

    // MARK: - Keyboard

    override func keyDown(with event: NSEvent) {
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
        switch chars.lowercased() {
        case "w": // erase all
            lines.removeAll()
            needsDisplay = true
        case "q": // cycle theme (Transparent → Light ↔ Dark)
            toggleTheme()
        default:
            super.keyDown(with: event)
        }
    }

    // MARK: - Theme

    /// Q key. First press from the pristine Transparent state wipes annotations
    /// and starts fresh on the Light canvas; afterwards Q toggles Light ↔ Dark
    /// and preserves the drawings, re-alpha'ing them to the new theme. Mirrors
    /// ID_CMD_TOGGLETHEME in Commands.cpp.
    private func toggleTheme() {
        if theme == .transparent {
            theme = .light
            isDrawing = false
            lines.removeAll()
        } else {
            theme = (theme == .light) ? .dark : .light
        }
        // Match the ink alpha to the new theme (opaque on Dark).
        alpha = DrawModel.alpha(for: theme)
        for i in lines.indices {
            lines[i].alpha = alpha
        }
        refreshCursor()
        needsDisplay = true
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
            }
        }

        drawCursorIndicator()
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
