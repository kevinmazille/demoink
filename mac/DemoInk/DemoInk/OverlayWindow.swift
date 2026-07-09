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
    private var alpha = DrawModel.lineAlpha

    override var isFlipped: Bool { true } // top-left origin, like Windows

    override var acceptsFirstResponder: Bool { true }

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        window?.makeFirstResponder(self)
    }

    // MARK: - Mouse

    override func mouseDown(with event: NSEvent) {
        let p = convert(event.locationInWindow, from: nil)
        var line = DrawLine()
        line.colorIndex = colorIndex
        line.penWidth = penWidth
        line.alpha = alpha
        line.points = [p]
        lines.append(line)
        isDrawing = true
        needsDisplay = true
    }

    override func mouseDragged(with event: NSEvent) {
        guard isDrawing, !lines.isEmpty else { return }
        let p = convert(event.locationInWindow, from: nil)
        // Skip sub-pixel jitter, matching the Windows >1px gate.
        if let last = lines[lines.count - 1].points.last,
           hypot(p.x - last.x, p.y - last.y) <= 1 {
            return
        }
        lines[lines.count - 1].points.append(p)
        needsDisplay = true
    }

    override func mouseUp(with event: NSEvent) {
        isDrawing = false
    }

    // MARK: - Scroll wheel = pen width (matches Windows wheel-resizes)

    override func scrollWheel(with event: NSEvent) {
        if event.deltaY > 0 {
            penWidth = min(penWidth + 1, DrawModel.maxPenWidth)
        } else if event.deltaY < 0 {
            penWidth = max(penWidth - 1, DrawModel.minPenWidth)
        }
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
            return
        case 125: // Down arrow — thinner
            penWidth = max(penWidth - 1, DrawModel.minPenWidth)
            return
        case 123: // Left arrow — previous color
            colorIndex = (colorIndex + 9) % 10
            return
        case 124: // Right arrow — next color
            colorIndex = (colorIndex + 1) % 10
            return
        default:
            break
        }

        guard let chars = event.charactersIgnoringModifiers else { return }
        if let digit = Int(chars), digit >= 0, digit <= 9 {
            colorIndex = digit
            return
        }
        switch chars.lowercased() {
        case "w": // erase all
            lines.removeAll()
            needsDisplay = true
        default:
            super.keyDown(with: event)
        }
    }

    // MARK: - Rendering

    override func draw(_ dirtyRect: NSRect) {
        super.draw(dirtyRect)
        guard let ctx = NSGraphicsContext.current else { return }
        ctx.imageInterpolation = .high

        for line in lines {
            let color = DrawModel.color(atIndex: line.colorIndex, alpha: line.alpha)
            color.setStroke()
            color.setFill()

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
                path.lineWidth = line.penWidth
                path.lineCapStyle = .round
                path.lineJoinStyle = .round
                path.stroke()
            }
        }
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
