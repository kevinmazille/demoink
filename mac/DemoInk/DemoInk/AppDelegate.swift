import AppKit
import Carbon.HIToolbox

class AppDelegate: NSObject, NSApplicationDelegate {
    private static let bundleIdentifier = "com.kevinmazille.DemoInk"

    private var statusItem: NSStatusItem!
    private var overlayWindow: OverlayWindow?
    private var hotkeyManager: HotkeyManager?

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
        menu.addItem(.separator())
        menu.addItem(NSMenuItem(title: "Quit", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q"))
        statusItem.menu = menu
    }

    @objc private func toggleOverlay() {
        if let overlayWindow {
            overlayWindow.close()
            self.overlayWindow = nil
        } else {
            guard let screen = NSScreen.main else { return }
            let window = OverlayWindow(screen: screen)
            (window.contentView as? OverlayView)?.onExit = { [weak self] in
                self?.toggleOverlay()
            }
            NSApp.activate(ignoringOtherApps: true)
            window.makeKeyAndOrderFront(nil)
            overlayWindow = window
        }
    }
}
