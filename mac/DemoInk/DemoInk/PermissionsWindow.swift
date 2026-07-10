import AppKit
import CoreGraphics

/// A small always-reachable panel for the one macOS permission DemoInk needs —
/// **Screen Recording** (auto-screenshot desktop capture) — with one-click
/// buttons to prompt for it or jump straight to the right System Settings pane,
/// so the grant flow never gets lost in the depths of System Settings.
///
/// It intentionally shows no live "granted/not granted" indicator: the only
/// reliable API for that (`CGPreflightScreenCaptureAccess`) is cached
/// per-process and keeps reporting the pre-launch state, which is more confusing
/// than helpful. The user grants it in System Settings and relaunches; the
/// authoritative status lives there. (The global hotkey uses `RegisterEventHotKey`,
/// which needs no Accessibility grant, so that permission isn't listed here.)
final class PermissionsWindowController {
    static let shared = PermissionsWindowController()

    private var window: NSPanel?

    /// Shows the panel (creating it on first use) and brings it to the front.
    func show() {
        if window == nil { buildWindow() }
        NSApp.activate(ignoringOtherApps: true)
        window?.center()
        window?.makeKeyAndOrderFront(nil)
    }

    // MARK: - Build

    private func buildWindow() {
        let panel = NSPanel(
            contentRect: NSRect(x: 0, y: 0, width: 480, height: 210),
            styleMask: [.titled, .closable, .utilityWindow],
            backing: .buffered,
            defer: false
        )
        panel.title = "DemoInk — Permissions"
        panel.isReleasedWhenClosed = false
        panel.hidesOnDeactivate = false

        let stack = NSStackView()
        stack.orientation = .vertical
        stack.alignment = .leading
        stack.spacing = 16
        stack.edgeInsets = NSEdgeInsets(top: 20, left: 20, bottom: 20, right: 20)
        stack.translatesAutoresizingMaskIntoConstraints = false

        let title = NSTextField(labelWithString: "Screen Recording")
        title.font = .systemFont(ofSize: 13, weight: .semibold)
        stack.addArrangedSubview(title)

        let intro = NSTextField(wrappingLabelWithString:
            "DemoInk needs Screen Recording to capture the desktop behind your "
            + "annotations when auto-saving screenshots.\n\n"
            + "1. Click Open Settings and enable DemoInk under Screen Recording.\n"
            + "2. Then Quit & Relaunch DemoInk for the change to take effect.")
        intro.font = .systemFont(ofSize: 12)
        intro.textColor = .secondaryLabelColor
        stack.addArrangedSubview(intro)
        intro.widthAnchor.constraint(equalToConstant: 440).isActive = true

        let settingsBtn = NSButton(title: "Open Settings", target: self,
                                   action: #selector(openScreenRecordingSettings))
        settingsBtn.bezelStyle = .rounded
        let relaunchBtn = NSButton(title: "Quit & Relaunch DemoInk", target: self,
                                   action: #selector(relaunchApp))
        relaunchBtn.bezelStyle = .rounded

        let buttons = NSStackView(views: [settingsBtn, relaunchBtn])
        buttons.orientation = .horizontal
        buttons.spacing = 10
        stack.addArrangedSubview(buttons)

        panel.contentView?.addSubview(stack)
        if let cv = panel.contentView {
            NSLayoutConstraint.activate([
                stack.leadingAnchor.constraint(equalTo: cv.leadingAnchor),
                stack.trailingAnchor.constraint(equalTo: cv.trailingAnchor),
                stack.topAnchor.constraint(equalTo: cv.topAnchor),
                stack.bottomAnchor.constraint(greaterThanOrEqualTo: cv.bottomAnchor, constant: -20),
            ])
        }
        window = panel
    }

    // MARK: - Actions

    @objc private func openScreenRecordingSettings() {
        // Prompt first so DemoInk is registered in the list, then deep-link
        // straight to the Screen Recording pane.
        if !CGPreflightScreenCaptureAccess() {
            _ = CGRequestScreenCaptureAccess()
        }
        if let url = URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_ScreenCapture") {
            NSWorkspace.shared.open(url)
        }
    }

    @objc private func relaunchApp() {
        let path = Bundle.main.bundlePath
        // Detached shell that waits for us to fully exit before relaunching —
        // otherwise the new instance's single-instance check would see the old
        // one still alive and immediately quit itself. `open -n` then starts a
        // fresh instance once we're gone.
        let pid = ProcessInfo.processInfo.processIdentifier
        let script = "while kill -0 \(pid) 2>/dev/null; do sleep 0.1; done; open -n \"\(path)\""
        let task = Process()
        task.launchPath = "/bin/sh"
        task.arguments = ["-c", script]
        try? task.run()
        NSApp.terminate(nil)
    }
}
