import AppKit
import Carbon.HIToolbox

class AppDelegate: NSObject, NSApplicationDelegate {
    private static let bundleIdentifier = "com.kevinmazille.DemoInk"

    private var statusItem: NSStatusItem!
    private var overlayWindow: OverlayWindow?
    private var hotkeyManager: HotkeyManager?
    private var permissionsHotkey: HotkeyManager?

    func applicationDidFinishLaunching(_ notification: Notification) {
        SingleInstance.ensureOnlyInstance(bundleIdentifier: Self.bundleIdentifier)

        NSApp.setActivationPolicy(.accessory)

        setUpStatusItem()
        hotkeyManager = HotkeyManager(
            keyCode: UInt32(kVK_ANSI_D),
            modifiers: UInt32(cmdKey | shiftKey),
            signature: "DINK"
        ) { [weak self] in
            self?.toggleOverlay()
        }

        // A fixed shortcut to the permissions panel so the grant flow is always
        // reachable and never lost in System Settings.
        permissionsHotkey = HotkeyManager(
            keyCode: UInt32(kVK_ANSI_P),
            modifiers: UInt32(cmdKey | shiftKey),
            signature: "DPRM"
        ) {
            PermissionsWindowController.shared.show()
        }
    }

    private func setUpStatusItem() {
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        if let button = statusItem.button {
            // Prefer an SF Symbol (renders reliably as a template image); fall
            // back to a text glyph if the symbol is unavailable.
            if let image = NSImage(systemSymbolName: "pencil.tip", accessibilityDescription: "DemoInk") {
                image.isTemplate = true
                button.image = image
            } else {
                button.title = "DemoInk"
            }
        }

        let menu = NSMenu()
        menu.addItem(NSMenuItem(title: "Toggle Draw Mode (⌘⇧D)", action: #selector(toggleOverlay), keyEquivalent: ""))
        menu.addItem(NSMenuItem(title: "Permissions… (⌘⇧P)", action: #selector(showPermissions), keyEquivalent: ""))
        menu.addItem(.separator())
        menu.addItem(NSMenuItem(title: "Quit", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q"))
        statusItem.menu = menu
    }

    @objc private func showPermissions() {
        PermissionsWindowController.shared.show()
    }

    @objc private func toggleOverlay() {
        if let overlayWindow {
            // Auto-save the annotated screen (no-op if nothing was drawn) while
            // the window is still on-screen — the desktop below it must be
            // capturable for the Transparent theme. Only then close it.
            let view = overlayWindow.contentView as? OverlayView
            self.overlayWindow = nil
            let closeIt: () -> Void = { overlayWindow.close() }
            if let view {
                view.saveScreenshotIfNeeded(completion: closeIt)
            } else {
                closeIt()
            }
        } else {
            guard let screen = screenUnderMouse() else { return }
            let window = OverlayWindow(screen: screen)
            (window.contentView as? OverlayView)?.onExit = { [weak self] in
                self?.toggleOverlay()
            }
            NSApp.activate(ignoringOtherApps: true)
            window.makeKeyAndOrderFront(nil)
            overlayWindow = window
        }
    }

    /// The screen the pointer is currently on, so the overlay opens where the
    /// user is working (multi-monitor) rather than always on the main display.
    /// Falls back to the main screen if none matches.
    private func screenUnderMouse() -> NSScreen? {
        let mouse = NSEvent.mouseLocation
        return NSScreen.screens.first { NSMouseInRect(mouse, $0.frame, false) }
            ?? NSScreen.main
    }
}
